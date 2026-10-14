#pragma once

#include "demo/steering.h"
#include "demo/world.h"

#include <cstdint>

namespace hive::demo {

inline constexpr float kDrawRadius = 4.0f;
inline constexpr int kMinEntities = 100;
inline constexpr int kMaxEntities = 200000;
inline constexpr int kDefaultEntities = 100000;

enum class Layout : std::uint8_t { SoA, AoS };

struct DemoConfig {
    int entity_count = kDefaultEntities;
    unsigned threads = 1;
    bool grid_on = true;
    Layout layout = Layout::SoA;
    BoidParams boids{};
};

} // namespace hive::demo
