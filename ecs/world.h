#pragma once

#include "ecs/archetype.h"
#include "ecs/entity.h"
#include "ecs/type.h"

#include "core/assert.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace hive::ecs {

template <typename... Cs> class Query;

class World {
public:
    World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = delete;
    World& operator=(World&&) = delete;
    ~World() = default;

    [[nodiscard]] Entity create();
    void destroy(Entity entity);

    // Allocate an id that is not alive and not in any archetype. Used by
    // CommandBuffer::create so subsequent add() ops in the same buffer can
    // name the entity before playback.
    [[nodiscard]] Entity reserve();
    void spawn(Entity entity);

    template <Component C> void add(Entity entity, const C& value);

    template <Component C> void remove(Entity entity);

    template <Component C> [[nodiscard]] bool has(Entity entity) const noexcept;

    template <Component C> [[nodiscard]] C* get(Entity entity) noexcept;

    [[nodiscard]] bool alive(Entity entity) const noexcept;

    template <typename... Cs> [[nodiscard]] Query<Cs...> query();

    void add_erased(Entity entity, std::uint32_t type_id, const void* bytes);
    void remove_erased(Entity entity, std::uint32_t type_id);

    // Public so Query can walk matching archetypes without a friend declaration.
    [[nodiscard]] std::vector<std::unique_ptr<Archetype>>& archetypes() noexcept;
    [[nodiscard]] const std::vector<std::unique_ptr<Archetype>>& archetypes() const noexcept;

private:
    void migrate(Entity entity, ComponentMask new_mask, std::uint32_t changed_id,
                 const void* added_bytes);
    [[nodiscard]] Archetype& get_or_create(ComponentMask mask);
    [[nodiscard]] EntityRecord& record(Entity entity) noexcept;
    [[nodiscard]] const EntityRecord& record(Entity entity) const noexcept;

    std::vector<EntityRecord> m_records;
    std::vector<std::uint32_t> m_free;
    std::vector<std::unique_ptr<Archetype>> m_archetypes;
    std::unordered_map<ComponentMask, std::uint32_t> m_mask_to_index;
    std::vector<std::unique_ptr<Chunk>> m_free_chunks;
};

} // namespace hive::ecs

#define HIVE_ECS_QUERY_INCLUDED_FROM_WORLD
#include "ecs/query.h"

namespace hive::ecs {

template <Component C> void World::add(Entity entity, const C& value) {
    add_erased(entity, TypeId::of<C>(), &value);
}

template <Component C> void World::remove(Entity entity) {
    remove_erased(entity, TypeId::of<C>());
}

template <Component C> bool World::has(Entity entity) const noexcept {
    if (!alive(entity)) {
        return false;
    }
    const EntityRecord& rec = record(entity);
    return m_archetypes[rec.archetype]->has_type(TypeId::of<C>());
}

template <Component C> C* World::get(Entity entity) noexcept {
    if (!has<C>(entity)) {
        return nullptr;
    }
    EntityRecord& rec = record(entity);
    Archetype& arch = *m_archetypes[rec.archetype];
    Chunk* chunk = arch.chunk_at(rec.chunk);
    return chunk->column<C>(arch) + rec.row;
}

template <typename... Cs> Query<Cs...> World::query() {
    return Query<Cs...>(this);
}

} // namespace hive::ecs
