#include "ecs/world.h"
#include "jobs/scheduler.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdint>

namespace {

struct Position {
    float x = 0;
    float y = 0;
};

struct Velocity {
    float x = 0;
    float y = 0;
};

void spawn_pair(hive::ecs::World& world, std::uint32_t count, float vx) {
    for (std::uint32_t i = 0; i < count; ++i) {
        const hive::ecs::Entity entity = world.create();
        world.add<Position>(entity, Position{static_cast<float>(i), 0.0f});
        world.add<Velocity>(entity, Velocity{vx, 1.0f});
    }
}

float checksum(hive::ecs::World& world) {
    float sum = 0.0f;
    world.query<const Position>().each([&](const Position& pos) {
        sum += pos.x + pos.y;
    });
    return sum;
}

} // namespace

TEST_CASE("parallel_each on empty world does not invoke") {
    hive::ecs::World world;
    hive::jobs::Scheduler sched{2};
    int calls = 0;
    world.query<Position>().parallel_each(sched, [&](Position&) {
        ++calls;
    });
    REQUIRE(calls == 0);
}

TEST_CASE("parallel_each matches sequential each") {
    hive::ecs::World sequential;
    hive::ecs::World parallel;
    spawn_pair(sequential, 3000, 1.5f);
    spawn_pair(parallel, 3000, 1.5f);

    sequential.query<Position, const Velocity>().each([](Position& pos, const Velocity& vel) {
        pos.x += vel.x;
        pos.y += vel.y;
    });

    hive::jobs::Scheduler sched{4};
    parallel.query<Position, const Velocity>().parallel_each(
        sched, [](Position& pos, const Velocity& vel) {
            pos.x += vel.x;
            pos.y += vel.y;
        });

    REQUIRE(checksum(parallel) == checksum(sequential));
}

TEST_CASE("parallel_each one worker matches many workers") {
    hive::ecs::World one;
    hive::ecs::World many;
    spawn_pair(one, 3000, 2.0f);
    spawn_pair(many, 3000, 2.0f);

    hive::jobs::Scheduler sched_one{1};
    one.query<Position, const Velocity>().parallel_each(sched_one,
                                                        [](Position& pos, const Velocity& vel) {
                                                            pos.x += vel.x;
                                                            pos.y += vel.y;
                                                        });

    hive::jobs::Scheduler sched_many{4};
    many.query<Position, const Velocity>().parallel_each(sched_many,
                                                         [](Position& pos, const Velocity& vel) {
                                                             pos.x += vel.x;
                                                             pos.y += vel.y;
                                                         });

    REQUIRE(checksum(one) == checksum(many));
}

TEST_CASE("parallel_each const component is read-only") {
    hive::ecs::World world;
    const hive::ecs::Entity entity = world.create();
    world.add<Position>(entity, Position{0.0f, 0.0f});
    world.add<Velocity>(entity, Velocity{1.5f, 2.5f});

    hive::jobs::Scheduler sched{2};
    world.query<Position, const Velocity>().parallel_each(sched,
                                                          [](Position& pos, const Velocity& vel) {
                                                              pos.x += vel.x;
                                                              pos.y += vel.y;
                                                          });

    REQUIRE(world.get<Position>(entity)->x == 1.5f);
    REQUIRE(world.get<Position>(entity)->y == 2.5f);
    REQUIRE(world.get<Velocity>(entity)->x == 1.5f);
}

TEST_CASE("parallel_each_with_entity visits every live row") {
    hive::ecs::World world;
    spawn_pair(world, 3000, 0.0f);

    hive::jobs::Scheduler sched{4};
    std::atomic<int> visits{0};
    std::atomic<int> nulls{0};
    world.query<Position>().parallel_each_with_entity(
        sched, [&](hive::ecs::Entity entity, Position&) {
            if (entity.is_null()) {
                nulls.fetch_add(1, std::memory_order_relaxed);
            }
            visits.fetch_add(1, std::memory_order_relaxed);
        });
    REQUIRE(nulls.load(std::memory_order_relaxed) == 0);
    REQUIRE(visits.load(std::memory_order_relaxed) == 3000);
}

TEST_CASE("parallel_each matches subset archetypes") {
    hive::ecs::World world;
    const hive::ecs::Entity only_pos = world.create();
    const hive::ecs::Entity both = world.create();
    world.add<Position>(only_pos, Position{1.0f, 0.0f});
    world.add<Position>(both, Position{2.0f, 0.0f});
    world.add<Velocity>(both, Velocity{5.0f, 0.0f});

    hive::jobs::Scheduler sched{2};
    std::atomic<int> pos_count{0};
    world.query<Position>().parallel_each(sched, [&](Position&) {
        pos_count.fetch_add(1, std::memory_order_relaxed);
    });
    REQUIRE(pos_count.load(std::memory_order_relaxed) == 2);

    std::atomic<int> both_count{0};
    world.query<Position, Velocity>().parallel_each(sched, [&](Position&, Velocity&) {
        both_count.fetch_add(1, std::memory_order_relaxed);
    });
    REQUIRE(both_count.load(std::memory_order_relaxed) == 1);
}
