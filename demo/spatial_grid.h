#pragma once

#include "core/vec2.h"
#include "demo/world.h"

#include <cstdint>
#include <span>
#include <vector>

namespace hive::demo {

// Uniform grid over [0, kWorldWidth] x [0, kWorldHeight]. rebuild() is a packed
// two-pass (counts → prefix → scatter). for_neighbors walks the 9 adjacent
// cells, visits entity ids, and skips `self`. for_neighbors_packed walks the
// same cells but visits packed slots in indices() order so neighbor reads can
// be sequential. Positions passed to rebuild must stay alive until the next
// rebuild or clear — the grid does not copy them.
class SpatialGrid {
public:
    void clear();
    void rebuild(std::span<const hive::core::Vec2> positions, float cell);

    template <typename Fn>
    void for_neighbors(std::uint32_t self, hive::core::Vec2 p, float radius, Fn&& visit) const;

    // packed_pos[k] must be the position of indices()[k]. visit(slot, away, dist_sq)
    // where away = p - packed_pos[slot] (same as self_pos - neighbor_pos).
    template <typename Fn>
    void for_neighbors_packed(std::uint32_t self, hive::core::Vec2 p, float radius,
                              std::span<const hive::core::Vec2> packed_pos, Fn&& visit) const;

    [[nodiscard]] std::span<const std::uint32_t> indices() const noexcept;
    [[nodiscard]] std::uint32_t cell_count() const noexcept;
    [[nodiscard]] std::uint32_t occupant_count() const noexcept;
    [[nodiscard]] std::uint32_t cols() const noexcept;
    [[nodiscard]] std::uint32_t rows() const noexcept;
    [[nodiscard]] std::size_t index_capacity() const noexcept;

private:
    [[nodiscard]] std::uint32_t cell_at(hive::core::Vec2 p) const noexcept;
    void cell_coords(hive::core::Vec2 p, int& col, int& row) const noexcept;

    float m_cell = 1.0f;
    std::uint32_t m_cols = 0;
    std::uint32_t m_rows = 0;
    std::span<const hive::core::Vec2> m_positions{};
    std::vector<std::uint32_t> m_offsets;
    std::vector<std::uint32_t> m_indices;
    std::vector<std::uint32_t> m_counts;
    std::vector<std::uint32_t> m_cursor;
};

template <typename Fn>
void SpatialGrid::for_neighbors(std::uint32_t self, hive::core::Vec2 p, float radius,
                                Fn&& visit) const {
    if (m_cols == 0 || m_rows == 0) {
        return;
    }
    const std::size_t needed = static_cast<std::size_t>(m_cols) * m_rows + 1;
    if (m_offsets.size() < needed) {
        return;
    }

    const float r2 = radius * radius;
    int col = 0;
    int row = 0;
    cell_coords(p, col, row);

    const int cols = static_cast<int>(m_cols);
    const int rows = static_cast<int>(m_rows);
    for (int dy = -1; dy <= 1; ++dy) {
        const int y = row + dy;
        if (y < 0 || y >= rows) {
            continue;
        }
        for (int dx = -1; dx <= 1; ++dx) {
            const int x = col + dx;
            if (x < 0 || x >= cols) {
                continue;
            }
            const std::uint32_t cell =
                static_cast<std::uint32_t>(y) * m_cols + static_cast<std::uint32_t>(x);
            const std::uint32_t begin = m_offsets[cell];
            const std::uint32_t end = m_offsets[cell + 1];
            for (std::uint32_t k = begin; k < end; ++k) {
                const std::uint32_t idx = m_indices[k];
                if (idx == self || idx >= m_positions.size()) {
                    continue;
                }
                const hive::core::Vec2 delta = m_positions[idx] - p;
                if (hive::core::length_sq(delta) > r2) {
                    continue;
                }
                visit(idx);
            }
        }
    }
}

template <typename Fn>
void SpatialGrid::for_neighbors_packed(std::uint32_t self, hive::core::Vec2 p, float radius,
                                       std::span<const hive::core::Vec2> packed_pos,
                                       Fn&& visit) const {
    if (m_cols == 0 || m_rows == 0) {
        return;
    }
    const std::size_t needed = static_cast<std::size_t>(m_cols) * m_rows + 1;
    if (m_offsets.size() < needed) {
        return;
    }

    const float r2 = radius * radius;
    int col = 0;
    int row = 0;
    cell_coords(p, col, row);

    const int cols = static_cast<int>(m_cols);
    const int rows = static_cast<int>(m_rows);
    const std::uint32_t packed_n = static_cast<std::uint32_t>(packed_pos.size());
    for (int dy = -1; dy <= 1; ++dy) {
        const int y = row + dy;
        if (y < 0 || y >= rows) {
            continue;
        }
        for (int dx = -1; dx <= 1; ++dx) {
            const int x = col + dx;
            if (x < 0 || x >= cols) {
                continue;
            }
            const std::uint32_t cell =
                static_cast<std::uint32_t>(y) * m_cols + static_cast<std::uint32_t>(x);
            const std::uint32_t begin = m_offsets[cell];
            const std::uint32_t end = m_offsets[cell + 1];
            for (std::uint32_t k = begin; k < end; ++k) {
                if (k >= packed_n) {
                    continue;
                }
                const std::uint32_t idx = m_indices[k];
                if (idx == self) {
                    continue;
                }
                const hive::core::Vec2 away = p - packed_pos[k];
                const float dist_sq = hive::core::length_sq(away);
                if (dist_sq > r2) {
                    continue;
                }
                visit(k, away, dist_sq);
            }
        }
    }
}

} // namespace hive::demo
