#pragma once

#include "ecs/entity.h"
#include "ecs/type.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace hive::ecs {

class Archetype;

inline constexpr std::size_t kChunkBytes = 16384;

class Chunk {
public:
    Chunk();
    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;
    Chunk(Chunk&&) = delete;
    Chunk& operator=(Chunk&&) = delete;
    ~Chunk() = default;

    [[nodiscard]] std::uint32_t count() const noexcept;
    [[nodiscard]] std::uint32_t capacity() const noexcept;
    [[nodiscard]] std::uint32_t archetype_index() const noexcept;

    void reset(std::uint32_t capacity, std::uint32_t archetype_index) noexcept;
    void set_count(std::uint32_t count) noexcept;

    [[nodiscard]] Entity* entities(const Archetype& archetype) noexcept;
    [[nodiscard]] std::byte* column_bytes(const Archetype& archetype,
                                          std::uint32_t type_id) noexcept;

    template <typename C> [[nodiscard]] auto column(const Archetype& archetype) noexcept;

private:
    std::uint32_t m_count = 0;
    std::uint32_t m_capacity = 0;
    std::uint32_t m_archetype = 0;
    std::unique_ptr<std::byte[]> m_storage;
};

struct ChunkRef {
    Chunk* chunk = nullptr;
    std::uint32_t index = 0;
};

template <typename C> auto Chunk::column(const Archetype& archetype) noexcept {
    using T = std::remove_cv_t<C>;
    T* ptr = reinterpret_cast<T*>(column_bytes(archetype, TypeId::of<C>()));
    if constexpr (std::is_const_v<C>) {
        return static_cast<const T*>(ptr);
    } else {
        return ptr;
    }
}

} // namespace hive::ecs
