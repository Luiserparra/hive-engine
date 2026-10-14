include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.1
    GIT_SHALLOW TRUE
    SYSTEM
)
FetchContent_MakeAvailable(Catch2)

add_executable(hive_tests
    core/test_arena.cpp
    core/test_pool.cpp
    core/test_vec2.cpp
    profiler/test_profiler.cpp
    jobs/test_deque.cpp
    jobs/test_scheduler.cpp
    jobs/test_stress.cpp
    ecs/test_entity.cpp
    ecs/test_world.cpp
    ecs/test_migration.cpp
    ecs/test_query.cpp
    ecs/test_commands.cpp
    ecs/test_parallel_query.cpp
    ecs/test_system_scheduler.cpp
    demo/test_spatial_grid.cpp
    demo/test_steering.cpp
)
target_link_libraries(hive_tests PRIVATE hive_core hive_profiler hive_jobs hive_ecs hive_swarm_sim Catch2::Catch2WithMain)
hive_set_warnings(hive_tests)
hive_enable_sanitizers(hive_tests)

include(Catch)
catch_discover_tests(hive_tests PROPERTIES TIMEOUT 180)
