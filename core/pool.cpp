#include "core/pool.h"
#include "core/assert.h"

#include <algorithm>

namespace {

std::size_t align_up(std::size_t value, std::size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

} // namespace

namespace hive::core {

Pool::Pool(std::size_t block_size, std::size_t block_count) {
    m_block_size = block_size;
    m_block_count = block_count;
    const std::size_t raw = std::max(block_size, sizeof(void*));
    m_stride = align_up(raw, alignof(void*));
    m_buffer = std::make_unique<std::byte[]>(m_block_count * m_stride);
    m_free_head = nullptr;
    m_allocated = 0;

    for (std::size_t i = m_block_count; i-- > 0;) {
        auto* node = reinterpret_cast<FreeNode*>(m_buffer.get() + i * m_stride);
        node->next = m_free_head;
        m_free_head = node;
    }
}

Pool::~Pool() = default;

void* Pool::allocate() {
    if (m_free_head == nullptr) {
        return nullptr;
    }
    void* p = m_free_head;
    m_free_head = m_free_head->next;
    ++m_allocated;
    return p;
}

void Pool::deallocate(void* ptr) noexcept {
    HIVE_ASSERT(ptr != nullptr);
    auto* base = m_buffer.get();
    auto* bytes = static_cast<std::byte*>(ptr);
    HIVE_ASSERT(bytes >= base && bytes < base + m_block_count * m_stride);
    HIVE_ASSERT(m_stride != 0 && (bytes - base) % m_stride == 0);

    auto* node = static_cast<FreeNode*>(ptr);
    node->next = m_free_head;
    m_free_head = node;
    HIVE_ASSERT(m_allocated > 0);
    --m_allocated;
}

std::size_t Pool::block_size() const noexcept {
    return m_block_size;
}

std::size_t Pool::allocated_count() const noexcept {
    return m_allocated;
}

} // namespace hive::core
