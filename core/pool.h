#pragma once

#include <cstddef>
#include <memory>

namespace hive::core {

// Fixed-size block allocator. Not thread-safe.
// All blocks share one backing store allocated in the constructor.
// deallocate() returns a block to an intrusive free list (O(1)).
class Pool {
public:
    // block_size is the user-facing payload size. Internally the stride may
    // be larger so each slot can hold a free-list pointer and stay aligned.
    Pool(std::size_t block_size, std::size_t block_count);
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;
    Pool(Pool&&) = delete;
    Pool& operator=(Pool&&) = delete;
    ~Pool();

    // Returns nullptr if no free block remains.
    [[nodiscard]] void* allocate();

    // ptr must come from allocate() on this pool (HIVE_ASSERT in debug).
    void deallocate(void* ptr) noexcept;

    [[nodiscard]] std::size_t block_size() const noexcept;
    [[nodiscard]] std::size_t allocated_count() const noexcept;

private:
    struct FreeNode {
        FreeNode* next;
    };

    std::unique_ptr<std::byte[]> m_buffer;
    FreeNode* m_free_head = nullptr;
    std::size_t m_block_size = 0;
    std::size_t m_block_count = 0;
    std::size_t m_stride = 0;
    std::size_t m_allocated = 0;
};

} // namespace hive::core
