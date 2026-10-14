#include "ecs/command_buffer.h"
#include "ecs/world.h"

#include <catch2/catch_test_macros.hpp>

namespace {

struct Position {
    float x = 0;
    float y = 0;
};

struct Velocity {
    float x = 0;
    float y = 0;
};

} // namespace

TEST_CASE("deferred create and add are invisible until playback") {
    hive::ecs::World world;
    hive::ecs::CommandBuffer commands(world);

    const hive::ecs::Entity entity = commands.create();
    commands.add<Position>(entity, Position{1.0f, 2.0f});
    commands.add<Velocity>(entity, Velocity{3.0f, 4.0f});

    REQUIRE_FALSE(world.alive(entity));
    int before = 0;
    world.query<Position>().each([&](Position&) {
        ++before;
    });
    REQUIRE(before == 0);

    commands.playback();

    REQUIRE(world.alive(entity));
    REQUIRE(world.get<Position>(entity)->x == 1.0f);
    REQUIRE(world.get<Velocity>(entity)->y == 4.0f);
}

TEST_CASE("destroy during iteration goes through the buffer") {
    hive::ecs::World world;
    for (int i = 0; i < 8; ++i) {
        const hive::ecs::Entity entity = world.create();
        world.add<Position>(entity, Position{static_cast<float>(i), 0.0f});
    }

    hive::ecs::CommandBuffer commands(world);
    world.query<Position>().each_with_entity([&](hive::ecs::Entity entity, Position&) {
        commands.destroy(entity);
    });

    int during = 0;
    world.query<Position>().each([&](Position&) {
        ++during;
    });
    REQUIRE(during == 8);

    commands.playback();

    int after = 0;
    world.query<Position>().each([&](Position&) {
        ++after;
    });
    REQUIRE(after == 0);
}

TEST_CASE("buffered remove applies on playback") {
    hive::ecs::World world;
    const hive::ecs::Entity entity = world.create();
    world.add<Position>(entity, Position{1.0f, 0.0f});
    world.add<Velocity>(entity, Velocity{9.0f, 0.0f});

    hive::ecs::CommandBuffer commands(world);
    commands.remove<Velocity>(entity);
    REQUIRE(world.has<Velocity>(entity));
    commands.playback();
    REQUIRE(world.has<Position>(entity));
    REQUIRE_FALSE(world.has<Velocity>(entity));
}
