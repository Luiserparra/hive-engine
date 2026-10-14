#include "ecs/type.h"

#include "core/assert.h"

#include <array>
#include <atomic>

namespace hive::ecs {
namespace {

std::atomic<std::uint32_t> g_next_id{0};
std::array<TypeInfo, kMaxComponents> g_infos{};

} // namespace

std::uint32_t TypeId::register_type(std::uint16_t size, std::uint16_t alignment) noexcept {
    const std::uint32_t id = g_next_id.fetch_add(1, std::memory_order_relaxed);
    HIVE_ASSERT(id < kMaxComponents);
    HIVE_ASSERT(size > 0);
    HIVE_ASSERT(alignment > 0);
    g_infos[id] = TypeInfo{id, size, alignment};
    return id;
}

TypeInfo TypeId::info(std::uint32_t id) noexcept {
    HIVE_ASSERT(id < g_next_id.load(std::memory_order_relaxed));
    return g_infos[id];
}

std::uint32_t TypeId::count() noexcept {
    return g_next_id.load(std::memory_order_relaxed);
}

} // namespace hive::ecs
