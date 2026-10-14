#pragma once

#include <atomic>
#include <new>

namespace hive::jobs {

class Scheduler;

// Remaining-work counter. schedule / parallel_for add; job completion subtracts.
// wait() loads this until it hits 0. Padded so adjacent counters do not share a line.
class Counter {
public:
    void add(int n) noexcept;
    [[nodiscard]] int load() const noexcept;

private:
    friend class Scheduler;
    void decrement() noexcept;

    alignas(std::hardware_destructive_interference_size) std::atomic<int> m_value{0};
};

} // namespace hive::jobs
