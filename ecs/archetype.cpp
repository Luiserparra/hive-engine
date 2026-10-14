#include "ecs/archetype.h"

#include "core/assert.h"

#include <cstring>
#include <utility>

namespace hive::ecs {
namespace {

[[nodiscard]] std::size_t bytes_for(std::uint32_t count,
                                    const std::vector<std::uint32_t>& type_ids) {
    std::size_t offset = count * sizeof(Entity);
    for (const std::uint32_t type_id : type_ids) {
        const TypeInfo info = TypeId::info(type_id);
        offset = align_up(offset, info.alignment);
        offset += static_cast<std::size_t>(count) * info.size;
    }
    return offset;
}

} // namespace

Archetype::Archetype(std::uint32_t index, ComponentMask mask, std::vector<std::uint32_t> type_ids,
                     std::vector<std::unique_ptr<Chunk>>* free_list)
    : m_index(index), m_mask(mask), m_type_ids(std::move(type_ids)), m_free_list(free_list) {
    HIVE_ASSERT(m_free_list != nullptr);
    compute_layout();
}

std::uint32_t Archetype::index() const noexcept {
    return m_index;
}

ComponentMask Archetype::mask() const noexcept {
    return m_mask;
}

std::uint32_t Archetype::capacity() const noexcept {
    return m_capacity;
}

std::uint32_t Archetype::entity_offset() const noexcept {
    return m_entity_offset;
}

bool Archetype::has_type(std::uint32_t type_id) const noexcept {
    HIVE_ASSERT(type_id < kMaxComponents);
    return (m_mask & (ComponentMask{1} << type_id)) != 0;
}

std::uint32_t Archetype::column_offset(std::uint32_t type_id) const noexcept {
    HIVE_ASSERT(has_type(type_id));
    return m_offset_by_id[type_id];
}

const std::vector<std::uint32_t>& Archetype::type_ids() const noexcept {
    return m_type_ids;
}

std::vector<std::unique_ptr<Chunk>>& Archetype::chunks() noexcept {
    return m_chunks;
}

const std::vector<std::unique_ptr<Chunk>>& Archetype::chunks() const noexcept {
    return m_chunks;
}

Chunk* Archetype::chunk_at(std::uint32_t index) noexcept {
    HIVE_ASSERT(index < m_chunks.size());
    HIVE_ASSERT(m_chunks[index] != nullptr);
    return m_chunks[index].get();
}

ChunkRef Archetype::writable() {
    for (std::uint32_t i = 0; i < m_chunks.size(); ++i) {
        Chunk* chunk = m_chunks[i].get();
        if (chunk != nullptr && chunk->count() < chunk->capacity()) {
            return ChunkRef{chunk, i};
        }
    }
    for (std::uint32_t i = 0; i < m_chunks.size(); ++i) {
        if (m_chunks[i] == nullptr) {
            m_chunks[i] = acquire_chunk();
            m_chunks[i]->reset(m_capacity, m_index);
            return ChunkRef{m_chunks[i].get(), i};
        }
    }
    m_chunks.push_back(acquire_chunk());
    const std::uint32_t index = static_cast<std::uint32_t>(m_chunks.size() - 1);
    m_chunks.back()->reset(m_capacity, m_index);
    return ChunkRef{m_chunks.back().get(), index};
}

void Archetype::remove_row(std::uint32_t chunk_index, std::uint32_t row,
                           std::vector<EntityRecord>& records) {
    Chunk& chunk = *chunk_at(chunk_index);
    HIVE_ASSERT(chunk.count() > 0);
    HIVE_ASSERT(row < chunk.count());

    const std::uint32_t last = chunk.count() - 1;
    if (row != last) {
        Entity* ents = chunk.entities(*this);
        const Entity moved = ents[last];
        ents[row] = moved;
        for (const std::uint32_t type_id : m_type_ids) {
            const std::uint16_t size = TypeId::info(type_id).size;
            std::byte* col = chunk.column_bytes(*this, type_id);
            std::memcpy(col + static_cast<std::size_t>(row) * size,
                        col + static_cast<std::size_t>(last) * size, size);
        }
        EntityRecord& rec = records[moved.index];
        rec.row = row;
        rec.chunk = chunk_index;
        rec.archetype = m_index;
    }
    chunk.set_count(last);
    if (chunk.count() == 0) {
        release_chunk(chunk_index);
    }
}

void Archetype::compute_layout() {
    m_entity_offset = 0;

    std::size_t min_stride = sizeof(Entity);
    for (const std::uint32_t type_id : m_type_ids) {
        min_stride += TypeId::info(type_id).size;
    }
    HIVE_ASSERT(min_stride > 0);
    std::uint32_t hi = static_cast<std::uint32_t>(kChunkBytes / min_stride);
    if (hi < 1) {
        hi = 1;
    }

    std::uint32_t best = 0;
    std::uint32_t lo = 1;
    while (lo <= hi) {
        const std::uint32_t mid = lo + (hi - lo) / 2;
        if (bytes_for(mid, m_type_ids) <= kChunkBytes) {
            best = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    HIVE_ASSERT(best >= 1);
    m_capacity = best;

    std::size_t offset = static_cast<std::size_t>(m_capacity) * sizeof(Entity);
    for (const std::uint32_t type_id : m_type_ids) {
        const TypeInfo info = TypeId::info(type_id);
        offset = align_up(offset, info.alignment);
        HIVE_ASSERT(type_id < kMaxComponents);
        m_offset_by_id[type_id] = static_cast<std::uint32_t>(offset);
        offset += static_cast<std::size_t>(m_capacity) * info.size;
    }
    HIVE_ASSERT(offset <= kChunkBytes);
}

std::unique_ptr<Chunk> Archetype::acquire_chunk() {
    if (!m_free_list->empty()) {
        std::unique_ptr<Chunk> chunk = std::move(m_free_list->back());
        m_free_list->pop_back();
        return chunk;
    }
    return std::make_unique<Chunk>();
}

void Archetype::release_chunk(std::uint32_t chunk_index) {
    HIVE_ASSERT(chunk_index < m_chunks.size());
    HIVE_ASSERT(m_chunks[chunk_index] != nullptr);
    m_free_list->push_back(std::move(m_chunks[chunk_index]));
}

} // namespace hive::ecs
