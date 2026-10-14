#include "jobs/scheduler.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <vector>

TEST_CASE("scheduler worker_count 0 becomes 1") {
    hive::jobs::Scheduler sched{0};
    REQUIRE(sched.worker_count() == 1);
}

TEST_CASE("parallel_for empty range is a no-op") {
    hive::jobs::Scheduler sched{2};
    hive::jobs::Counter done;
    sched.parallel_for(
        0, 0, 16,
        [&](std::size_t) {
            FAIL("must not run");
        },
        &done);
    sched.wait(done);
}

TEST_CASE("parallel_for grain larger than range runs inline") {
    hive::jobs::Scheduler sched{2};
    hive::jobs::Counter done;
    int sum = 0;
    sched.parallel_for(
        0, 5, 64,
        [&](std::size_t i) {
            sum += static_cast<int>(i);
        },
        &done);
    sched.wait(done);
    REQUIRE(sum == 10);
}

TEST_CASE("parallel_for fills every index") {
    hive::jobs::Scheduler sched{4};
    hive::jobs::Counter done;
    const std::size_t n = 10000;
    std::vector<int> out(n, 0);
    sched.parallel_for(
        0, n, 128,
        [&](std::size_t i) {
            out[i] = static_cast<int>(i);
        },
        &done);
    sched.wait(done);
    for (std::size_t i = 0; i < n; ++i) {
        REQUIRE(out[i] == static_cast<int>(i));
    }
}

TEST_CASE("parallel_for 1 thread vs N threads same result") {
    const std::size_t n = 20000;
    std::vector<int> one(n, 0);
    std::vector<int> many(n, 0);
    {
        hive::jobs::Scheduler sched{1};
        hive::jobs::Counter done;
        sched.parallel_for(
            0, n, 64,
            [&](std::size_t i) {
                one[i] = static_cast<int>(i * 3);
            },
            &done);
        sched.wait(done);
    }
    {
        hive::jobs::Scheduler sched{4};
        hive::jobs::Counter done;
        sched.parallel_for(
            0, n, 64,
            [&](std::size_t i) {
                many[i] = static_cast<int>(i * 3);
            },
            &done);
        sched.wait(done);
    }
    REQUIRE(one == many);
}

TEST_CASE("schedule with counter wait") {
    hive::jobs::Scheduler sched{4};
    hive::jobs::Counter done;
    std::atomic<int> hits{0};
    for (int i = 0; i < 64; ++i) {
        sched.schedule(
            [&] {
                hits.fetch_add(1, std::memory_order_relaxed);
            },
            &done);
    }
    sched.wait(done);
    REQUIRE(hits.load(std::memory_order_relaxed) == 64);
}

TEST_CASE("nested parallel_for inside a job") {
    hive::jobs::Scheduler sched{4};
    hive::jobs::Counter outer;
    std::atomic<int> hits{0};
    sched.schedule(
        [&] {
            hive::jobs::Counter inner;
            sched.parallel_for(
                0, 1000, 50,
                [&](std::size_t) {
                    hits.fetch_add(1, std::memory_order_relaxed);
                },
                &inner);
            sched.wait(inner);
        },
        &outer);
    sched.wait(outer);
    REQUIRE(hits.load(std::memory_order_relaxed) == 1000);
}

TEST_CASE("dependency tree three levels") {
    hive::jobs::Scheduler sched{4};
    hive::jobs::Counter top;
    std::atomic<int> leaves{0};
    constexpr int kBranch = 8;
    for (int i = 0; i < kBranch; ++i) {
        sched.schedule(
            [&] {
                hive::jobs::Counter mid;
                for (int j = 0; j < kBranch; ++j) {
                    sched.schedule(
                        [&] {
                            hive::jobs::Counter bot;
                            for (int k = 0; k < kBranch; ++k) {
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
    REQUIRE(leaves.load(std::memory_order_relaxed) == kBranch * kBranch * kBranch);
}

TEST_CASE("shutdown with idle workers does not hang") {
    hive::jobs::Scheduler sched{4};
    REQUIRE(sched.worker_count() == 4);
}

TEST_CASE("two sequential schedulers on the same thread") {
    std::atomic<int> hits{0};
    {
        hive::jobs::Scheduler sched{2};
        hive::jobs::Counter done;
        sched.schedule(
            [&] {
                hits.fetch_add(1, std::memory_order_relaxed);
            },
            &done);
        sched.wait(done);
    }
    {
        hive::jobs::Scheduler sched{2};
        hive::jobs::Counter done;
        sched.schedule(
            [&] {
                hits.fetch_add(1, std::memory_order_relaxed);
            },
            &done);
        sched.wait(done);
    }
    REQUIRE(hits.load(std::memory_order_relaxed) == 2);
}
