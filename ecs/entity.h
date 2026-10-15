#pragma once

#include <cstdint>

namespace hive::ecs {

inline constexpr std::uint32_t kInvalidArchetype = 0xFFFFFFFFu;

// Generational handle. index 0 is null. generation is bumped on destroy so a
// stale copy no longer matches the record.
struct Entity {
    std::uint32_t index = 0;
    std::uint32_t generation = 0;

    [[nodiscard]] constexpr bool operator==(Entity other) const noexcept {
        return index == other.index && generation == other.generation;
    }

    [[nodiscard]] constexpr bool operator!=(Entity other) const noexcept {
        return !(*this == other);
    }

    [[nodiscard]] constexpr bool is_null() const noexcept {
        return index == 0;
    }
};

// Sparse slot keyed by Entity.index. archetype == kInvalidArchetype means the
// id is reserved (CommandBuffer::create) or free, not visible to queries.
struct EntityRecord {
    std::uint32_t generation = 0;
    std::uint32_t archetype = kInvalidArchetype;
    std::uint32_t chunk = 0;
    std::uint32_t row = 0;
};

} // namespace hive::ecs
