#include "profiler/profiler.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

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

// Mutex only for init / shutdown / snapshot_all_threads. Scope ctor/dtor
// never take it — those stay on the per-thread ring.
std::mutex g_registry_mu;
std::vector<ThreadState*> g_threads;

std::uint64_t now_us() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                          std::chrono::steady_clock::now().time_since_epoch())
                                          .count());
}

void register_this_thread() {
    std::lock_guard<std::mutex> lock(g_registry_mu);
    if (std::find(g_threads.begin(), g_threads.end(), &t_state) == g_threads.end()) {
        g_threads.push_back(&t_state);
    }
}

void unregister_this_thread() {
    std::lock_guard<std::mutex> lock(g_registry_mu);
    g_threads.erase(std::remove(g_threads.begin(), g_threads.end(), &t_state), g_threads.end());
}

void copy_ring(const ThreadState& state, std::vector<Event>& out) {
    if (state.ring == nullptr || state.count == 0) {
        return;
    }
    const std::size_t oldest = (state.head + state.capacity - state.count) % state.capacity;
    out.reserve(out.size() + state.count);
    for (std::size_t i = 0; i < state.count; ++i) {
        out.push_back(state.ring[(oldest + i) % state.capacity]);
    }
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
    register_this_thread();
}

void shutdown() {
    unregister_this_thread();
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
    const std::size_t oldest = (t_state.head + t_state.capacity - t_state.count) % t_state.capacity;
    for (std::size_t i = 0; i < t_state.count; ++i) {
        t_state.snapshot[i] = t_state.ring[(oldest + i) % t_state.capacity];
    }
    return {t_state.snapshot.get(), t_state.count};
}

std::vector<Event> snapshot_all_threads() {
    std::lock_guard<std::mutex> lock(g_registry_mu);
    std::vector<Event> out;
    for (const ThreadState* state : g_threads) {
        copy_ring(*state, out);
    }
    return out;
}

} // namespace hive::profiler
