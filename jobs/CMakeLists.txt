find_package(Threads REQUIRED)

add_library(hive_jobs
    deque.cpp
    counter.cpp
    scheduler.cpp
)
target_include_directories(hive_jobs PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(hive_jobs PUBLIC hive_core hive_profiler Threads::Threads)
hive_set_warnings(hive_jobs)
hive_enable_sanitizers(hive_jobs)
