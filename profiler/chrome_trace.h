#pragma once

namespace hive::profiler {

// Writes every registered thread's events as Chrome Trace JSON (ph: "X").
// Call after wait/run so worker rings are idle. Overwrites `path`. Not a hot
// path — may allocate.
void write_chrome_trace(const char* path);

} // namespace hive::profiler
