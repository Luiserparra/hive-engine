#include "core/arena.h"
#include "core/assert.h"

namespace {

std::size_t align_up(std::size_t value, std::size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

} // namespace

namespace hive::core {

Arena::Arena(std::size_t capacity) {
    m_capacity = capacity;
    m_offset = 0;
    m_buffer = std::make_unique<std::byte[]>(capacity);
}

Arena::Arena(Arena&& other) noexcept
    : m_buffer(std::move(other.m_buffer))
    , m_capacity(other.m_capacity)
    , m_offset(other.m_offset) {
    other.m_capacity = 0;
    other.m_offset = 0;
}
Arena& Arena::operator=(Arena&& other) noexcept {
    if (this != &other) {
        m_buffer = std::move(other.m_buffer);
        m_capacity = other.m_capacity;
        m_offset = other.m_offset;
        other.m_capacity = 0;
        other.m_offset = 0;
    }
    return *this;
}
Arena::~Arena() = default;

void* Arena::allocate(std::size_t size, std::size_t alignment) {
    if (size == 0) { return nullptr; }
    HIVE_ASSERT(alignment != 0 && (alignment & (alignment - 1)) == 0);
    const std::size_t aligned = align_up(m_offset, alignment);
    if (aligned > m_capacity || size > m_capacity - aligned) {
        return nullptr; 
    }
    m_offset = aligned + size;
    return m_buffer.get() + aligned;
}

void Arena::reset() noexcept {
    m_offset = 0;
}

std::size_t Arena::used() const noexcept {
    return m_offset;
}

std::size_t Arena::capacity() const noexcept {
    return m_capacity;
}

} // namespace hive::core
