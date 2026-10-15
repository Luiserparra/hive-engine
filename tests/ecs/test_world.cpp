#include "ecs/world.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

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

TEST_CASE("create is alive destroy is not") {
    hive::ecs::World world;
    const hive::ecs::Entity entity = world.create();
    REQUIRE_FALSE(entity.is_null());
    REQUIRE(world.alive(entity));
    world.destroy(entity);
    REQUIRE_FALSE(world.alive(entity));
}

TEST_CASE("destroy of stale handle is a no-op") {
    hive::ecs::World world;
    const hive::ecs::Entity entity = world.create();
    world.destroy(entity);
    world.destroy(entity);
    REQUIRE_FALSE(world.alive(entity));
}

TEST_CASE("has and get after add") {
    hive::ecs::World world;
    const hive::ecs::Entity entity = world.create();
    REQUIRE_FALSE(world.has<Position>(entity));
    REQUIRE(world.get<Position>(entity) == nullptr);

    world.add<Position>(entity, Position{3.0f, 4.0f});
    REQUIRE(world.has<Position>(entity));
    REQUIRE_FALSE(world.has<Velocity>(entity));
    Position* pos = world.get<Position>(entity);
    REQUIRE(pos != nullptr);
    REQUIRE(pos->x == 3.0f);
    REQUIRE(pos->y == 4.0f);
}

TEST_CASE("get on dead entity is nullptr") {
    hive::ecs::World world;
    const hive::ecs::Entity entity = world.create();
    world.add<Position>(entity, Position{1.0f, 2.0f});
    world.destroy(entity);
    REQUIRE(world.get<Position>(entity) == nullptr);
    REQUIRE_FALSE(world.has<Position>(entity));
}

TEST_CASE("fill spans more than one chunk") {
    hive::ecs::World world;
    constexpr int kCount = 3000;
    std::vector<hive::ecs::Entity> entities;
    entities.reserve(kCount);
    for (int i = 0; i < kCount; ++i) {
        const hive::ecs::Entity entity = world.create();
        world.add<Position>(entity, Position{static_cast<float>(i), 0.0f});
        entities.push_back(entity);
    }

    int visited = 0;
    world.query<Position>().each([&](Position& pos) {
        REQUIRE(pos.x >= 0.0f);
        ++visited;
    });
    REQUIRE(visited == kCount);

    for (const hive::ecs::Entity entity : entities) {
        REQUIRE(world.alive(entity));
        REQUIRE(world.get<Position>(entity) != nullptr);
    }
}
