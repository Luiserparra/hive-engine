#pragma once

#include "core/vec2.h"
#include "demo/components.h"

#include <span>

namespace hive::demo {

struct BoidParams {
    float perception = 40.0f;
    float separation_radius = 16.0f;
    float max_speed = 140.0f;
    float max_force = 240.0f;
    float w_sep = 1.6f;
    float w_align = 1.1f;
    float w_coh = 0.85f;
};

struct Neighbor {
    hive::core::Vec2 pos{};
    hive::core::Vec2 vel{};
    Team team{};
};

struct SteerAccum {
    hive::core::Vec2 sep{};
    hive::core::Vec2 align{};
    hive::core::Vec2 coh{};
    int n_sep = 0;
    int n_flock = 0;
};

void accumulate_neighbor(SteerAccum& acc, hive::core::Vec2 pos, hive::core::Vec2 vel, Team team,
                         hive::core::Vec2 npos, hive::core::Vec2 nvel, Team nteam,
                         const BoidParams& params);

[[nodiscard]] hive::core::Vec2 finish_steer(hive::core::Vec2 pos, hive::core::Vec2 vel,
                                            const SteerAccum& acc, const BoidParams& params);

[[nodiscard]] hive::core::Vec2 steer(hive::core::Vec2 pos, hive::core::Vec2 vel, Team team,
                                     std::span<const Neighbor> neighbors, const BoidParams& params);

[[nodiscard]] Color color_for_speed(Team team, float speed, float max_speed);

} // namespace hive::demo
