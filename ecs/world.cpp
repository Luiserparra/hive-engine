#include "ecs/world.h"

#include "core/assert.h"

#include <cstring>
#include <utility>

namespace hive::ecs {

World::World() {
    m_records.emplace_back();
    m_archetypes.push_back(std::make_unique<Archetype>(
        0, ComponentMask{0}, std::vector<std::uint32_t>{}, &m_free_chunks));
    m_mask_to_index.emplace(ComponentMask{0}, 0);
}

Entity World::create() {
    const Entity entity = reserve();
    spawn(entity);
    return entity;
}

void World::destroy(Entity entity) {
    if (!alive(entity)) {
        return;
    }
    EntityRecord& rec = record(entity);
    m_archetypes[rec.archetype]->remove_row(rec.chunk, rec.row, m_records);
    rec.generation += 1;
    rec.archetype = kInvalidArchetype;
    rec.chunk = 0;
    rec.row = 0;
    m_free.push_back(entity.index);
}

Entity World::reserve() {
    std::uint32_t index = 0;
    if (!m_free.empty()) {
        index = m_free.back();
        m_free.pop_back();
    } else {
        index = static_cast<std::uint32_t>(m_records.size());
        EntityRecord rec;
        rec.generation = 1;
        rec.archetype = kInvalidArchetype;
        m_records.push_back(rec);
    }
    EntityRecord& rec = m_records[index];
    rec.archetype = kInvalidArchetype;
    rec.chunk = 0;
    rec.row = 0;
    return Entity{index, rec.generation};
}

void World::spawn(Entity entity) {
    HIVE_ASSERT(entity.index > 0 && entity.index < m_records.size());
    EntityRecord& rec = m_records[entity.index];
    HIVE_ASSERT(rec.generation == entity.generation);
    HIVE_ASSERT(rec.archetype == kInvalidArchetype);

    Archetype& arch = *m_archetypes[0];
    const ChunkRef slot = arch.writable();
    const std::uint32_t row = slot.chunk->count();
    slot.chunk->entities(arch)[row] = entity;
    slot.chunk->set_count(row + 1);

    rec.archetype = 0;
    rec.chunk = slot.index;
    rec.row = row;
}

bool World::alive(Entity entity) const noexcept {
    if (entity.index == 0 || entity.index >= m_records.size()) {
        return false;
    }
    const EntityRecord& rec = m_records[entity.index];
    return rec.generation == entity.generation && rec.archetype != kInvalidArchetype;
}

void World::add_erased(Entity entity, std::uint32_t type_id, const void* bytes) {
    HIVE_ASSERT(alive(entity));
    HIVE_ASSERT(bytes != nullptr);
    EntityRecord& rec = record(entity);
    Archetype& src = *m_archetypes[rec.archetype];
    HIVE_ASSERT(!src.has_type(type_id));
    const ComponentMask new_mask = src.mask() | (ComponentMask{1} << type_id);
    migrate(entity, new_mask, type_id, bytes);
}

void World::remove_erased(Entity entity, std::uint32_t type_id) {
    HIVE_ASSERT(alive(entity));
    EntityRecord& rec = record(entity);
    Archetype& src = *m_archetypes[rec.archetype];
    HIVE_ASSERT(src.has_type(type_id));
    const ComponentMask new_mask = src.mask() & ~(ComponentMask{1} << type_id);
    migrate(entity, new_mask, type_id, nullptr);
}

std::vector<std::unique_ptr<Archetype>>& World::archetypes() noexcept {
    return m_archetypes;
}

const std::vector<std::unique_ptr<Archetype>>& World::archetypes() const noexcept {
    return m_archetypes;
}

void World::migrate(Entity entity, ComponentMask new_mask, std::uint32_t changed_id,
                    const void* added_bytes) {
    EntityRecord& rec = record(entity);
    const std::uint32_t src_arch_index = rec.archetype;
    const std::uint32_t src_chunk_index = rec.chunk;
    const std::uint32_t src_row = rec.row;

    Archetype& src = *m_archetypes[src_arch_index];
    Archetype& dst = get_or_create(new_mask);
    Chunk& src_chunk = *src.chunk_at(src_chunk_index);
    const ChunkRef dst_slot = dst.writable();
    Chunk& dst_chunk = *dst_slot.chunk;
    const std::uint32_t dst_row = dst_chunk.count();

    dst_chunk.entities(dst)[dst_row] = entity;
    for (const std::uint32_t type_id : dst.type_ids()) {
        const TypeInfo info = TypeId::info(type_id);
        std::byte* dest =
            dst_chunk.column_bytes(dst, type_id) + static_cast<std::size_t>(dst_row) * info.size;
        if (type_id == changed_id && added_bytes != nullptr) {
            std::memcpy(dest, added_bytes, info.size);
        } else {
            std::byte* from = src_chunk.column_bytes(src, type_id) +
                              static_cast<std::size_t>(src_row) * info.size;
            std::memcpy(dest, from, info.size);
        }
    }
    dst_chunk.set_count(dst_row + 1);

    rec.archetype = dst.index();
    rec.chunk = dst_slot.index;
    rec.row = dst_row;

    src.remove_row(src_chunk_index, src_row, m_records);
}

Archetype& World::get_or_create(ComponentMask mask) {
    const auto it = m_mask_to_index.find(mask);
    if (it != m_mask_to_index.end()) {
        return *m_archetypes[it->second];
    }
    std::vector<std::uint32_t> ids;
    for (std::uint32_t i = 0; i < kMaxComponents; ++i) {
        if ((mask & (ComponentMask{1} << i)) != 0) {
            ids.push_back(i);
        }
    }
    const std::uint32_t index = static_cast<std::uint32_t>(m_archetypes.size());
    m_archetypes.push_back(
        std::make_unique<Archetype>(index, mask, std::move(ids), &m_free_chunks));
    m_mask_to_index.emplace(mask, index);
    return *m_archetypes[index];
}

EntityRecord& World::record(Entity entity) noexcept {
    HIVE_ASSERT(entity.index > 0 && entity.index < m_records.size());
    return m_records[entity.index];
}

const EntityRecord& World::record(Entity entity) const noexcept {
    HIVE_ASSERT(entity.index > 0 && entity.index < m_records.size());
    return m_records[entity.index];
}

} // namespace hive::ecs
