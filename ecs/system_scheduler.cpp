#include "ecs/system_scheduler.h"

#include "core/assert.h"
#include "jobs/counter.h"

#include <cstring>

namespace hive::ecs {

SystemScheduler::SystemScheduler(jobs::Scheduler& jobs) noexcept : m_jobs(&jobs) {}

SystemScheduler::~SystemScheduler() {
    for (System& sys : m_systems) {
        if (sys.destroy != nullptr && sys.self != nullptr) {
            sys.destroy(sys.self);
            sys.self = nullptr;
        }
    }
}

void SystemScheduler::build() {
    m_wave_count = 0;
    for (std::size_t i = 0; i < m_systems.size(); ++i) {
        std::uint32_t wave = 0;
        for (std::size_t j = 0; j < i; ++j) {
            if (conflicts(m_systems[i].access, m_systems[j].access) &&
                m_systems[j].wave + 1 > wave) {
                wave = m_systems[j].wave + 1;
            }
        }
        m_systems[i].wave = wave;
        if (wave + 1 > m_wave_count) {
            m_wave_count = wave + 1;
        }
    }
    m_dirty = false;
}

void SystemScheduler::run(World& world) {
    if (m_dirty) {
        build();
    }
    for (std::uint32_t w = 0; w < m_wave_count; ++w) {
        jobs::Counter done;
        for (System& sys : m_systems) {
            if (sys.wave != w) {
                continue;
            }
            m_jobs->schedule(
                [sys_ptr = &sys, world_ptr = &world, jobs_ptr = m_jobs]() {
                    sys_ptr->invoke(sys_ptr->self, *world_ptr, *jobs_ptr, sys_ptr->name);
                },
                &done);
        }
        m_jobs->wait(done);
    }
}

std::uint32_t SystemScheduler::wave_count() const noexcept {
    return m_wave_count;
}

std::uint32_t SystemScheduler::wave_of(const char* name) const noexcept {
    HIVE_ASSERT(name != nullptr);
    HIVE_ASSERT(!m_dirty);
    for (const System& sys : m_systems) {
        if (std::strcmp(sys.name, name) == 0) {
            return sys.wave;
        }
    }
    HIVE_ASSERT(false && "unknown system name");
    return 0;
}

} // namespace hive::ecs
