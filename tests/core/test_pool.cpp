#include "core/pool.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

TEST_CASE("pool construct of 4 blocks then fifth allocate is nullptr") {
    hive::core::Pool pool(32, 4);
    REQUIRE(pool.allocate() != nullptr);
    REQUIRE(pool.allocate() != nullptr);
    REQUIRE(pool.allocate() != nullptr);
    REQUIRE(pool.allocate() != nullptr);
    REQUIRE(pool.allocate() == nullptr);
}

TEST_CASE("pool deallocate is LIFO: next allocate returns the same address") {
    hive::core::Pool pool(32, 4);
    void* a = pool.allocate();
    void* b = pool.allocate();
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(a != b);
    pool.deallocate(b);
    void* c = pool.allocate();
    REQUIRE(c == b);
}

TEST_CASE("pool block_size is at least the requested size") {
    hive::core::Pool pool(32, 2);
    REQUIRE(pool.block_size() >= 32);
    void* p = pool.allocate();
    REQUIRE(p != nullptr);
    REQUIRE(reinterpret_cast<std::uintptr_t>(p) % alignof(void*) == 0);
}

TEST_CASE("pool allocated_count tracks live blocks") {
    hive::core::Pool pool(32, 3);
    REQUIRE(pool.allocated_count() == 0);
    void* a = pool.allocate();
    void* b = pool.allocate();
    REQUIRE(pool.allocated_count() == 2);
    pool.deallocate(a);
    REQUIRE(pool.allocated_count() == 1);
    pool.deallocate(b);
    REQUIRE(pool.allocated_count() == 0);
}

TEST_CASE("pool tiny block_size still yields distinct slots") {
    hive::core::Pool pool(1, 3);
    void* a = pool.allocate();
    void* b = pool.allocate();
    void* c = pool.allocate();
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);
    REQUIRE(a != b);
    REQUIRE(b != c);
    REQUIRE(a != c);
    REQUIRE(pool.allocate() == nullptr);
}
