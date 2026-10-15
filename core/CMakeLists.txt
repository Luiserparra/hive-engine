add_library(hive_core
    arena.cpp
    pool.cpp
)
target_include_directories(hive_core PUBLIC ${PROJECT_SOURCE_DIR})
hive_set_warnings(hive_core)
hive_enable_sanitizers(hive_core)
