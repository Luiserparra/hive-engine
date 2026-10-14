#include "demo/aos_sim.h"

#include "profiler/profiler.h"

#include <algorithm>

namespace hive::demo {
namespace {

constexpr std::size_t kBoidGrain = 256;

} // namespace

AosSim::AosSim(jobs::Scheduler& jobs, DemoConfig config) : m_jobs(&jobs), m_config(config) {
    m_rng.seed(2);
    match_count();
}

void AosSim::set_scheduler(jobs::Scheduler& jobs) {
    m_jobs = &jobs;
}

void AosSim::set_config(const DemoConfig& config) {
    m_config = config;
    match_count();
}

void AosSim::match_count() {
    const auto target = static_cast<std::size_t>(std::max(kMinEntities, m_config.entity_count));
    while (m_boids.size() < target) {
        m_boids.push_back(make_random_boid(m_rng, m_config.boids));
    }
    if (m_boids.size() > target) {
        m_boids.resize(target);
    }
}

void AosSim::update(float dt, SpatialGrid& grid) {
    m_dt = dt;
    match_count();
    m_grid_ptr = &grid;
    {
        PROFILE_SCOPE("grid");
        m_pos.resize(m_boids.size());
        for (std::size_t i = 0; i < m_boids.size(); ++i) {
            m_pos[i] = m_boids[i].pos.value;
        }
        if (m_config.grid_on) {
            grid.rebuild(m_pos, m_config.boids.perception);
        } else {
            grid.clear();
        }
    }
    {
        PROFILE_SCOPE("boids");
        const std::uint32_t n = static_cast<std::uint32_t>(m_boids.size());
        m_accel.assign(n, {});
        jobs::Counter done;
        m_jobs->parallel_for(
            0, n, kBoidGrain,
            [this](std::size_t i) {
                SteerAccum acc;
                const auto self = static_cast<std::uint32_t>(i);
                const Boid& boid = m_boids[i];
                if (m_config.grid_on) {
                    m_grid_ptr->for_neighbors(
                        self, boid.pos.value, m_config.boids.perception, [&](std::uint32_t j) {
                            const Boid& other = m_boids[j];
                            accumulate_neighbor(acc, boid.pos.value, boid.vel.value, boid.team,
                                                other.pos.value, other.vel.value, other.team,
                                                m_config.boids);
                        });
                } else {
                    for (std::uint32_t j = 0; j < m_boids.size(); ++j) {
                        if (j == self) {
                            continue;
                        }
                        const Boid& other = m_boids[j];
                        accumulate_neighbor(acc, boid.pos.value, boid.vel.value, boid.team,
                                            other.pos.value, other.vel.value, other.team,
                                            m_config.boids);
                    }
                }
                m_accel[i] = finish_steer(boid.pos.value, boid.vel.value, acc, m_config.boids);
            },
            &done);
        m_jobs->wait(done);
        for (std::uint32_t i = 0; i < n; ++i) {
            m_boids[i].vel.value += m_accel[i] * m_dt;
            clamp_speed(m_boids[i].vel, m_config.boids.max_speed);
        }
    }
    {
        PROFILE_SCOPE("integrate");
        jobs::Counter done;
        m_jobs->parallel_for(
            0, m_boids.size(), kBoidGrain,
            [this](std::size_t i) {
                integrate_pos(m_boids[i].pos, m_boids[i].vel, m_dt);
            },
            &done);
        m_jobs->wait(done);
    }
    {
        PROFILE_SCOPE("recolor");
        jobs::Counter done;
        m_jobs->parallel_for(
            0, m_boids.size(), kBoidGrain,
            [this](std::size_t i) {
                Boid& boid = m_boids[i];
                boid.color = color_for_speed(boid.team, hive::core::length(boid.vel.value),
                                             m_config.boids.max_speed);
            },
            &done);
        m_jobs->wait(done);
    }
    {
        PROFILE_SCOPE("bounce");
        jobs::Counter done;
        m_jobs->parallel_for(
            0, m_boids.size(), kBoidGrain,
            [this](std::size_t i) {
                bounce_world(m_boids[i].pos, m_boids[i].vel);
            },
            &done);
        m_jobs->wait(done);
    }
    {
        PROFILE_SCOPE("lifetime");
        for (std::size_t i = 0; i < m_boids.size();) {
            m_boids[i].life.seconds -= m_dt;
            if (m_boids[i].life.seconds > 0.0f) {
                ++i;
                continue;
            }
            m_boids[i] = make_random_boid(m_rng, m_config.boids);
            ++i;
        }
    }
}

void AosSim::fill_instances(std::vector<Instance>& out) const {
    out.clear();
    out.reserve(m_boids.size());
    for (const Boid& boid : m_boids) {
        out.push_back(Instance{boid.pos.value, boid.color});
    }
}

} // namespace hive::demo
