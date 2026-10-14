#include "core/arena.h"
#include "core/pool.h"
#include "profiler/chrome_trace.h"
#include "profiler/profiler.h"

#include <iostream>
#include <thread>

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
    }

    hive::profiler::write_chrome_trace("trace.json");
    hive::profiler::shutdown();

    std::cout << "Hive Engine ready. Hardware threads: "
              << std::thread::hardware_concurrency() << '\n';
    std::cout << "Wrote trace.json\n";
    return 0;
}
