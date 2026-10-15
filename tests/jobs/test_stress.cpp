#include "jobs/scheduler.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

#ifndef __has_feature
#define __has_feature(x) 0
#endif

// One million jobs does not finish inside ctest's 180s under TSan. A few tens
// of thousands still cross threads often enough to catch a lost increment.
#if defined(__SANITIZE_THREAD__) || __has_feature(thread_sanitizer)
constexpr int kIncrementJobs = 20'000;
#else
constexpr int kIncrementJobs = 1'000'000;
#endif

TEST_CASE("stress one million increment jobs", "[stress]") {
    const unsigned threads = std::max(2u, std::thread::hardware_concurrency());
    hive::jobs::Scheduler sched{threads};
    hive::jobs::Counter done;
    std::atomic<int> count{0};
    constexpr int kJobs = kIncrementJobs;
    for (int i = 0; i < kJobs; ++i) {
        sched.schedule(
            [&] {
                count.fetch_add(1, std::memory_order_relaxed);
            },
            &done);
    }
    sched.wait(done);
    REQUIRE(count.load(std::memory_order_relaxed) == kJobs);
}

TEST_CASE("stress parallel_for million indices", "[stress]") {
    const unsigned threads = std::max(2u, std::thread::hardware_concurrency());
    hive::jobs::Scheduler sched{threads};
    hive::jobs::Counter done;
    constexpr std::size_t kN = 1'000'000;
    std::vector<std::uint8_t> flags(kN, 0);
    sched.parallel_for(
        0, kN, 1024,
        [&](std::size_t i) {
            flags[i] = 1;
        },
        &done);
    sched.wait(done);
    std::size_t sum = 0;
    for (std::uint8_t f : flags) {
        sum += f;
    }
    REQUIRE(sum == kN);
}

TEST_CASE("stress dependency tree under load", "[stress]") {
    const unsigned threads = std::max(2u, std::thread::hardware_concurrency());
    hive::jobs::Scheduler sched{threads};
    hive::jobs::Counter top;
    std::atomic<int> leaves{0};
    constexpr int kOuter = 32;
    constexpr int kMid = 16;
    constexpr int kInner = 8;
    for (int i = 0; i < kOuter; ++i) {
        sched.schedule(
            [&] {
                hive::jobs::Counter mid;
                for (int j = 0; j < kMid; ++j) {
                    sched.schedule(
                        [&] {
                            hive::jobs::Counter bot;
                            for (int k = 0; k < kInner; ++k) {
                                sched.schedule(
                                    [&] {
                                        leaves.fetch_add(1, std::memory_order_relaxed);
                                    },
                                    &bot);
                            }
                            sched.wait(bot);
                        },
                        &mid);
                }
                sched.wait(mid);
            },
            &top);
    }
    sched.wait(top);
    REQUIRE(leaves.load(std::memory_order_relaxed) == kOuter * kMid * kInner);
}
