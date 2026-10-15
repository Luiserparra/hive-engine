#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace hive::bench {

// Comparison-only mutex + queue pool. Not used by the engine.
// Persistent workers, caller thread helps. Same n-1 spawn model as Scheduler.
class MutexPool {
public:
    explicit MutexPool(unsigned thread_count) {
        if (thread_count == 0) {
            thread_count = 1;
        }
        m_thread_count = thread_count;
        m_stop.store(false, std::memory_order_relaxed);
        m_threads.reserve(thread_count > 0 ? thread_count - 1 : 0);
        for (unsigned i = 1; i < thread_count; ++i) {
            m_threads.emplace_back([this] {
                worker_loop();
            });
        }
    }

    MutexPool(const MutexPool&) = delete;
    MutexPool& operator=(const MutexPool&) = delete;
    MutexPool(MutexPool&&) = delete;
    MutexPool& operator=(MutexPool&&) = delete;

    ~MutexPool() {
        m_stop.store(true, std::memory_order_release);
        m_cv.notify_all();
        for (std::thread& t : m_threads) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    template <typename F>
    void parallel_for(std::size_t begin, std::size_t end, std::size_t grain, F fn) {
        if (begin >= end) {
            return;
        }
        if (grain == 0) {
            grain = 1;
        }

        auto trampoline = [](void* ctx, std::size_t b, std::size_t e) {
            F& callable = *static_cast<F*>(ctx);
            for (std::size_t i = b; i < e; ++i) {
                callable(i);
            }
        };

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (std::size_t b = begin; b < end; b += grain) {
                const std::size_t slice_end = std::min(b + grain, end);
                m_queue.push(Task{trampoline, &fn, b, slice_end});
                m_outstanding.fetch_add(1, std::memory_order_relaxed);
            }
        }
        m_cv.notify_all();
        help_until_idle();
    }

    [[nodiscard]] unsigned thread_count() const noexcept {
        return m_thread_count;
    }

private:
    struct Task {
        void (*run)(void*, std::size_t, std::size_t) = nullptr;
        void* ctx = nullptr;
        std::size_t begin = 0;
        std::size_t end = 0;
    };

    void worker_loop() {
        while (!m_stop.load(std::memory_order_acquire)) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this] {
                    return m_stop.load(std::memory_order_relaxed) || !m_queue.empty();
                });
                if (m_stop.load(std::memory_order_relaxed) && m_queue.empty()) {
                    return;
                }
                if (m_queue.empty()) {
                    continue;
                }
                task = m_queue.front();
                m_queue.pop();
            }
            task.run(task.ctx, task.begin, task.end);
            m_outstanding.fetch_sub(1, std::memory_order_acq_rel);
        }
    }

    void help_until_idle() {
        while (m_outstanding.load(std::memory_order_acquire) > 0) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                if (m_queue.empty()) {
                    lock.unlock();
                    std::this_thread::yield();
                    continue;
                }
                task = m_queue.front();
                m_queue.pop();
            }
            task.run(task.ctx, task.begin, task.end);
            m_outstanding.fetch_sub(1, std::memory_order_acq_rel);
        }
    }

    unsigned m_thread_count = 0;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<Task> m_queue;
    std::atomic<int> m_outstanding{0};
    std::atomic<bool> m_stop{false};
    std::vector<std::thread> m_threads;
};

} // namespace hive::bench
