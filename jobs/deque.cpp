#include "jobs/deque.h"

#include "core/assert.h"

#include <cstddef>

namespace hive::jobs {

WorkStealingDeque::WorkStealingDeque()
    : m_buffer(std::make_unique<std::atomic<void*>[]>(kCapacity)) {
    for (std::size_t i = 0; i < kCapacity; ++i) {
        m_buffer[i].store(nullptr, std::memory_order_relaxed);
    }
}

void WorkStealingDeque::push(void* item) noexcept {
    HIVE_ASSERT(item != nullptr);

    // Owner-only: relaxed load of bottom is enough (we are the only writer).
    const std::int64_t b = m_bottom.load(std::memory_order_relaxed);
    // Acquire: observe a thief's published top so b - t is not a stale underestimate
    // (which would miss a full deque).
    [[maybe_unused]] const std::int64_t t = m_top.load(std::memory_order_acquire);
    HIVE_ASSERT(b - t < static_cast<std::int64_t>(kCapacity));

    m_buffer[static_cast<std::size_t>(b) & kMask].store(item, std::memory_order_relaxed);
    // Seq_cst fence: the slot store must be visible to a thief before bottom is
    // published. Lê et al. — a release store of bottom is not enough on ARM.
    std::atomic_thread_fence(std::memory_order_seq_cst);
    m_bottom.store(b + 1, std::memory_order_relaxed);
}

void* WorkStealingDeque::pop() noexcept {
    // Decrement bottom first so a concurrent steal sees a smaller window.
    std::int64_t b = m_bottom.load(std::memory_order_relaxed) - 1;
    m_bottom.store(b, std::memory_order_relaxed);
    // Seq_cst fence: this bottom store must race correctly with steal's load of
    // bottom / CAS of top (Lê et al. take()).
    std::atomic_thread_fence(std::memory_order_seq_cst);
    std::int64_t t = m_top.load(std::memory_order_relaxed);

    if (t <= b) {
        void* item = m_buffer[static_cast<std::size_t>(b) & kMask].load(std::memory_order_relaxed);
        if (t == b) {
            // Last item: a thief may be stealing the same slot. Winner of the CAS
            // owns it. Restore bottom to empty either way.
            if (!m_top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst,
                                               std::memory_order_relaxed)) {
                item = nullptr;
            }
            m_bottom.store(b + 1, std::memory_order_relaxed);
        }
        return item;
    }

    // Was empty. Restore bottom so push has a consistent empty state (t == b+1
    // after the decrement, we set bottom back to t).
    m_bottom.store(b + 1, std::memory_order_relaxed);
    return nullptr;
}

void* WorkStealingDeque::steal() noexcept {
    std::int64_t t = m_top.load(std::memory_order_acquire);
    // Seq_cst fence: pair with owner's fence in push/pop so we observe either
    // the published item or a consistent empty.
    std::atomic_thread_fence(std::memory_order_seq_cst);
    const std::int64_t b = m_bottom.load(std::memory_order_acquire);
    if (t < b) {
        void* item = m_buffer[static_cast<std::size_t>(t) & kMask].load(std::memory_order_relaxed);
        // Seq_cst CAS: claim index t. Failure means another thief or the owner
        // pop took it — return nullptr and let the caller retry.
        if (!m_top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst,
                                           std::memory_order_relaxed)) {
            return nullptr;
        }
        return item;
    }
    return nullptr;
}

bool WorkStealingDeque::empty() const noexcept {
    const std::int64_t t = m_top.load(std::memory_order_relaxed);
    const std::int64_t b = m_bottom.load(std::memory_order_relaxed);
    return t >= b;
}

} // namespace hive::jobs
