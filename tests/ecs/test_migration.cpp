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

struct Health {
    int hp = 0;
};

} // namespace

TEST_CASE("add migrates and preserves other components") {
    hive::ecs::World world;
    const hive::ecs::Entity entity = world.create();
    world.add<Position>(entity, Position{1.0f, 2.0f});
    world.add<Velocity>(entity, Velocity{3.0f, 4.0f});

    REQUIRE(world.has<Position>(entity));
    REQUIRE(world.has<Velocity>(entity));
    REQUIRE(world.get<Position>(entity)->x == 1.0f);
    REQUIRE(world.get<Velocity>(entity)->x == 3.0f);
}

TEST_CASE("remove migrates off the component") {
    hive::ecs::World world;
    const hive::ecs::Entity entity = world.create();
    world.add<Position>(entity, Position{1.0f, 2.0f});
    world.add<Velocity>(entity, Velocity{3.0f, 4.0f});
    world.remove<Position>(entity);

    REQUIRE_FALSE(world.has<Position>(entity));
    REQUIRE(world.has<Velocity>(entity));
    REQUIRE(world.get<Position>(entity) == nullptr);
    REQUIRE(world.get<Velocity>(entity)->y == 4.0f);
}

TEST_CASE("swap-remove keeps the other entity") {
    hive::ecs::World world;
    const hive::ecs::Entity first = world.create();
    const hive::ecs::Entity second = world.create();
    world.add<Position>(first, Position{1.0f, 0.0f});
    world.add<Position>(second, Position{2.0f, 0.0f});
    world.add<Health>(second, Health{10});

    world.destroy(first);

    REQUIRE_FALSE(world.alive(first));
    REQUIRE(world.alive(second));
    REQUIRE(world.get<Position>(second)->x == 2.0f);
    REQUIRE(world.get<Health>(second)->hp == 10);
}

TEST_CASE("remove last remaining component leaves an empty entity") {
    hive::ecs::World world;
    const hive::ecs::Entity entity = world.create();
    world.add<Position>(entity, Position{8.0f, 9.0f});
    world.remove<Position>(entity);
    REQUIRE(world.alive(entity));
    REQUIRE_FALSE(world.has<Position>(entity));
}
