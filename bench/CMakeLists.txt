include(FetchContent)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.1
    GIT_SHALLOW TRUE
    SYSTEM
)
FetchContent_MakeAvailable(benchmark)

add_executable(hive_bench bench_jobs.cpp bench_ecs.cpp)
target_include_directories(hive_bench PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(hive_bench PRIVATE hive_jobs hive_ecs benchmark::benchmark benchmark::benchmark_main)
hive_set_warnings(hive_bench)
