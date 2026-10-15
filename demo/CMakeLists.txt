include(FetchContent)

if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(OPENGL_VERSION "3.3" CACHE STRING "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
    SYSTEM
)
FetchContent_MakeAvailable(raylib)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
if(MSVC)
    target_compile_options(imgui PRIVATE /W0)
else()
    target_compile_options(imgui PRIVATE -w)
endif()

add_library(hive_swarm_sim
    spatial_grid.cpp
    steering.cpp
    soa_sim.cpp
    aos_sim.cpp
    steer_packed_avx2.cpp
)
target_include_directories(hive_swarm_sim PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(hive_swarm_sim PUBLIC hive_core hive_ecs hive_jobs hive_profiler)
hive_set_warnings(hive_swarm_sim)
hive_enable_sanitizers(hive_swarm_sim)

# AVX2 only on the packed neighbor kernel. The rest of hive_swarm_sim stays
# at the toolchain baseline (MSVC x64 = SSE2). Callers see a Vec2/span API.
if(MSVC)
    set_source_files_properties(steer_packed_avx2.cpp PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
else()
    set_source_files_properties(steer_packed_avx2.cpp PROPERTIES COMPILE_OPTIONS "-mavx2")
endif()

add_executable(hive_engine
    main.cpp
    renderer.cpp
    imgui_raylib.cpp
)
set_target_properties(hive_engine PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_include_directories(hive_engine PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(hive_engine PRIVATE hive_swarm_sim raylib imgui)
hive_set_warnings(hive_engine)
hive_enable_sanitizers(hive_engine)
