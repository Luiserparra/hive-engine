#pragma once

#include "core/vec2.h"
#include "demo/components.h"

#include <cstdint>
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

// Hot-path accumulate: `away` is pos - npos, `dist_sq` is already length_sq(away).
// Separation uses dist_sq < r², which matches dist < r for these non-negative values.
inline void accumulate_neighbor_dist_sq(SteerAccum& acc, hive::core::Vec2 pos, hive::core::Vec2 vel,
                                        Team team, hive::core::Vec2 npos, hive::core::Vec2 nvel,
                                        Team nteam, hive::core::Vec2 away, float dist_sq,
                                        const BoidParams& params) noexcept {
    (void)pos;
    (void)vel;
    const float perc_sq = params.perception * params.perception;
    if (dist_sq > perc_sq || dist_sq < 1.0e-8f) {
        return;
    }

    const float sep_sq = params.separation_radius * params.separation_radius;
    if (dist_sq < sep_sq) {
        acc.sep += away * (1.0f / (dist_sq + 1.0e-4f));
        acc.n_sep += 1;
    }

    if (team.id == nteam.id) {
        acc.align += nvel;
        acc.coh += npos;
        acc.n_flock += 1;
    }
}

// AVX2 kernel over a contiguous packed slot range [begin, end). Same filters as
// accumulate_neighbor_dist_sq. Skips indices[k] == self_id. No allocation.
void accumulate_packed_range(SteerAccum& acc, hive::core::Vec2 self_pos, Team self_team,
                             std::uint32_t self_id, std::span<const hive::core::Vec2> packed_pos,
                             std::span<const hive::core::Vec2> packed_vel,
                             std::span<const Team> packed_team,
                             std::span<const std::uint32_t> indices, std::uint32_t begin,
                             std::uint32_t end, float perc_sq, float sep_sq) noexcept;

[[nodiscard]] hive::core::Vec2 finish_steer(hive::core::Vec2 pos, hive::core::Vec2 vel,
                                            const SteerAccum& acc, const BoidParams& params);

[[nodiscard]] hive::core::Vec2 steer(hive::core::Vec2 pos, hive::core::Vec2 vel, Team team,
                                     std::span<const Neighbor> neighbors, const BoidParams& params);

[[nodiscard]] Color color_for_speed(Team team, float speed, float max_speed);

} // namespace hive::demo
