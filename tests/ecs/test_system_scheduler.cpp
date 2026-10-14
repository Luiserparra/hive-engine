#include "ecs/system_scheduler.h"
#include "ecs/world.h"
#include "jobs/scheduler.h"

#include <catch2/catch_test_macros.hpp>

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

struct Color {
    float r = 0;
    float g = 0;
};

void spawn(hive::ecs::World& world, std::uint32_t count) {
    for (std::uint32_t i = 0; i < count; ++i) {
        const hive::ecs::Entity entity = world.create();
        world.add<Position>(entity, Position{0.0f, 0.0f});
        world.add<Velocity>(entity, Velocity{1.0f, 2.0f});
        world.add<Color>(entity, Color{0.0f, 0.0f});
    }
}

} // namespace

TEST_CASE("empty system scheduler run is a no-op") {
    hive::jobs::Scheduler sched{2};
    hive::ecs::SystemScheduler systems(sched);
    hive::ecs::World world;
    systems.run(world);
    REQUIRE(systems.wave_count() == 0);
}

TEST_CASE("independent systems share a wave") {
    hive::jobs::Scheduler sched{2};
    hive::ecs::SystemScheduler systems(sched);
    systems.add<Position, const Velocity>("integrate", [](Position& pos, const Velocity& vel) {
        pos.x += vel.x;
        pos.y += vel.y;
    });
    systems.add<Color, const Velocity>("recolor", [](Color& color, const Velocity& vel) {
        color.r = vel.x;
        color.g = vel.y;
    });
    systems.build();

    REQUIRE(systems.wave_count() == 1);
    REQUIRE(systems.wave_of("integrate") == 0);
    REQUIRE(systems.wave_of("recolor") == 0);

    hive::ecs::World world;
    spawn(world, 128);
    systems.run(world);

    world.query<const Position, const Color, const Velocity>().each(
        [](const Position& pos, const Color& color, const Velocity& vel) {
            REQUIRE(pos.x == vel.x);
            REQUIRE(pos.y == vel.y);
            REQUIRE(color.r == vel.x);
            REQUIRE(color.g == vel.y);
        });
}

TEST_CASE("write-write conflict serializes in registration order") {
    hive::jobs::Scheduler sched{4};
    hive::ecs::SystemScheduler systems(sched);
    systems.add<Position>("set", [](Position& pos) {
        pos.x = 1.0f;
    });
    systems.add<Position>("add", [](Position& pos) {
        pos.x += 1.0f;
    });
    systems.build();

    REQUIRE(systems.wave_count() == 2);
    REQUIRE(systems.wave_of("set") == 0);
    REQUIRE(systems.wave_of("add") == 1);

    hive::ecs::World world;
    spawn(world, 256);
    systems.run(world);

    world.query<const Position>().each([](const Position& pos) {
        REQUIRE(pos.x == 2.0f);
    });
}

TEST_CASE("read-write conflict puts the reader in a later wave") {
    hive::jobs::Scheduler sched{2};
    hive::ecs::SystemScheduler systems(sched);
    systems.add<Position>("write_pos", [](Position& pos) {
        pos.x = 4.0f;
    });
    systems.add<Color, const Position>("copy_pos", [](Color& color, const Position& pos) {
        color.r = pos.x;
    });
    systems.build();

    REQUIRE(systems.wave_count() == 2);
    REQUIRE(systems.wave_of("write_pos") == 0);
    REQUIRE(systems.wave_of("copy_pos") == 1);

    hive::ecs::World world;
    spawn(world, 64);
    systems.run(world);

    world.query<const Color>().each([](const Color& color) {
        REQUIRE(color.r == 4.0f);
    });
}

TEST_CASE("read-read of the same component does not conflict") {
    hive::jobs::Scheduler sched{2};
    hive::ecs::SystemScheduler systems(sched);
    systems.add<Position, const Velocity>("a", [](Position& pos, const Velocity& vel) {
        pos.x = vel.x;
    });
    systems.add<Color, const Velocity>("b", [](Color& color, const Velocity& vel) {
        color.r = vel.y;
    });
    systems.build();

    REQUIRE(systems.wave_count() == 1);
    REQUIRE(systems.wave_of("a") == systems.wave_of("b"));
}
