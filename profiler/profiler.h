#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#define HIVE_CONCAT_INNER(a, b) a##b
#define HIVE_CONCAT(a, b) HIVE_CONCAT_INNER(a, b)

#define PROFILE_SCOPE(name)                                                                        \
    ::hive::profiler::Scope HIVE_CONCAT(_hive_scope_, __LINE__) {                                  \
        name                                                                                       \
    }

namespace hive::profiler {

struct Event {
    const char* name = nullptr; // string literal only — do not allocate
    std::uint64_t start_us = 0;
    std::uint64_t end_us = 0;
    std::uint32_t tid = 0;
};

class Scope {
public:
    explicit Scope(const char* name) noexcept;
    ~Scope() noexcept;

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&&) = delete;
    Scope& operator=(Scope&&) = delete;

private:
    const char* m_name = nullptr;
    std::uint64_t m_start_us = 0;
};

// Allocates this thread's ring buffer and registers it for snapshot_all_threads.
// Safe to call more than once (re-init replaces the buffer and discards prior
// events). Capacity 0 is treated as 1.
void init(std::size_t events_per_thread = 65536);

// Unregisters this thread and drops its buffer. Scopes become no-ops until init()
// again.
void shutdown();

// Chronological order, oldest surviving event first. Empty if not initialized.
[[nodiscard]] std::span<const Event> events_for_this_thread();

// Copies every registered thread's ring. Not a hot path — may allocate.
// Call only when those threads are idle (after wait/run); concurrent Scope
// writes would race the copy.
[[nodiscard]] std::vector<Event> snapshot_all_threads();

} // namespace hive::profiler
