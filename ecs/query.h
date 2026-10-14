#pragma once

#ifndef HIVE_ECS_QUERY_INCLUDED_FROM_WORLD
#error "Include ecs/world.h rather than ecs/query.h directly"
#endif

#include "ecs/archetype.h"
#include "ecs/chunk.h"
#include "ecs/entity.h"
#include "ecs/type.h"

#include <cstdint>
#include <tuple>
#include <utility>

namespace hive::ecs {

class World;

namespace detail {

template <typename F, typename Tuple, std::size_t... I>
void invoke_row(F&& fn, Tuple& cols, std::uint32_t row, std::index_sequence<I...>) {
    fn(std::get<I>(cols)[row]...);
}

template <typename F, typename Tuple, std::size_t... I>
void invoke_row_entity(F&& fn, Entity entity, Tuple& cols, std::uint32_t row,
                       std::index_sequence<I...>) {
    fn(entity, std::get<I>(cols)[row]...);
}

} // namespace detail

// Sequential typed query. Walks matching archetypes -> chunks -> rows.
// M4 will split the inner chunk loop onto the job system.
template <typename... Cs> class Query {
public:
    explicit Query(World* world) noexcept : m_world(world) {}

    template <typename F> void each(F&& fn);

    template <typename F> void each_with_entity(F&& fn);

private:
    World* m_world;
};

template <typename... Cs> template <typename F> void Query<Cs...>::each(F&& fn) {
    static_assert(sizeof...(Cs) > 0, "query requires at least one component");
    const ComponentMask required = component_mask<Cs...>();
    for (auto& arch_ptr : m_world->archetypes()) {
        Archetype& arch = *arch_ptr;
        if ((arch.mask() & required) != required) {
            continue;
        }
        for (auto& chunk_ptr : arch.chunks()) {
            if (chunk_ptr == nullptr) {
                continue;
            }
            Chunk& chunk = *chunk_ptr;
            const std::uint32_t n = chunk.count();
            if (n == 0) {
                continue;
            }
            auto cols = std::tuple{chunk.column<Cs>(arch)...};
            for (std::uint32_t row = 0; row < n; ++row) {
                detail::invoke_row(fn, cols, row, std::make_index_sequence<sizeof...(Cs)>{});
            }
        }
    }
}

template <typename... Cs> template <typename F> void Query<Cs...>::each_with_entity(F&& fn) {
    static_assert(sizeof...(Cs) > 0, "query requires at least one component");
    const ComponentMask required = component_mask<Cs...>();
    for (auto& arch_ptr : m_world->archetypes()) {
        Archetype& arch = *arch_ptr;
        if ((arch.mask() & required) != required) {
            continue;
        }
        for (auto& chunk_ptr : arch.chunks()) {
            if (chunk_ptr == nullptr) {
                continue;
            }
            Chunk& chunk = *chunk_ptr;
            const std::uint32_t n = chunk.count();
            if (n == 0) {
                continue;
            }
            Entity* entities = chunk.entities(arch);
            auto cols = std::tuple{chunk.column<Cs>(arch)...};
            for (std::uint32_t row = 0; row < n; ++row) {
                detail::invoke_row_entity(fn, entities[row], cols, row,
                                          std::make_index_sequence<sizeof...(Cs)>{});
            }
        }
    }
}

} // namespace hive::ecs
