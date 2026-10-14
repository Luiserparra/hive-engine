#include "demo/steering.h"

#include <algorithm>
#include <cmath>

namespace hive::demo {
namespace {

hive::core::Vec2 limit(hive::core::Vec2 v, float max_length) noexcept {
    const float mag = hive::core::length(v);
    if (mag > max_length && mag > 1.0e-6f) {
        return v * (max_length / mag);
    }
    return v;
}

hive::core::Vec2 seek_velocity(hive::core::Vec2 desired_dir, hive::core::Vec2 vel,
                               const BoidParams& params) noexcept {
    const hive::core::Vec2 desired = hive::core::normalize(desired_dir) * params.max_speed;
    return limit(desired - vel, params.max_force);
}

std::uint8_t lerp_u8(std::uint8_t a, std::uint8_t b, float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    const float v = static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t;
    return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 255.0f));
}

} // namespace

void accumulate_neighbor(SteerAccum& acc, hive::core::Vec2 pos, hive::core::Vec2 vel, Team team,
                         hive::core::Vec2 npos, hive::core::Vec2 nvel, Team nteam,
                         const BoidParams& params) {
    (void)vel;
    const hive::core::Vec2 away = pos - npos;
    const float dist_sq = hive::core::length_sq(away);
    const float perc_sq = params.perception * params.perception;
    if (dist_sq > perc_sq || dist_sq < 1.0e-8f) {
        return;
    }

    const float dist = std::sqrt(dist_sq);
    if (dist < params.separation_radius) {
        acc.sep += away * (1.0f / (dist_sq + 1.0e-4f));
        acc.n_sep += 1;
    }

    if (team.id == nteam.id) {
        acc.align += nvel;
        acc.coh += npos;
        acc.n_flock += 1;
    }
}

hive::core::Vec2 finish_steer(hive::core::Vec2 pos, hive::core::Vec2 vel, const SteerAccum& acc,
                              const BoidParams& params) {
    hive::core::Vec2 force{};
    if (acc.n_sep > 0) {
        force += hive::core::normalize(acc.sep) * params.w_sep;
    }
    if (acc.n_flock > 0) {
        const float inv = 1.0f / static_cast<float>(acc.n_flock);
        force += seek_velocity(acc.align * inv, vel, params) * params.w_align;
        force += seek_velocity(acc.coh * inv - pos, vel, params) * params.w_coh;
    }
    return limit(force, params.max_force);
}

hive::core::Vec2 steer(hive::core::Vec2 pos, hive::core::Vec2 vel, Team team,
                       std::span<const Neighbor> neighbors, const BoidParams& params) {
    SteerAccum acc;
    for (const Neighbor& n : neighbors) {
        accumulate_neighbor(acc, pos, vel, team, n.pos, n.vel, n.team, params);
    }
    return finish_steer(pos, vel, acc, params);
}

Color color_for_speed(Team team, float speed, float max_speed) {
    const float t = (max_speed > 1.0e-4f) ? std::clamp(speed / max_speed, 0.0f, 1.0f) : 0.0f;
    if (team.id == 0) {
        return Color{lerp_u8(40, 80, t), lerp_u8(90, 220, t), lerp_u8(200, 255, t), 255};
    }
    return Color{lerp_u8(200, 255, t), lerp_u8(90, 200, t), lerp_u8(40, 80, t), 255};
}

} // namespace hive::demo
