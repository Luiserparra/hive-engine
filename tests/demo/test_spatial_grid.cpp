#include "core/vec2.h"
#include "demo/spatial_grid.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
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

namespace {

std::vector<hive::core::Vec2> pack_positions(const hive::demo::SpatialGrid& grid,
                                             std::span<const hive::core::Vec2> pos) {
    const auto idx = grid.indices();
    std::vector<hive::core::Vec2> packed(idx.size());
    for (std::size_t k = 0; k < idx.size(); ++k) {
        packed[k] = pos[idx[k]];
    }
    return packed;
}

std::vector<std::uint32_t> neighbor_ids(const hive::demo::SpatialGrid& grid, std::uint32_t self,
                                        hive::core::Vec2 p, float radius) {
    std::vector<std::uint32_t> found;
    grid.for_neighbors(self, p, radius, [&](std::uint32_t j) {
        found.push_back(j);
    });
    std::sort(found.begin(), found.end());
    return found;
}

std::vector<std::uint32_t> packed_neighbor_ids(const hive::demo::SpatialGrid& grid,
                                               std::span<const hive::core::Vec2> packed,
                                               std::uint32_t self, hive::core::Vec2 p,
                                               float radius) {
    std::vector<std::uint32_t> found;
    const auto idx = grid.indices();
    grid.for_neighbors_packed(self, p, radius, packed,
                              [&](std::uint32_t slot, hive::core::Vec2, float) {
                                  found.push_back(idx[slot]);
                              });
    std::sort(found.begin(), found.end());
    return found;
}

} // namespace

TEST_CASE("spatial grid packed visits match index walk") {
    const std::array<hive::core::Vec2, 6> pos{{
        {10.0f, 10.0f},
        {12.0f, 10.0f},
        {38.0f, 20.0f},
        {42.0f, 20.0f},
        {200.0f, 10.0f},
        {11.0f, 14.0f},
    }};
    hive::demo::SpatialGrid grid;
    grid.rebuild(pos, 40.0f);
    const auto packed = pack_positions(grid, pos);

    for (std::uint32_t self = 0; self < pos.size(); ++self) {
        const auto ids = neighbor_ids(grid, self, pos[self], 40.0f);
        const auto packed_ids = packed_neighbor_ids(grid, packed, self, pos[self], 40.0f);
        REQUIRE(ids == packed_ids);
        REQUIRE(std::find(packed_ids.begin(), packed_ids.end(), self) == packed_ids.end());
    }
}

TEST_CASE("spatial grid packed skips self") {
    const std::array<hive::core::Vec2, 2> pos{{{10.0f, 10.0f}, {12.0f, 10.0f}}};
    hive::demo::SpatialGrid grid;
    grid.rebuild(pos, 40.0f);
    const auto packed = pack_positions(grid, pos);

    std::vector<std::uint32_t> found;
    grid.for_neighbors_packed(0, pos[0], 40.0f, packed,
                              [&](std::uint32_t slot, hive::core::Vec2, float) {
                                  REQUIRE(grid.indices()[slot] != 0);
                                  found.push_back(grid.indices()[slot]);
                              });
    REQUIRE(found.size() == 1);
    REQUIRE(found[0] == 1);
}

TEST_CASE("spatial grid packed ignores occupants outside radius") {
    const std::array<hive::core::Vec2, 2> pos{{{10.0f, 10.0f}, {200.0f, 10.0f}}};
    hive::demo::SpatialGrid grid;
    grid.rebuild(pos, 40.0f);
    const auto packed = pack_positions(grid, pos);
    int visited = 0;
    grid.for_neighbors_packed(0, pos[0], 40.0f, packed,
                              [&](std::uint32_t, hive::core::Vec2, float) {
                                  visited += 1;
                              });
    REQUIRE(visited == 0);
}

TEST_CASE("spatial grid packed finds neighbor in adjacent cell") {
    const float cell = 40.0f;
    const std::array<hive::core::Vec2, 2> pos{{{38.0f, 20.0f}, {42.0f, 20.0f}}};
    hive::demo::SpatialGrid grid;
    grid.rebuild(pos, cell);
    REQUIRE(grid.cols() > 1);
    const auto packed = pack_positions(grid, pos);

    std::vector<std::uint32_t> found;
    grid.for_neighbors_packed(0, pos[0], 20.0f, packed,
                              [&](std::uint32_t slot, hive::core::Vec2, float) {
                                  found.push_back(grid.indices()[slot]);
                              });
    REQUIRE(found.size() == 1);
    REQUIRE(found[0] == 1);
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
