#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

namespace hive::jobs {

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324) // padding from alignas splits top/bottom cache lines
#endif

// Chase-Lev work-stealing deque, Lê et al. 2013 (weak-memory-correct).
// Owner thread: push / pop at bottom. Any thief: steal at top.
// Fixed power-of-two capacity — push when full is a HIVE_ASSERT, never grows.
class WorkStealingDeque {
public:
    static constexpr std::size_t kCapacity = 4096;

    WorkStealingDeque();
    WorkStealingDeque(const WorkStealingDeque&) = delete;
    WorkStealingDeque& operator=(const WorkStealingDeque&) = delete;
    WorkStealingDeque(WorkStealingDeque&&) = delete;
    WorkStealingDeque& operator=(WorkStealingDeque&&) = delete;
    ~WorkStealingDeque() = default;

    // Owner only. item must not be nullptr (nullptr is "empty").
    void push(void* item) noexcept;

    // Owner only. nullptr if empty or if a thief won the last item.
    [[nodiscard]] void* pop() noexcept;

    // Any thread except the owner (or owner, harmlessly). nullptr if empty
    // or if this thief lost the CAS.
    [[nodiscard]] void* steal() noexcept;

    // Racy hint for idle parking. Not a substitute for pop/steal.
    [[nodiscard]] bool empty() const noexcept;

private:
    static constexpr std::size_t kMask = kCapacity - 1;
    static constexpr std::size_t kCacheLine = std::hardware_destructive_interference_size;

    // Thieves CAS m_top; the owner writes m_bottom. Separate cache lines so
    // a steal does not bounce the owner's bottom line (false sharing).
    alignas(kCacheLine) std::atomic<std::int64_t> m_top{0};
    alignas(kCacheLine) std::atomic<std::int64_t> m_bottom{0};
    std::unique_ptr<std::atomic<void*>[]> m_buffer;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace hive::jobs
