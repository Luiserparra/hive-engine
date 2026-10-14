#include "ecs/chunk.h"

#include "ecs/archetype.h"

#include "core/assert.h"

namespace hive::ecs {

Chunk::Chunk() : m_storage(std::make_unique<std::byte[]>(kChunkBytes)) {}

std::uint32_t Chunk::count() const noexcept {
    return m_count;
}

std::uint32_t Chunk::capacity() const noexcept {
    return m_capacity;
}

std::uint32_t Chunk::archetype_index() const noexcept {
    return m_archetype;
}

void Chunk::reset(std::uint32_t capacity, std::uint32_t archetype_index) noexcept {
    HIVE_ASSERT(capacity > 0);
    m_count = 0;
    m_capacity = capacity;
    m_archetype = archetype_index;
}

void Chunk::set_count(std::uint32_t count) noexcept {
    HIVE_ASSERT(count <= m_capacity);
    m_count = count;
}

Entity* Chunk::entities(const Archetype& archetype) noexcept {
    return reinterpret_cast<Entity*>(m_storage.get() + archetype.entity_offset());
}

std::byte* Chunk::column_bytes(const Archetype& archetype, std::uint32_t type_id) noexcept {
    return m_storage.get() + archetype.column_offset(type_id);
}

} // namespace hive::ecs
