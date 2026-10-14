#pragma once

#include "demo/config.h"
#include "demo/renderer.h"
#include "demo/sim_common.h"
#include "demo/spatial_grid.h"
#include "jobs/scheduler.h"

#include <cstdint>
#include <random>
#include <vector>

namespace hive::demo {

class AosSim {
public:
    explicit AosSim(jobs::Scheduler& jobs, DemoConfig config);

    void set_scheduler(jobs::Scheduler& jobs);
    void set_config(const DemoConfig& config);
    void update(float dt, SpatialGrid& grid);
    void fill_instances(std::vector<Instance>& out) const;

    [[nodiscard]] std::uint32_t count() const noexcept {
        return static_cast<std::uint32_t>(m_boids.size());
    }

private:
    void match_count();

    jobs::Scheduler* m_jobs = nullptr;
    SpatialGrid* m_grid_ptr = nullptr;
    DemoConfig m_config{};
    float m_dt = 1.0f / 60.0f;
    std::minstd_rand m_rng{2};
    std::vector<Boid> m_boids;
    std::vector<hive::core::Vec2> m_pos;
    std::vector<hive::core::Vec2> m_accel;
};

} // namespace hive::demo
