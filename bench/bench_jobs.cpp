#include "bench/mutex_pool.h"
#include "jobs/scheduler.h"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

constexpr std::size_t kN = 1 << 20;
constexpr std::size_t kGrain = 1024;

void tiny_kernel(std::vector<std::int32_t>& data, std::size_t i) {
    data[i] += 1;
}

void medium_kernel(std::vector<std::int32_t>& data, std::size_t i) {
    std::int32_t v = data[i];
    for (int k = 0; k < 32; ++k) {
        v = v * 1664525 + 1013904223;
    }
    data[i] = v;
}

} // namespace

static void BM_WorkStealing_Tiny(benchmark::State& state) {
    const unsigned threads = static_cast<unsigned>(state.range(0));
    hive::jobs::Scheduler sched{threads};
    std::vector<std::int32_t> data(kN, 1);
    for (auto _ : state) {
        hive::jobs::Counter done;
        sched.parallel_for(
            0, kN, kGrain,
            [&](std::size_t i) {
                tiny_kernel(data, i);
            },
            &done);
        sched.wait(done);
        benchmark::DoNotOptimize(data.data());
    }
}
BENCHMARK(BM_WorkStealing_Tiny)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);

static void BM_MutexQueue_Tiny(benchmark::State& state) {
    const unsigned threads = static_cast<unsigned>(state.range(0));
    hive::bench::MutexPool pool{threads};
    std::vector<std::int32_t> data(kN, 1);
    for (auto _ : state) {
        pool.parallel_for(0, kN, kGrain, [&](std::size_t i) {
            tiny_kernel(data, i);
        });
        benchmark::DoNotOptimize(data.data());
    }
}
BENCHMARK(BM_MutexQueue_Tiny)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);

static void BM_WorkStealing_Medium(benchmark::State& state) {
    const unsigned threads = static_cast<unsigned>(state.range(0));
    hive::jobs::Scheduler sched{threads};
    std::vector<std::int32_t> data(kN, 1);
    for (auto _ : state) {
        hive::jobs::Counter done;
        sched.parallel_for(
            0, kN, kGrain,
            [&](std::size_t i) {
                medium_kernel(data, i);
            },
            &done);
        sched.wait(done);
        benchmark::DoNotOptimize(data.data());
    }
}
BENCHMARK(BM_WorkStealing_Medium)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);

static void BM_MutexQueue_Medium(benchmark::State& state) {
    const unsigned threads = static_cast<unsigned>(state.range(0));
    hive::bench::MutexPool pool{threads};
    std::vector<std::int32_t> data(kN, 1);
    for (auto _ : state) {
        pool.parallel_for(0, kN, kGrain, [&](std::size_t i) {
            medium_kernel(data, i);
        });
        benchmark::DoNotOptimize(data.data());
    }
}
BENCHMARK(BM_MutexQueue_Medium)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);
