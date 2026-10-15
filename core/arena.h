#pragma once

#include <cstddef>
#include <memory>

namespace hive::core {

// Bump / linear allocator. Not thread-safe.
// allocate() never calls the OS; it only advances an offset into a buffer
// reserved in the constructor. There is no per-object free — call reset()
// to rewind (typical use: once per frame).
class Arena {
public:
    explicit Arena(std::size_t capacity);
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena(Arena&&) noexcept;
    Arena& operator=(Arena&&) noexcept;
    ~Arena();

    // Returns nullptr if size == 0 or if the aligned allocation does not fit.
    // alignment must be a power of 2.
    [[nodiscard]] void* allocate(std::size_t size,
                                 std::size_t alignment = alignof(std::max_align_t));

    // used() = 0. Does not release the backing store.
    void reset() noexcept;

    [[nodiscard]] std::size_t used() const noexcept;
    [[nodiscard]] std::size_t capacity() const noexcept;

private:
    std::unique_ptr<std::byte[]> m_buffer;
    std::size_t m_capacity = 0;
    std::size_t m_offset = 0;
};

} // namespace hive::core
