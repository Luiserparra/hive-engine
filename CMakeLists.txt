cmake_minimum_required(VERSION 3.25)
project(hive_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/warnings.cmake)
include(cmake/sanitizers.cmake)

add_subdirectory(core)
add_subdirectory(profiler)

add_executable(hive_engine src/main.cpp)
hive_set_warnings(hive_engine)
hive_enable_sanitizers(hive_engine)
target_link_libraries(hive_engine PRIVATE hive_core hive_profiler)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
