#pragma once

#include "ecs/chunk.h"
#include "ecs/entity.h"
#include "ecs/type.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace hive::ecs {

class Archetype {
public:
    Archetype(std::uint32_t index, ComponentMask mask, std::vector<std::uint32_t> type_ids,
              std::vector<std::unique_ptr<Chunk>>* free_list);

    Archetype(const Archetype&) = delete;
    Archetype& operator=(const Archetype&) = delete;
    Archetype(Archetype&&) = delete;
    Archetype& operator=(Archetype&&) = delete;
    ~Archetype() = default;

    [[nodiscard]] std::uint32_t index() const noexcept;
    [[nodiscard]] ComponentMask mask() const noexcept;
    [[nodiscard]] std::uint32_t capacity() const noexcept;
    [[nodiscard]] std::uint32_t entity_offset() const noexcept;
    [[nodiscard]] bool has_type(std::uint32_t type_id) const noexcept;
    [[nodiscard]] std::uint32_t column_offset(std::uint32_t type_id) const noexcept;
    [[nodiscard]] const std::vector<std::uint32_t>& type_ids() const noexcept;

    [[nodiscard]] std::vector<std::unique_ptr<Chunk>>& chunks() noexcept;
    [[nodiscard]] const std::vector<std::unique_ptr<Chunk>>& chunks() const noexcept;
    [[nodiscard]] Chunk* chunk_at(std::uint32_t index) noexcept;

    ChunkRef writable();
    void remove_row(std::uint32_t chunk_index, std::uint32_t row,
                    std::vector<EntityRecord>& records);

private:
    void compute_layout();
    std::unique_ptr<Chunk> acquire_chunk();
    void release_chunk(std::uint32_t chunk_index);

    std::uint32_t m_index = 0;
    ComponentMask m_mask = 0;
    std::uint32_t m_capacity = 0;
    std::uint32_t m_entity_offset = 0;
    std::vector<std::uint32_t> m_type_ids;
    std::array<std::uint32_t, kMaxComponents> m_offset_by_id{};
    std::vector<std::unique_ptr<Chunk>> m_chunks;
    std::vector<std::unique_ptr<Chunk>>* m_free_list = nullptr;
};

} // namespace hive::ecs
