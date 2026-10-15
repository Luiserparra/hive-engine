add_library(hive_profiler
    profiler.cpp
    chrome_trace.cpp
)
target_include_directories(hive_profiler PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(hive_profiler PUBLIC hive_core)
hive_set_warnings(hive_profiler)
hive_enable_sanitizers(hive_profiler)
