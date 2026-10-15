#pragma once

#include "core/vec2.h"
#include "demo/components.h"
#include "demo/config.h"
#include "demo/steering.h"

#include <cmath>
#include <cstdint>
#include <random>

namespace hive::demo {

inline constexpr float kLifetimeMin = 6.0f;
inline constexpr float kLifetimeMax = 18.0f;

struct Boid {
    Position pos{};
    Velocity vel{};
    Color color{};
    Lifetime life{};
    Team team{};
};

[[nodiscard]] inline float rand01(std::minstd_rand& rng) {
    return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
}

[[nodiscard]] inline Boid make_random_boid(std::minstd_rand& rng, const BoidParams& params) {
    Boid boid;
    boid.pos.value = {rand01(rng) * kWorldWidth, rand01(rng) * kWorldHeight};
    const float angle = rand01(rng) * 6.2831853f;
    const float speed = params.max_speed * (0.4f + 0.6f * rand01(rng));
    boid.vel.value = {std::cos(angle) * speed, std::sin(angle) * speed};
    boid.team.id = static_cast<std::uint8_t>(rng() & 1u);
    boid.life.seconds = kLifetimeMin + rand01(rng) * (kLifetimeMax - kLifetimeMin);
    boid.color = color_for_speed(boid.team, hive::core::length(boid.vel.value), params.max_speed);
    return boid;
}

inline void integrate_pos(Position& pos, const Velocity& vel, float dt) {
    pos.value += vel.value * dt;
}

inline void bounce_world(Position& pos, Velocity& vel) {
    if (pos.value.x < 0.0f) {
        pos.value.x = 0.0f;
        vel.value.x = std::abs(vel.value.x);
    } else if (pos.value.x > kWorldWidth) {
        pos.value.x = kWorldWidth;
        vel.value.x = -std::abs(vel.value.x);
    }
    if (pos.value.y < 0.0f) {
        pos.value.y = 0.0f;
        vel.value.y = std::abs(vel.value.y);
    } else if (pos.value.y > kWorldHeight) {
        pos.value.y = kWorldHeight;
        vel.value.y = -std::abs(vel.value.y);
    }
}

inline void clamp_speed(Velocity& vel, float max_speed) {
    const float mag = hive::core::length(vel.value);
    if (mag > max_speed && mag > 1.0e-6f) {
        vel.value *= max_speed / mag;
    }
}

} // namespace hive::demo
