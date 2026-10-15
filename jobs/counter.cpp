#include "jobs/counter.h"

#include "core/assert.h"

namespace hive::jobs {

void Counter::add(int n) noexcept {
    HIVE_ASSERT(n >= 0);
    if (n == 0) {
        return;
    }
    // Release: a following push (seq_cst fence) and wait()'s acquire load must
    // observe this increment. Without it wait() could see 0 and return early.
    m_value.fetch_add(n, std::memory_order_release);
}

int Counter::load() const noexcept {
    // Acquire: pairs with decrement's acq_rel and add's release.
    return m_value.load(std::memory_order_acquire);
}

void Counter::decrement() noexcept {
    // acq_rel: publish "this job is done" to wait(), and observe the matching add.
    [[maybe_unused]] const int prev = m_value.fetch_sub(1, std::memory_order_acq_rel);
    HIVE_ASSERT(prev > 0);
}

} // namespace hive::jobs
