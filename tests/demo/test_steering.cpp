#include "core/vec2.h"
#include "demo/steering.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <span>

TEST_CASE("steer with no neighbors is zero") {
    hive::demo::BoidParams params{};
    const hive::core::Vec2 force =
        hive::demo::steer({0.0f, 0.0f}, {1.0f, 0.0f}, hive::demo::Team{0}, {}, params);
    REQUIRE_THAT(force.x, Catch::Matchers::WithinAbs(0.0f, 1.0e-5f));
    REQUIRE_THAT(force.y, Catch::Matchers::WithinAbs(0.0f, 1.0e-5f));
}

TEST_CASE("close neighbor produces separation away from them") {
    hive::demo::BoidParams params{};
    params.perception = 40.0f;
    params.separation_radius = 16.0f;
    const hive::demo::Neighbor other{{2.0f, 0.0f}, {0.0f, 0.0f}, hive::demo::Team{1}};
    const hive::core::Vec2 force =
        hive::demo::steer({0.0f, 0.0f}, {0.0f, 0.0f}, hive::demo::Team{0},
                          std::span<const hive::demo::Neighbor>(&other, 1), params);
    REQUIRE(force.x < 0.0f);
}

TEST_CASE("same-team neighbors contribute alignment") {
    hive::demo::BoidParams params{};
    params.w_sep = 0.0f;
    params.w_coh = 0.0f;
    params.w_align = 1.0f;
    const hive::demo::Neighbor other{{5.0f, 0.0f}, {0.0f, 20.0f}, hive::demo::Team{0}};
    const hive::core::Vec2 force =
        hive::demo::steer({0.0f, 0.0f}, {0.0f, 0.0f}, hive::demo::Team{0},
                          std::span<const hive::demo::Neighbor>(&other, 1), params);
    REQUIRE(force.y > 0.0f);
}

TEST_CASE("color_for_speed differs by team") {
    const hive::demo::Color a = hive::demo::color_for_speed(hive::demo::Team{0}, 10.0f, 140.0f);
    const hive::demo::Color b = hive::demo::color_for_speed(hive::demo::Team{1}, 10.0f, 140.0f);
    REQUIRE(a.b > a.r);
    REQUIRE(b.r > b.b);
}
