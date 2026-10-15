#pragma once

#include "core/assert.h"
#include "jobs/counter.h"
#include "jobs/job.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace hive::jobs {

namespace detail {
inline thread_local class Scheduler* t_scheduler = nullptr;
inline thread_local Job* t_current_job = nullptr;
} // namespace detail

// Work-stealing pool. The constructing thread is worker 0 (no extra thread).
// Scheduler(n) spawns n-1 workers. wait() helps by running jobs instead of sleeping.
class Scheduler {
public:
    explicit Scheduler(unsigned worker_count);
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    Scheduler(Scheduler&&) = delete;
    Scheduler& operator=(Scheduler&&) = delete;
    ~Scheduler();

    template <typename F> void schedule(F&& fn, Counter* counter = nullptr);

    template <typename F>
    void parallel_for(std::size_t begin, std::size_t end, std::size_t grain, F fn,
                      Counter* done = nullptr);

    void wait(Counter& counter);

    [[nodiscard]] unsigned worker_count() const noexcept;

private:
    struct Worker;

    template <typename F>
    void split_and_run(std::size_t begin, std::size_t end, std::size_t grain, const F& fn,
                       Counter* done);

    Job* allocate_job();
    void submit(Job* job) noexcept;
    void execute_job(Job* job);
    bool try_run_one(Worker& worker);
    void worker_main(unsigned index);
    void notify_idle() noexcept;
    void park_idle();
    [[nodiscard]] bool has_any_work() const noexcept;

    // thread_local, not an instance field — t_ prefix matches detail::t_scheduler.
    // NOLINTNEXTLINE(readability-identifier-naming)
    static thread_local Worker* t_worker;

    std::vector<std::unique_ptr<Worker>> m_workers;
    unsigned m_worker_count = 0;
    std::atomic<bool> m_running{false};
    std::atomic<int> m_idle_count{0};
    std::mutex m_idle_mutex;
    std::condition_variable m_idle_cv;
};

template <typename F> void Scheduler::schedule(F&& fn, Counter* counter) {
    using Fn = std::decay_t<F>;
    static_assert(std::is_invocable_r_v<void, Fn>, "job lambda must be void()");
    static_assert(sizeof(Fn) <= Job::kPayloadBytes,
                  "job lambda is too large to fit in the 64-byte Job payload");
    static_assert(alignof(Fn) <= Job::kPayloadAlign,
                  "job lambda alignment exceeds payload alignment");

    Job* job = allocate_job();
    HIVE_ASSERT(job != nullptr);
    job->counter = counter;
    job->execute = [](Job* j) {
        Fn& callable = *std::launder(reinterpret_cast<Fn*>(j->payload));
        callable();
        callable.~Fn();
    };
    ::new (static_cast<void*>(job->payload)) Fn(std::forward<F>(fn));
    if (counter != nullptr) {
        counter->add(1);
    }
    submit(job);
}

template <typename F>
void Scheduler::parallel_for(std::size_t begin, std::size_t end, std::size_t grain, F fn,
                             Counter* done) {
    if (begin >= end) {
        return;
    }
    if (grain == 0) {
        grain = 1;
    }
    split_and_run(begin, end, grain, fn, done);
}

template <typename F>
void Scheduler::split_and_run(std::size_t begin, std::size_t end, std::size_t grain, const F& fn,
                              Counter* done) {
    while (end - begin > grain) {
        const std::size_t mid = begin + (end - begin) / 2;
        schedule(
            [mid, end, grain, fn]() {
                Scheduler* sched = detail::t_scheduler;
                HIVE_ASSERT(sched != nullptr);
                Counter* nested =
                    detail::t_current_job != nullptr ? detail::t_current_job->counter : nullptr;
                sched->split_and_run(mid, end, grain, fn, nested);
            },
            done);
        end = mid;
    }
    for (std::size_t i = begin; i < end; ++i) {
        fn(i);
    }
}

} // namespace hive::jobs
