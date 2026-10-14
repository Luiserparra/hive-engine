#include "jobs/deque.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

TEST_CASE("deque empty pop and steal return nullptr") {
    hive::jobs::WorkStealingDeque deque;
    REQUIRE(deque.pop() == nullptr);
    REQUIRE(deque.steal() == nullptr);
    REQUIRE(deque.empty());
}

TEST_CASE("deque push then pop is LIFO") {
    hive::jobs::WorkStealingDeque deque;
    int a = 1;
    int b = 2;
    int c = 3;
    deque.push(&a);
    deque.push(&b);
    deque.push(&c);
    REQUIRE(deque.pop() == &c);
    REQUIRE(deque.pop() == &b);
    REQUIRE(deque.pop() == &a);
    REQUIRE(deque.pop() == nullptr);
}

TEST_CASE("deque steal takes the oldest item") {
    hive::jobs::WorkStealingDeque deque;
    int a = 1;
    int b = 2;
    int c = 3;
    deque.push(&a);
    deque.push(&b);
    deque.push(&c);
    REQUIRE(deque.steal() == &a);
    REQUIRE(deque.pop() == &c);
    REQUIRE(deque.pop() == &b);
    REQUIRE(deque.pop() == nullptr);
}

TEST_CASE("deque last item: pop vs steal exactly one winner") {
    hive::jobs::WorkStealingDeque deque;
    int item = 7;
    constexpr int kIters = 10000;
    int taken = 0;
    for (int i = 0; i < kIters; ++i) {
        deque.push(&item);
        std::atomic<int> ready{0};
        std::atomic<int> winners{0};
        std::thread thief([&] {
            ready.fetch_add(1, std::memory_order_acq_rel);
            while (ready.load(std::memory_order_acquire) < 2) {
            }
            if (deque.steal() != nullptr) {
                winners.fetch_add(1, std::memory_order_relaxed);
            }
        });
        ready.fetch_add(1, std::memory_order_acq_rel);
        while (ready.load(std::memory_order_acquire) < 2) {
        }
        if (deque.pop() != nullptr) {
            winners.fetch_add(1, std::memory_order_relaxed);
        }
        thief.join();
        REQUIRE(winners.load(std::memory_order_relaxed) == 1);
        REQUIRE(deque.pop() == nullptr);
        REQUIRE(deque.steal() == nullptr);
        ++taken;
    }
    REQUIRE(taken == kIters);
}

TEST_CASE("deque single producer many thieves drain every item") {
    hive::jobs::WorkStealingDeque deque;
    constexpr int kCount = 50000;
    std::atomic<int> remaining{kCount};
    std::atomic<bool> producing{true};

    auto steal_loop = [&] {
        while (producing.load(std::memory_order_acquire) || !deque.empty()) {
            if (deque.steal() != nullptr) {
                remaining.fetch_sub(1, std::memory_order_relaxed);
            }
        }
    };

    std::thread t1(steal_loop);
    std::thread t2(steal_loop);
    std::thread t3(steal_loop);

    for (int i = 0; i < kCount; ++i) {
        deque.push(reinterpret_cast<void*>(static_cast<std::uintptr_t>(i + 1)));
        if (void* p = deque.pop()) {
            (void)p;
            remaining.fetch_sub(1, std::memory_order_relaxed);
        }
    }
    producing.store(false, std::memory_order_release);

    t1.join();
    t2.join();
    t3.join();

    while (deque.pop() != nullptr) {
        remaining.fetch_sub(1, std::memory_order_relaxed);
    }
    while (deque.steal() != nullptr) {
        remaining.fetch_sub(1, std::memory_order_relaxed);
    }

    REQUIRE(remaining.load(std::memory_order_relaxed) == 0);
}
