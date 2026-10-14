#include "core/vec2.h"
#include "demo/spatial_grid.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <vector>

TEST_CASE("spatial grid empty rebuild has no occupants") {
    hive::demo::SpatialGrid grid;
    grid.rebuild({}, 40.0f);
    REQUIRE(grid.occupant_count() == 0);
    REQUIRE(grid.cell_count() > 0);
    int visited = 0;
    grid.for_neighbors(0, {100.0f, 100.0f}, 40.0f, [&](std::uint32_t) {
        visited += 1;
    });
    REQUIRE(visited == 0);
}

TEST_CASE("spatial grid skips self and finds in-cell neighbor") {
    const std::array<hive::core::Vec2, 2> pos{{{10.0f, 10.0f}, {12.0f, 10.0f}}};
    hive::demo::SpatialGrid grid;
    grid.rebuild(pos, 40.0f);

    std::vector<std::uint32_t> found;
    grid.for_neighbors(0, pos[0], 40.0f, [&](std::uint32_t j) {
        found.push_back(j);
    });
    REQUIRE(found.size() == 1);
    REQUIRE(found[0] == 1);

    found.clear();
    grid.for_neighbors(0, pos[0], 40.0f, [&](std::uint32_t j) {
        REQUIRE(j != 0);
        found.push_back(j);
    });
    REQUIRE(found.size() == 1);
}

TEST_CASE("spatial grid ignores occupants outside radius") {
    const std::array<hive::core::Vec2, 2> pos{{{10.0f, 10.0f}, {200.0f, 10.0f}}};
    hive::demo::SpatialGrid grid;
    grid.rebuild(pos, 40.0f);
    int visited = 0;
    grid.for_neighbors(0, pos[0], 40.0f, [&](std::uint32_t) {
        visited += 1;
    });
    REQUIRE(visited == 0);
}

TEST_CASE("spatial grid finds neighbor in adjacent cell") {
    const float cell = 40.0f;
    const std::array<hive::core::Vec2, 2> pos{{{38.0f, 20.0f}, {42.0f, 20.0f}}};
    hive::demo::SpatialGrid grid;
    grid.rebuild(pos, cell);
    REQUIRE(grid.cols() > 1);

    std::vector<std::uint32_t> found;
    grid.for_neighbors(0, pos[0], 20.0f, [&](std::uint32_t j) {
        found.push_back(j);
    });
    REQUIRE(found.size() == 1);
    REQUIRE(found[0] == 1);
}

TEST_CASE("spatial grid rebuild reuses index capacity") {
    std::vector<hive::core::Vec2> pos(64, hive::core::Vec2{100.0f, 100.0f});
    for (std::size_t i = 0; i < pos.size(); ++i) {
        pos[i].x = static_cast<float>(i) * 4.0f;
    }
    hive::demo::SpatialGrid grid;
    grid.rebuild(pos, 40.0f);
    const std::size_t cap = grid.index_capacity();
    REQUIRE(cap >= pos.size());
    grid.rebuild(pos, 40.0f);
    REQUIRE(grid.index_capacity() >= cap);
    REQUIRE(grid.occupant_count() == static_cast<std::uint32_t>(pos.size()));
}

TEST_CASE("spatial grid clear drops occupants") {
    const std::array<hive::core::Vec2, 1> pos{{{50.0f, 50.0f}}};
    hive::demo::SpatialGrid grid;
    grid.rebuild(pos, 40.0f);
    REQUIRE(grid.occupant_count() == 1);
    grid.clear();
    REQUIRE(grid.occupant_count() == 0);
    REQUIRE(grid.cell_count() == 0);
}
