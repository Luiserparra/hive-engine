#include "core/arena.h"
#include "core/pool.h"
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
    }

    hive::profiler::write_chrome_trace("trace.json");
    hive::profiler::shutdown();

    std::cout << "Hive Engine ready. Hardware threads: " << std::thread::hardware_concurrency()
              << '\n';
    std::cout << "Wrote trace.json\n";
    return 0;
}
