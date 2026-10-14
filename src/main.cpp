#include "core/arena.h"
#include "core/pool.h"
#include "ecs/system_scheduler.h"
#include "ecs/world.h"
#include "jobs/scheduler.h"
#include "profiler/chrome_trace.h"
#include "profiler/profiler.h"

#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

int main() {
    hive::profiler::init();
    {
        PROFILE_SCOPE("main");

        hive::core::Arena arena(1024);
        {
            PROFILE_SCOPE("Arena.allocate");
            (void)arena.allocate(128);
        }

        hive::core::Pool pool(64, 8);
        {
            PROFILE_SCOPE("Pool.allocate");
            void* block = pool.allocate();
            if (block != nullptr) {
                pool.deallocate(block);
            }
        }

        hive::jobs::Scheduler sched{std::thread::hardware_concurrency()};
        hive::jobs::Counter done;
        constexpr std::size_t kN = 10000;
        std::vector<std::int32_t> values(kN, 1);
        {
            PROFILE_SCOPE("jobs.parallel_for");
            sched.parallel_for(
                0, kN, 256,
                [&](std::size_t i) {
                    values[i] += 1;
                },
                &done);
            sched.wait(done);
        }
        std::cout << "parallel_for checksum: " << values[0] << '\n';

        struct Position {
            float x = 0;
            float y = 0;
        };
        struct Velocity {
            float x = 0;
            float y = 0;
        };
        struct Color {
            float r = 0;
            float g = 0;
        };

        hive::ecs::World world;
        constexpr int kEntities = 50000;
        for (int i = 0; i < kEntities; ++i) {
            const hive::ecs::Entity entity = world.create();
            world.add<Position>(entity, Position{static_cast<float>(i), 0.0f});
            world.add<Velocity>(entity, Velocity{1.0f, 0.5f});
            world.add<Color>(entity, Color{});
        }

        constexpr float kDt = 1.0f / 60.0f;
        hive::ecs::SystemScheduler systems(sched);
        systems.add<Position, const Velocity>("integrate",
                                              [kDt](Position& pos, const Velocity& vel) {
                                                  for (int k = 0; k < 16; ++k) {
                                                      pos.x += vel.x * kDt;
                                                      pos.y += vel.y * kDt;
                                                  }
                                              });
        systems.add<Color, const Velocity>("recolor", [](Color& color, const Velocity& vel) {
            for (int k = 0; k < 16; ++k) {
                color.r = vel.x * static_cast<float>(k + 1);
                color.g = vel.y * static_cast<float>(k + 1);
            }
        });
        systems.add<Position>("bounce", [](Position& pos) {
            if (pos.x < 0.0f) {
                pos.x = 0.0f;
            }
        });

        {
            PROFILE_SCOPE("ecs.systems");
            systems.run(world);
        }

        float checksum = 0.0f;
        world.query<const Position, const Color>().each(
            [&](const Position& pos, const Color& color) {
                checksum += pos.x + color.r;
            });
        std::cout << "ecs systems checksum: " << checksum << '\n';
        std::cout << "waves: " << systems.wave_count()
                  << " (integrate=" << systems.wave_of("integrate")
                  << " recolor=" << systems.wave_of("recolor")
                  << " bounce=" << systems.wave_of("bounce") << ")\n";

        // Dump while workers are still alive so their rings are in the registry.
        hive::profiler::write_chrome_trace("trace.json");
    }

    hive::profiler::shutdown();

    std::cout << "Hive Engine ready. Hardware threads: " << std::thread::hardware_concurrency()
              << '\n';
    std::cout << "Wrote trace.json\n";
    return 0;
}
