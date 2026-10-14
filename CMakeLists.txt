cmake_minimum_required(VERSION 3.25)
project(hive_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/warnings.cmake)
include(cmake/sanitizers.cmake)

# Fetch Google Benchmark only when asked, or automatically in Release.
if(NOT DEFINED HIVE_BUILD_BENCH AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set(HIVE_BUILD_BENCH ON)
endif()

add_subdirectory(core)
add_subdirectory(profiler)
add_subdirectory(jobs)
add_subdirectory(ecs)
add_subdirectory(demo)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

option(HIVE_BUILD_BENCH "Build Google Benchmarks" OFF)
if(HIVE_BUILD_BENCH)
    add_subdirectory(bench)
endif()
