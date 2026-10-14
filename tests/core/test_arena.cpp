#include "core/arena.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <utility>

TEST_CASE("arena construct reports capacity and empty used") {
    hive::core::Arena arena(256);
    REQUIRE(arena.capacity() == 256);
    REQUIRE(arena.used() == 0);
}

TEST_CASE("arena allocate 16 returns non-null and accounts for used") {
    hive::core::Arena arena(256);
    void* p = arena.allocate(16);
    REQUIRE(p != nullptr);
    REQUIRE(arena.used() >= 16);
}

TEST_CASE("arena two allocations are distinct and in-order") {
    hive::core::Arena arena(256);
    auto* a = static_cast<std::byte*>(arena.allocate(16, 8));
    auto* b = static_cast<std::byte*>(arena.allocate(16, 8));
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(a != b);
    REQUIRE(b >= a + 16);
}

TEST_CASE("arena honors alignment") {
    hive::core::Arena arena(256);
    void* p8 = arena.allocate(8, 8);
    void* p16 = arena.allocate(8, 16);
    REQUIRE(p8 != nullptr);
    REQUIRE(p16 != nullptr);
    REQUIRE(reinterpret_cast<std::uintptr_t>(p8) % 8 == 0);
    REQUIRE(reinterpret_cast<std::uintptr_t>(p16) % 16 == 0);
}

TEST_CASE("arena allocate returns nullptr when full and does not bump used") {
    hive::core::Arena arena(64);
    std::size_t n = 0;
    while (arena.allocate(8, 8) != nullptr) {
        ++n;
        REQUIRE(n <= 64);
    }
    const std::size_t used = arena.used();
    REQUIRE(arena.allocate(8, 8) == nullptr);
    REQUIRE(arena.used() == used);
}

TEST_CASE("arena reset rewinds without releasing capacity") {
    hive::core::Arena arena(128);
    REQUIRE(arena.allocate(64) != nullptr);
    REQUIRE(arena.used() >= 64);
    arena.reset();
    REQUIRE(arena.used() == 0);
    REQUIRE(arena.capacity() == 128);
    REQUIRE(arena.allocate(64) != nullptr);
}

TEST_CASE("arena allocate of size 0 returns nullptr") {
    hive::core::Arena arena(128);
    REQUIRE(arena.allocate(0) == nullptr);
    REQUIRE(arena.used() == 0);
}

TEST_CASE("arena move transfers the buffer and empties the source") {
    hive::core::Arena src(128);
    REQUIRE(src.allocate(32, 8) != nullptr);
    const std::size_t used = src.used();

    hive::core::Arena dst(std::move(src));
    REQUIRE(dst.capacity() == 128);
    REQUIRE(dst.used() == used);
    REQUIRE(src.capacity() == 0);
    REQUIRE(src.allocate(8) == nullptr);
    REQUIRE(dst.allocate(8, 8) != nullptr);
}
