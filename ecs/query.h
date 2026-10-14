#pragma once

#ifndef HIVE_ECS_QUERY_INCLUDED_FROM_WORLD
#error "Include ecs/world.h rather than ecs/query.h directly"
#endif

#include "ecs/archetype.h"
#include "ecs/chunk.h"
#include "ecs/entity.h"
#include "ecs/type.h"
#include "jobs/scheduler.h"
#include "profiler/profiler.h"

#include <cstdint>
#include <tuple>
#include <type_traits>
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

template <typename... Cs, typename F> void for_each_row(Archetype& arch, Chunk& chunk, F&& fn) {
    const std::uint32_t n = chunk.count();
    auto cols = std::tuple{chunk.column<Cs>(arch)...};
    for (std::uint32_t row = 0; row < n; ++row) {
        invoke_row(fn, cols, row, std::make_index_sequence<sizeof...(Cs)>{});
    }
}

template <typename... Cs, typename F>
void for_each_row_entity(Archetype& arch, Chunk& chunk, F&& fn) {
    const std::uint32_t n = chunk.count();
    Entity* entities = chunk.entities(arch);
    auto cols = std::tuple{chunk.column<Cs>(arch)...};
    for (std::uint32_t row = 0; row < n; ++row) {
        invoke_row_entity(fn, entities[row], cols, row, std::make_index_sequence<sizeof...(Cs)>{});
    }
}

} // namespace detail

// Typed query. each() is sequential. parallel_each() schedules one job per
// matching 16 KB chunk, then wait()s. World does not own the scheduler.
template <typename... Cs> class Query {
public:
    explicit Query(World* world) noexcept : m_world(world) {}

    template <typename F> void each(F&& fn);

    template <typename F> void each_with_entity(F&& fn);

    // `scope` is a string literal (or nullptr). When set, each chunk job records
    // a profiler event with that name so independent systems show up as
    // overlapping Chrome Trace slices.
    template <typename F>
    void parallel_each(jobs::Scheduler& sched, F&& fn, const char* scope = nullptr);

    template <typename F>
    void parallel_each_with_entity(jobs::Scheduler& sched, F&& fn, const char* scope = nullptr);

private:
    template <typename F> void for_each_matching_chunk(F&& chunk_fn);

    World* m_world;
};

template <typename... Cs>
template <typename F>
void Query<Cs...>::for_each_matching_chunk(F&& chunk_fn) {
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
            if (chunk.count() == 0) {
                continue;
            }
            chunk_fn(arch, chunk);
        }
    }
}

template <typename... Cs> template <typename F> void Query<Cs...>::each(F&& fn) {
    for_each_matching_chunk([&](Archetype& arch, Chunk& chunk) {
        detail::for_each_row<Cs...>(arch, chunk, fn);
    });
}

template <typename... Cs> template <typename F> void Query<Cs...>::each_with_entity(F&& fn) {
    for_each_matching_chunk([&](Archetype& arch, Chunk& chunk) {
        detail::for_each_row_entity<Cs...>(arch, chunk, fn);
    });
}

template <typename... Cs>
template <typename F>
void Query<Cs...>::parallel_each(jobs::Scheduler& sched, F&& fn, const char* scope) {
    using Fn = std::decay_t<F>;
    Fn local = std::forward<F>(fn);
    jobs::Counter done;
    for_each_matching_chunk([&](Archetype& arch, Chunk& chunk) {
        sched.schedule(
            [&local, chunk_ptr = &chunk, arch_ptr = &arch, scope]() {
                if (scope != nullptr) {
                    PROFILE_SCOPE(scope);
                    detail::for_each_row<Cs...>(*arch_ptr, *chunk_ptr, local);
                } else {
                    detail::for_each_row<Cs...>(*arch_ptr, *chunk_ptr, local);
                }
            },
            &done);
    });
    sched.wait(done);
}

template <typename... Cs>
template <typename F>
void Query<Cs...>::parallel_each_with_entity(jobs::Scheduler& sched, F&& fn, const char* scope) {
    using Fn = std::decay_t<F>;
    Fn local = std::forward<F>(fn);
    jobs::Counter done;
    for_each_matching_chunk([&](Archetype& arch, Chunk& chunk) {
        sched.schedule(
            [&local, chunk_ptr = &chunk, arch_ptr = &arch, scope]() {
                if (scope != nullptr) {
                    PROFILE_SCOPE(scope);
                    detail::for_each_row_entity<Cs...>(*arch_ptr, *chunk_ptr, local);
                } else {
                    detail::for_each_row_entity<Cs...>(*arch_ptr, *chunk_ptr, local);
                }
            },
            &done);
    });
    sched.wait(done);
}

} // namespace hive::ecs
