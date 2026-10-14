#pragma once

#include "demo/config.h"
#include "demo/renderer.h"
#include "demo/sim_common.h"
#include "demo/spatial_grid.h"
#include "ecs/system_scheduler.h"
#include "ecs/world.h"
#include "jobs/scheduler.h"

#include <cstdint>
#include <memory>
#include <random>
#include <vector>

namespace hive::demo {

class SoaSim {
public:
    explicit SoaSim(jobs::Scheduler& jobs, DemoConfig config);

    void set_scheduler(jobs::Scheduler& jobs);
    void set_config(const DemoConfig& config);
    void update(float dt, SpatialGrid& grid);
    void fill_instances(std::vector<Instance>& out);

    [[nodiscard]] std::uint32_t count() const noexcept {
        return m_count;
    }

private:
    void build_systems();
    void spawn_one(hive::ecs::World& world);
    void gather();
    void apply_boids();
    void scatter_velocity();
    void lifetime_pass(float dt);
    void match_count();

    jobs::Scheduler* m_jobs = nullptr;
    SpatialGrid* m_grid_ptr = nullptr;
    DemoConfig m_config{};
    float m_dt = 1.0f / 60.0f;
    hive::ecs::World m_world;
    std::unique_ptr<hive::ecs::SystemScheduler> m_systems;
    std::minstd_rand m_rng{1};
    std::uint32_t m_count = 0;

    std::vector<hive::core::Vec2> m_pos;
    std::vector<hive::core::Vec2> m_vel;
    std::vector<Team> m_team;
    std::vector<Velocity*> m_vel_ptr;
    std::vector<hive::core::Vec2> m_accel;
};

} // namespace hive::demo
