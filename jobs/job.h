#pragma once

#include <cstddef>
#include <cstdint>
#include <new>

namespace hive::jobs {

class Counter;

// One job: function pointer + inlined lambda. alignas(64) so adjacent jobs in
// the pool do not false-share. sizeof is 64 — no heap, no std::function.
// next_free overlays payload: the free list is only used when the job is dead.
struct alignas(64) Job {
    static constexpr std::size_t kPayloadBytes = 40;

    void (*execute)(Job*) = nullptr;
    Counter* counter = nullptr;
    std::uint16_t owner = 0;
    union {
        Job* next_free;
        alignas(std::max_align_t) std::byte payload[kPayloadBytes];
    };
};

static_assert(sizeof(Job) == 64, "Job must occupy exactly one cache line");
static_assert(alignof(Job) == 64, "Job must be cache-line aligned");

} // namespace hive::jobs
