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

TEST_CASE("query on empty world does not invoke") {
    hive::ecs::World world;
    int calls = 0;
    world.query<Position>().each([&](Position&) {
        ++calls;
    });
    REQUIRE(calls == 0);
}

TEST_CASE("query matches subset archetypes") {
    hive::ecs::World world;
    const hive::ecs::Entity only_pos = world.create();
    const hive::ecs::Entity both = world.create();
    world.add<Position>(only_pos, Position{1.0f, 0.0f});
    world.add<Position>(both, Position{2.0f, 0.0f});
    world.add<Velocity>(both, Velocity{5.0f, 0.0f});

    int pos_count = 0;
    world.query<Position>().each([&](Position&) {
        ++pos_count;
    });
    REQUIRE(pos_count == 2);

    int both_count = 0;
    world.query<Position, Velocity>().each([&](Position&, Velocity&) {
        ++both_count;
    });
    REQUIRE(both_count == 1);

    int vel_count = 0;
    world.query<Velocity>().each([&](Velocity&) {
        ++vel_count;
    });
    REQUIRE(vel_count == 1);
}

TEST_CASE("query const component is read-only in the lambda") {
    hive::ecs::World world;
    const hive::ecs::Entity entity = world.create();
    world.add<Position>(entity, Position{0.0f, 0.0f});
    world.add<Velocity>(entity, Velocity{1.5f, 2.5f});

    world.query<Position, const Velocity>().each([](Position& pos, const Velocity& vel) {
        pos.x += vel.x;
        pos.y += vel.y;
    });

    REQUIRE(world.get<Position>(entity)->x == 1.5f);
    REQUIRE(world.get<Position>(entity)->y == 2.5f);
    REQUIRE(world.get<Velocity>(entity)->x == 1.5f);
}

TEST_CASE("each_with_entity yields the handle") {
    hive::ecs::World world;
    const hive::ecs::Entity entity = world.create();
    world.add<Position>(entity, Position{9.0f, 0.0f});

    std::vector<hive::ecs::Entity> seen;
    world.query<Position>().each_with_entity([&](hive::ecs::Entity e, Position& pos) {
        seen.push_back(e);
        REQUIRE(pos.x == 9.0f);
    });
    REQUIRE(seen.size() == 1);
    REQUIRE(seen[0] == entity);
    REQUIRE(world.alive(seen[0]));
}
