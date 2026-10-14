#pragma once

#include "core/assert.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace hive::ecs {

inline constexpr std::uint32_t kMaxComponents = 64;

// POD-only: migrate is memcpy, destroy is a bump of generation. Empty tag types
// are out of scope for M3 — use a 1-byte enum instead.
template <typename C>
concept Component =
    std::is_trivially_copyable_v<std::remove_cv_t<C>> &&
    std::is_trivially_destructible_v<std::remove_cv_t<C>> && (sizeof(std::remove_cv_t<C>) > 0) &&
    (alignof(std::remove_cv_t<C>) <= alignof(std::max_align_t));

using ComponentMask = std::uint64_t;

struct TypeInfo {
    std::uint32_t id = 0;
    std::uint16_t size = 0;
    std::uint16_t alignment = 0;
};

class TypeId {
public:
    template <Component C> [[nodiscard]] static std::uint32_t of() noexcept {
        return id_for<std::remove_cv_t<C>>();
    }

    [[nodiscard]] static TypeInfo info(std::uint32_t id) noexcept;
    [[nodiscard]] static std::uint32_t count() noexcept;

private:
    // Static local is per instantiation: of<T> and of<const T> must share this.
    template <typename T> [[nodiscard]] static std::uint32_t id_for() noexcept {
        static const std::uint32_t id = register_type(sizeof(T), alignof(T));
        return id;
    }

    static std::uint32_t register_type(std::uint16_t size, std::uint16_t alignment) noexcept;
};

[[nodiscard]] inline std::size_t align_up(std::size_t value, std::size_t alignment) noexcept {
    HIVE_ASSERT(alignment > 0 && (alignment & (alignment - 1)) == 0);
    return (value + alignment - 1) & ~(alignment - 1);
}

template <typename... Cs> [[nodiscard]] inline ComponentMask component_mask() noexcept {
    static_assert(sizeof...(Cs) > 0, "component_mask requires at least one component");
    return ((ComponentMask{1} << TypeId::of<Cs>()) | ...);
}

} // namespace hive::ecs
