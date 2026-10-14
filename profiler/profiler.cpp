#include "profiler/profiler.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>

namespace hive::profiler {
namespace {

struct ThreadState {
    std::unique_ptr<Event[]> ring;
    std::unique_ptr<Event[]> snapshot;
    std::size_t capacity = 0;
    std::size_t head = 0;
    std::size_t count = 0;
    std::uint32_t tid = 0;
};

thread_local ThreadState t_state;
std::atomic<std::uint32_t> g_next_tid{1};

std::uint64_t now_us() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

} // namespace

void init(std::size_t events_per_thread) {
    if (events_per_thread == 0) {
        events_per_thread = 1;
    }
    t_state.capacity = events_per_thread;
    t_state.ring = std::make_unique<Event[]>(t_state.capacity);
    t_state.snapshot = std::make_unique<Event[]>(t_state.capacity);
    t_state.head = 0;
    t_state.count = 0;
    if (t_state.tid == 0) {
        t_state.tid = g_next_tid.fetch_add(1, std::memory_order_relaxed);
    }
}

void shutdown() {
    t_state.ring.reset();
    t_state.snapshot.reset();
    t_state.capacity = 0;
    t_state.head = 0;
    t_state.count = 0;
}

Scope::Scope(const char* name) noexcept {
    if (!t_state.ring) {
        return;
    }
    m_name = name;
    m_start_us = now_us();
}

Scope::~Scope() noexcept {
    if (!t_state.ring || m_name == nullptr) {
        return;
    }
    Event& slot = t_state.ring[t_state.head];
    slot.name = m_name;
    slot.start_us = m_start_us;
    slot.end_us = now_us();
    slot.tid = t_state.tid;
    t_state.head = (t_state.head + 1) % t_state.capacity;
    if (t_state.count < t_state.capacity) {
        ++t_state.count;
    }
}

std::span<const Event> events_for_this_thread() {
    if (!t_state.ring || t_state.count == 0) {
        return {};
    }
    const std::size_t oldest =
        (t_state.head + t_state.capacity - t_state.count) % t_state.capacity;
    for (std::size_t i = 0; i < t_state.count; ++i) {
        t_state.snapshot[i] = t_state.ring[(oldest + i) % t_state.capacity];
    }
    return {t_state.snapshot.get(), t_state.count};
}

} // namespace hive::profiler
