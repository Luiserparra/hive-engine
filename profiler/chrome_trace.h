#pragma once

namespace hive::profiler {

// Writes this thread's recorded events as Chrome Trace JSON (ph: "X").
// Overwrites `path`. Not a hot path — may allocate.
void write_chrome_trace(const char* path);

} // namespace hive::profiler
