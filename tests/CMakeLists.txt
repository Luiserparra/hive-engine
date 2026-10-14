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
    profiler/test_profiler.cpp
)
target_link_libraries(hive_tests PRIVATE hive_core hive_profiler Catch2::Catch2WithMain)
hive_set_warnings(hive_tests)
hive_enable_sanitizers(hive_tests)

include(Catch)
catch_discover_tests(hive_tests)
