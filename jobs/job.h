#pragma once

#include "core/cache_line.h"

#include <cstddef>
#include <cstdint>
#include <new>

namespace hive::jobs {

class Counter;

// One job: function pointer + inlined lambda. One cache line so adjacent jobs
// in the pool do not false-share. No heap, no std::function.
// next_free overlays payload: the free list is only used when the job is dead.
//
// Payload alignment is 8, not alignof(std::max_align_t). On libstdc++ that
// alignment is 16, which inserts padding so a 40-byte payload starts at
// offset 32 and sizeof(Job) becomes 128. Lambdas stricter than 8 are rejected
// in schedule().
struct alignas(hive::core::kCacheLineBytes) Job {
    static constexpr std::size_t kPayloadBytes = 40;
    static constexpr std::size_t kPayloadAlign = 8;

    void (*execute)(Job*) = nullptr;
    Counter* counter = nullptr;
    std::uint16_t owner = 0;
    union {
        Job* next_free;
        alignas(kPayloadAlign) std::byte payload[kPayloadBytes];
    };
};

static_assert(sizeof(Job) == hive::core::kCacheLineBytes, "Job must occupy exactly one cache line");
static_assert(alignof(Job) == hive::core::kCacheLineBytes, "Job must be cache-line aligned");

} // namespace hive::jobs
