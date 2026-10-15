#include "ecs/world.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("null entity is index 0") {
    hive::ecs::Entity entity;
    REQUIRE(entity.is_null());
    REQUIRE(entity == hive::ecs::Entity{});
}

TEST_CASE("entity equality uses index and generation") {
    const hive::ecs::Entity a{1, 1};
    const hive::ecs::Entity b{1, 1};
    const hive::ecs::Entity c{1, 2};
    const hive::ecs::Entity d{2, 1};
    REQUIRE(a == b);
    REQUIRE(a != c);
    REQUIRE(a != d);
}

TEST_CASE("destroy recycles index and bumps generation") {
    hive::ecs::World world;
    const hive::ecs::Entity first = world.create();
    REQUIRE(world.alive(first));
    const std::uint32_t index = first.index;
    world.destroy(first);
    REQUIRE_FALSE(world.alive(first));

    const hive::ecs::Entity second = world.create();
    REQUIRE(second.index == index);
    REQUIRE(second.generation != first.generation);
    REQUIRE(world.alive(second));
    REQUIRE_FALSE(world.alive(first));
}
