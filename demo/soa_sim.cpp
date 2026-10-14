#include "demo/soa_sim.h"

#include "core/vec2.h"
#include "ecs/command_buffer.h"
#include "profiler/profiler.h"

#include <algorithm>

namespace hive::demo {
namespace {

constexpr std::size_t kBoidGrain = 256;

} // namespace

SoaSim::SoaSim(jobs::Scheduler& jobs, DemoConfig config) : m_jobs(&jobs), m_config(config) {
    m_rng.seed(1);
    match_count();
    build_systems();
}

void SoaSim::set_scheduler(jobs::Scheduler& jobs) {
    m_jobs = &jobs;
    build_systems();
}

void SoaSim::set_config(const DemoConfig& config) {
    m_config = config;
    match_count();
}

void SoaSim::build_systems() {
    m_systems = std::make_unique<hive::ecs::SystemScheduler>(*m_jobs);
    m_systems->add<Position, const Velocity>("integrate",
                                             [this](Position& pos, const Velocity& vel) {
                                                 integrate_pos(pos, vel, m_dt);
                                             });
    m_systems->add<Color, const Velocity, const Team>(
        "recolor", [this](Color& color, const Velocity& vel, const Team& team) {
            color = color_for_speed(team, hive::core::length(vel.value), m_config.boids.max_speed);
        });
    m_systems->add<Position, Velocity>("bounce", [](Position& pos, Velocity& vel) {
        bounce_world(pos, vel);
    });
}

void SoaSim::spawn_one(hive::ecs::World& world) {
    const Boid boid = make_random_boid(m_rng, m_config.boids);
    const hive::ecs::Entity entity = world.create();
    world.add<Position>(entity, boid.pos);
    world.add<Velocity>(entity, boid.vel);
    world.add<Color>(entity, boid.color);
    world.add<Lifetime>(entity, boid.life);
    world.add<Team>(entity, boid.team);
    m_count += 1;
}

void SoaSim::match_count() {
    const auto target = static_cast<std::uint32_t>(std::max(kMinEntities, m_config.entity_count));
    while (m_count < target) {
        spawn_one(m_world);
    }
    if (m_count <= target) {
        return;
    }
    hive::ecs::CommandBuffer commands(m_world);
    std::uint32_t extra = m_count - target;
    m_world.query<Position>().each_with_entity([&](hive::ecs::Entity entity, Position&) {
        if (extra == 0) {
            return;
        }
        commands.destroy(entity);
        extra -= 1;
        m_count -= 1;
    });
    commands.playback();
}

void SoaSim::gather() {
    m_pos.clear();
    m_vel.clear();
    m_team.clear();
    m_vel_ptr.clear();
    m_world.query<Position, Velocity, Team>().each([&](Position& pos, Velocity& vel, Team& team) {
        m_pos.push_back(pos.value);
        m_vel.push_back(vel.value);
        m_team.push_back(team);
        m_vel_ptr.push_back(&vel);
    });
}

void SoaSim::apply_boids() {
    const std::uint32_t n = static_cast<std::uint32_t>(m_pos.size());
    m_accel.assign(n, {});
    if (n == 0) {
        return;
    }

    jobs::Counter done;
    m_jobs->parallel_for(
        0, n, kBoidGrain,
        [this](std::size_t i) {
            SteerAccum acc;
            const auto self = static_cast<std::uint32_t>(i);
            if (m_config.grid_on) {
                m_grid_ptr->for_neighbors(
                    self, m_pos[i], m_config.boids.perception, [&](std::uint32_t j) {
                        accumulate_neighbor(acc, m_pos[i], m_vel[i], m_team[i], m_pos[j], m_vel[j],
                                            m_team[j], m_config.boids);
                    });
            } else {
                for (std::uint32_t j = 0; j < m_pos.size(); ++j) {
                    if (j == self) {
                        continue;
                    }
                    accumulate_neighbor(acc, m_pos[i], m_vel[i], m_team[i], m_pos[j], m_vel[j],
                                        m_team[j], m_config.boids);
                }
            }
            m_accel[i] = finish_steer(m_pos[i], m_vel[i], acc, m_config.boids);
        },
        &done);
    m_jobs->wait(done);

    for (std::uint32_t i = 0; i < n; ++i) {
        m_vel[i] += m_accel[i] * m_dt;
        Velocity tmp{m_vel[i]};
        clamp_speed(tmp, m_config.boids.max_speed);
        m_vel[i] = tmp.value;
    }
}

void SoaSim::scatter_velocity() {
    const std::uint32_t n = static_cast<std::uint32_t>(m_vel_ptr.size());
    for (std::uint32_t i = 0; i < n; ++i) {
        m_vel_ptr[i]->value = m_vel[i];
    }
}

void SoaSim::lifetime_pass(float dt) {
    hive::ecs::CommandBuffer commands(m_world);
    std::uint32_t spawned = 0;
    m_world.query<Lifetime>().each_with_entity([&](hive::ecs::Entity entity, Lifetime& life) {
        life.seconds -= dt;
        if (life.seconds > 0.0f) {
            return;
        }
        commands.destroy(entity);
        m_count -= 1;
        const Boid boid = make_random_boid(m_rng, m_config.boids);
        const hive::ecs::Entity fresh = commands.create();
        commands.add<Position>(fresh, boid.pos);
        commands.add<Velocity>(fresh, boid.vel);
        commands.add<Color>(fresh, boid.color);
        commands.add<Lifetime>(fresh, boid.life);
        commands.add<Team>(fresh, boid.team);
        spawned += 1;
    });
    commands.playback();
    m_count += spawned;
}

void SoaSim::update(float dt, SpatialGrid& grid) {
    m_dt = dt;
    {
        PROFILE_SCOPE("grid");
        gather();
        if (m_config.grid_on) {
            grid.rebuild(m_pos, m_config.boids.perception);
        } else {
            grid.clear();
        }
    }
    m_grid_ptr = &grid;
    {
        PROFILE_SCOPE("boids");
        apply_boids();
        scatter_velocity();
    }
    {
        PROFILE_SCOPE("systems");
        m_systems->run(m_world);
    }
    {
        PROFILE_SCOPE("lifetime");
        lifetime_pass(dt);
        match_count();
    }
}

void SoaSim::fill_instances(std::vector<Instance>& out) {
    out.clear();
    out.reserve(m_count);
    m_world.query<const Position, const Color>().each([&](const Position& pos, const Color& color) {
        out.push_back(Instance{pos.value, color});
    });
}

} // namespace hive::demo
