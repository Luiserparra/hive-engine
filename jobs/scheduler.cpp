#include "jobs/scheduler.h"

#include "core/cache_line.h"
#include "jobs/deque.h"
#include "profiler/profiler.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace hive::jobs {
namespace {

constexpr std::size_t kJobPoolSize = WorkStealingDeque::kCapacity;
constexpr unsigned kYieldsBeforePark = 64;

} // namespace

thread_local Scheduler::Worker* Scheduler::t_worker = nullptr;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324) // Worker is padded so adjacent workers do not false-share
#endif

struct alignas(hive::core::kCacheLineBytes) Scheduler::Worker {
    explicit Worker(unsigned idx) : m_index(idx) {
        m_jobs = std::make_unique<Job[]>(kJobPoolSize);
        Job* head = nullptr;
        for (std::size_t i = kJobPoolSize; i-- > 0;) {
            m_jobs[i].owner = static_cast<std::uint16_t>(idx);
            m_jobs[i].next_free = head;
            head = &m_jobs[i];
        }
        m_free_head.store(head, std::memory_order_relaxed);
    }

    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    [[nodiscard]] Job* pop_free() noexcept {
        Job* head = m_free_head.load(std::memory_order_acquire);
        while (head != nullptr) {
            Job* next = head->next_free;
            // Acquire on success: the job's previous owner published the recycle
            // with release. Failure: reload head (updated by CAS).
            if (m_free_head.compare_exchange_weak(head, next, std::memory_order_acquire,
                                                  std::memory_order_acquire)) {
                return head;
            }
        }
        return nullptr;
    }

    void push_free(Job* job) noexcept {
        HIVE_ASSERT(job != nullptr);
        Job* head = m_free_head.load(std::memory_order_relaxed);
        do {
            job->next_free = head;
            // Release: the job's payload destructor has finished; a subsequent
            // acquire pop_free may reuse the slot.
        } while (!m_free_head.compare_exchange_weak(head, job, std::memory_order_release,
                                                    std::memory_order_relaxed));
    }

    WorkStealingDeque m_deque;
    std::unique_ptr<Job[]> m_jobs;
    std::atomic<Job*> m_free_head{nullptr};
    std::thread m_thread;
    unsigned m_index = 0;
    unsigned m_steal_cursor = 0;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

Scheduler::Scheduler(unsigned worker_count) {
    if (worker_count == 0) {
        worker_count = 1;
    }
    m_worker_count = worker_count;
    m_workers.reserve(worker_count);
    for (unsigned i = 0; i < worker_count; ++i) {
        m_workers.push_back(std::make_unique<Worker>(i));
    }

    m_running.store(true, std::memory_order_relaxed);
    detail::t_scheduler = this;
    t_worker = m_workers[0].get();

    for (unsigned i = 1; i < worker_count; ++i) {
        m_workers[i]->m_thread = std::thread([this, i] {
            worker_main(i);
        });
    }
}

Scheduler::~Scheduler() {
    m_running.store(false, std::memory_order_release);
    m_idle_cv.notify_all();
    for (unsigned i = 1; i < m_worker_count; ++i) {
        if (m_workers[i]->m_thread.joinable()) {
            m_workers[i]->m_thread.join();
        }
    }
    if (detail::t_scheduler == this) {
        detail::t_scheduler = nullptr;
        t_worker = nullptr;
    }
}

unsigned Scheduler::worker_count() const noexcept {
    return m_worker_count;
}

Job* Scheduler::allocate_job() {
    Worker* w = t_worker != nullptr ? t_worker : m_workers[0].get();
    for (;;) {
        if (Job* job = w->pop_free()) {
            job->owner = static_cast<std::uint16_t>(w->m_index);
            job->execute = nullptr;
            job->counter = nullptr;
            return job;
        }
        // Pool exhausted: run something so completed jobs recycle to this owner.
        if (!try_run_one(*w)) {
            std::this_thread::yield();
        }
    }
}

void Scheduler::submit(Job* job) noexcept {
    Worker* w = t_worker != nullptr ? t_worker : m_workers[0].get();
    w->m_deque.push(job);
    notify_idle();
}

void Scheduler::execute_job(Job* job) {
    HIVE_ASSERT(job != nullptr);
    HIVE_ASSERT(job->execute != nullptr);
    detail::t_current_job = job;
    job->execute(job);
    detail::t_current_job = nullptr;

    Counter* counter = job->counter;
    const unsigned owner = job->owner;
    job->execute = nullptr;
    job->counter = nullptr;
    HIVE_ASSERT(owner < m_worker_count);
    m_workers[owner]->push_free(job);
    if (counter != nullptr) {
        counter->decrement();
    }
}

bool Scheduler::try_run_one(Worker& worker) {
    if (void* p = worker.m_deque.pop()) {
        execute_job(static_cast<Job*>(p));
        return true;
    }
    const unsigned n = m_worker_count;
    if (n == 1) {
        return false;
    }
    unsigned start = worker.m_steal_cursor++ % n;
    for (unsigned k = 0; k < n; ++k) {
        const unsigned i = (start + k) % n;
        if (i == worker.m_index) {
            continue;
        }
        if (void* p = m_workers[i]->m_deque.steal()) {
            execute_job(static_cast<Job*>(p));
            return true;
        }
    }
    return false;
}

void Scheduler::wait(Counter& counter) {
    PROFILE_SCOPE("jobs.wait");
    Worker* w = t_worker != nullptr ? t_worker : m_workers[0].get();
    while (counter.load() > 0) {
        if (!try_run_one(*w)) {
            // Work is in flight on another thread, not stealable. Yield, do not park:
            // wait() must keep polling so it notices the counter hitting 0.
            std::this_thread::yield();
        }
    }
}

void Scheduler::worker_main(unsigned index) {
    detail::t_scheduler = this;
    t_worker = m_workers[index].get();
    hive::profiler::init();

    unsigned yields = 0;
    while (m_running.load(std::memory_order_acquire)) {
        if (try_run_one(*t_worker)) {
            yields = 0;
            continue;
        }
        if (yields < kYieldsBeforePark) {
            ++yields;
            std::this_thread::yield();
            continue;
        }
        park_idle();
        yields = 0;
    }

    hive::profiler::shutdown();
    detail::t_scheduler = nullptr;
    t_worker = nullptr;
}

void Scheduler::notify_idle() noexcept {
    // No mutex on the hot path. notify_one is safe without the lock; idle_count
    // is the handshake that avoids a pure lost wakeup.
    if (m_idle_count.load(std::memory_order_seq_cst) > 0) {
        m_idle_cv.notify_one();
    }
}

void Scheduler::park_idle() {
    m_idle_count.fetch_add(1, std::memory_order_seq_cst);
    if (has_any_work() || !m_running.load(std::memory_order_acquire)) {
        m_idle_count.fetch_sub(1, std::memory_order_seq_cst);
        return;
    }
    {
        // Mutex only to wait on the condvar (idle path, not push/pop/steal).
        std::unique_lock<std::mutex> lock(m_idle_mutex);
        m_idle_cv.wait(lock, [this] {
            return !m_running.load(std::memory_order_acquire) || has_any_work();
        });
    }
    m_idle_count.fetch_sub(1, std::memory_order_seq_cst);
}

bool Scheduler::has_any_work() const noexcept {
    for (unsigned i = 0; i < m_worker_count; ++i) {
        if (!m_workers[i]->m_deque.empty()) {
            return true;
        }
    }
    return false;
}

} // namespace hive::jobs
