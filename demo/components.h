#pragma once

#include "core/vec2.h"

#include <cstdint>

namespace hive::demo {

struct Position {
    hive::core::Vec2 value{};
};

struct Velocity {
    hive::core::Vec2 value{};
};

struct Color {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
};

struct Lifetime {
    float seconds = 1.0f;
};

struct Team {
    std::uint8_t id = 0;
};

} // namespace hive::demo
