#pragma once

#include "core/cache_line.h"

#include <atomic>

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

    alignas(hive::core::kCacheLineBytes) std::atomic<int> m_value{0};
};

} // namespace hive::jobs
