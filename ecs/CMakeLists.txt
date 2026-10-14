add_library(hive_ecs
    type.cpp
    chunk.cpp
    archetype.cpp
    world.cpp
    command_buffer.cpp
)
target_include_directories(hive_ecs PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(hive_ecs PUBLIC hive_core)
hive_set_warnings(hive_ecs)
hive_enable_sanitizers(hive_ecs)
