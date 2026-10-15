#include "profiler/profiler.h"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

namespace {

constexpr int kRings = 12;
constexpr std::size_t kCapacity = 65536;

constexpr const char* kNames[] = {
    "grid", "boids", "systems", "integrate", "recolor", "bounce", "lifetime", "render", "frame",
};

std::uint64_t now_us() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                          std::chrono::steady_clock::now().time_since_epoch())
                                          .count());
}

void record_names() {
    for (const char* name : kNames) {
        PROFILE_SCOPE(name);
    }
}

float wall_ms(const std::vector<hive::profiler::Event>& events, const char* name,
              std::uint64_t frame_start) {
    std::uint64_t lo = ~std::uint64_t{0};
    std::uint64_t hi = 0;
    for (const hive::profiler::Event& event : events) {
        if (event.name == nullptr || std::strcmp(event.name, name) != 0) {
            continue;
        }
        if (event.end_us < frame_start) {
            continue;
        }
        lo = std::min(lo, event.start_us);
        hi = std::max(hi, event.end_us);
    }
    if (hi <= lo || lo == ~std::uint64_t{0}) {
        return 0.0f;
    }
    return static_cast<float>(hi - lo) / 1000.0f;
}

float sample_nine(const std::vector<hive::profiler::Event>& events, std::uint64_t frame_start) {
    float acc = 0.0f;
    for (const char* name : kNames) {
        acc += wall_ms(events, name, frame_start);
    }
    return acc;
}

struct Rings {
    std::vector<std::thread> workers;
    std::atomic<int> warmup_done{0};
    std::atomic<int> frame_done{0};
    std::atomic<int> phase{0};
    std::atomic<bool> stop{false};
    std::uint64_t frame_start = 0;

    explicit Rings(std::size_t fill) {
        workers.reserve(static_cast<std::size_t>(kRings - 1));
        for (int i = 0; i < kRings - 1; ++i) {
            workers.emplace_back([this, fill] {
                hive::profiler::init(kCapacity);
                for (std::size_t n = 0; n < fill; ++n) {
                    PROFILE_SCOPE("warmup");
                }
                warmup_done.fetch_add(1, std::memory_order_release);
                while (phase.load(std::memory_order_acquire) < 1 &&
                       !stop.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
                record_names();
                frame_done.fetch_add(1, std::memory_order_release);
                while (!stop.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
                hive::profiler::shutdown();
            });
        }

        hive::profiler::init(kCapacity);
        for (std::size_t n = 0; n < fill; ++n) {
            PROFILE_SCOPE("warmup");
        }
        warmup_done.fetch_add(1, std::memory_order_release);
        while (warmup_done.load(std::memory_order_acquire) < kRings) {
            std::this_thread::yield();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        frame_start = now_us();
        phase.store(1, std::memory_order_release);
        record_names();
        frame_done.fetch_add(1, std::memory_order_release);
        while (frame_done.load(std::memory_order_acquire) < kRings) {
            std::this_thread::yield();
        }
    }

    Rings(const Rings&) = delete;
    Rings& operator=(const Rings&) = delete;

    ~Rings() {
        stop.store(true, std::memory_order_release);
        for (std::thread& worker : workers) {
            worker.join();
        }
        hive::profiler::shutdown();
    }
};

} // namespace

static void BM_Snapshot(benchmark::State& state) {
    Rings rings{static_cast<std::size_t>(state.range(0))};
    for (auto _ : state) {
        std::vector<hive::profiler::Event> events = hive::profiler::snapshot_all_threads();
        benchmark::DoNotOptimize(events.data());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_Snapshot)
    ->Arg(1024)
    ->Arg(16384)
    ->Arg(65536)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond);

static void BM_SnapshotSince(benchmark::State& state) {
    Rings rings{static_cast<std::size_t>(state.range(0))};
    for (auto _ : state) {
        std::vector<hive::profiler::Event> events =
            hive::profiler::snapshot_all_threads_since(rings.frame_start);
        benchmark::DoNotOptimize(events.data());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_SnapshotSince)
    ->Arg(1024)
    ->Arg(16384)
    ->Arg(65536)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond);

static void BM_SampleTimings(benchmark::State& state) {
    Rings rings{static_cast<std::size_t>(state.range(0))};
    for (auto _ : state) {
        const std::vector<hive::profiler::Event> events = hive::profiler::snapshot_all_threads();
        const float acc = sample_nine(events, rings.frame_start);
        benchmark::DoNotOptimize(acc);
    }
}
BENCHMARK(BM_SampleTimings)
    ->Arg(1024)
    ->Arg(16384)
    ->Arg(65536)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond);

static void BM_SampleTimingsSince(benchmark::State& state) {
    Rings rings{static_cast<std::size_t>(state.range(0))};
    for (auto _ : state) {
        const std::vector<hive::profiler::Event> events =
            hive::profiler::snapshot_all_threads_since(rings.frame_start);
        const float acc = sample_nine(events, rings.frame_start);
        benchmark::DoNotOptimize(acc);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_SampleTimingsSince)
    ->Arg(1024)
    ->Arg(16384)
    ->Arg(65536)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond);
