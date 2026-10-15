#include "core/vec2.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("vec2 add sub mul and dot") {
    const hive::core::Vec2 a{3.0f, 4.0f};
    const hive::core::Vec2 b{1.0f, 2.0f};
    const hive::core::Vec2 s = a + b;
    REQUIRE(s.x == 4.0f);
    REQUIRE(s.y == 6.0f);
    const hive::core::Vec2 d = a - b;
    REQUIRE(d.x == 2.0f);
    REQUIRE(d.y == 2.0f);
    REQUIRE(hive::core::dot(a, b) == 11.0f);
    REQUIRE(hive::core::length_sq(a) == 25.0f);
    REQUIRE_THAT(hive::core::length(a), Catch::Matchers::WithinAbs(5.0f, 1.0e-5f));
}

TEST_CASE("vec2 normalize zero is zero") {
    const hive::core::Vec2 z = hive::core::normalize({0.0f, 0.0f});
    REQUIRE(z.x == 0.0f);
    REQUIRE(z.y == 0.0f);
    const hive::core::Vec2 n = hive::core::normalize({0.0f, 8.0f});
    REQUIRE_THAT(n.x, Catch::Matchers::WithinAbs(0.0f, 1.0e-5f));
    REQUIRE_THAT(n.y, Catch::Matchers::WithinAbs(1.0f, 1.0e-5f));
}
