#pragma once

#include "ecs/type.h"
#include "ecs/world.h"
#include "jobs/scheduler.h"

#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace hive::ecs {

struct Access {
    ComponentMask reads = 0;
    ComponentMask writes = 0;
};

[[nodiscard]] inline bool conflicts(Access a, Access b) noexcept {
    return ((a.writes & b.writes) != 0) || ((a.writes & b.reads) != 0) ||
           ((a.reads & b.writes) != 0);
}

template <typename C> inline void accumulate_access(Access& access) noexcept {
    const ComponentMask bit = ComponentMask{1} << TypeId::of<C>();
    if constexpr (std::is_const_v<C>) {
        access.reads |= bit;
    } else {
        access.writes |= bit;
    }
}

template <typename... Cs> [[nodiscard]] inline Access access_of() noexcept {
    static_assert(sizeof...(Cs) > 0, "access_of requires at least one component");
    Access access;
    (accumulate_access<Cs>(access), ...);
    return access;
}

// Registers systems, colours them into waves from R/W masks, then runs each
// wave: non-conflicting systems as concurrent jobs, each system parallel_each
// over chunks. Registration order is the happens-before for conflicts.
class SystemScheduler {
public:
    explicit SystemScheduler(jobs::Scheduler& jobs) noexcept;
    SystemScheduler(const SystemScheduler&) = delete;
    SystemScheduler& operator=(const SystemScheduler&) = delete;
    SystemScheduler(SystemScheduler&&) = delete;
    SystemScheduler& operator=(SystemScheduler&&) = delete;
    ~SystemScheduler();

    // `name` must be a string literal (profiler stores the pointer). `fn` is
    // the row lambda; masks come from Cs... (const = read, mutable = write).
    template <typename... Cs, typename F> void add(const char* name, F fn);

    void build();
    void run(World& world);

    [[nodiscard]] std::uint32_t wave_count() const noexcept;
    [[nodiscard]] std::uint32_t wave_of(const char* name) const noexcept;

private:
    struct System {
        const char* name = nullptr;
        Access access{};
        std::uint32_t wave = 0;
        void (*invoke)(void* self, World& world, jobs::Scheduler& sched,
                       const char* name) = nullptr;
        void (*destroy)(void* self) = nullptr;
        void* self = nullptr;
    };

    jobs::Scheduler* m_jobs = nullptr;
    std::vector<System> m_systems;
    std::uint32_t m_wave_count = 0;
    bool m_dirty = true;
};

template <typename... Cs, typename F> void SystemScheduler::add(const char* name, F fn) {
    static_assert(sizeof...(Cs) > 0, "system requires at least one component");
    HIVE_ASSERT(name != nullptr);

    using Fn = std::decay_t<F>;
    auto heap_fn = std::make_unique<Fn>(std::move(fn));

    System sys;
    sys.name = name;
    sys.access = access_of<Cs...>();
    sys.self = heap_fn.release();
    sys.destroy = [](void* self) {
        std::unique_ptr<Fn> owned(static_cast<Fn*>(self));
    };
    sys.invoke = [](void* self, World& world, jobs::Scheduler& sched, const char* scope) {
        Fn& row_fn = *static_cast<Fn*>(self);
        world.query<Cs...>().parallel_each(sched, row_fn, scope);
    };
    m_systems.push_back(sys);
    m_dirty = true;
}

} // namespace hive::ecs
