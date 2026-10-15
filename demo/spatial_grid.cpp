#include "demo/spatial_grid.h"

#include "core/assert.h"

#include <algorithm>
#include <cmath>

namespace hive::demo {
namespace {

std::uint32_t ceil_div_positive(float extent, float cell) noexcept {
    HIVE_ASSERT(cell > 0.0f);
    const float cells = extent / cell;
    auto count = static_cast<std::uint32_t>(std::ceil(cells));
    if (count == 0) {
        count = 1;
    }
    return count;
}

} // namespace

void SpatialGrid::cell_coords(hive::core::Vec2 p, int& col, int& row) const noexcept {
    if (m_cols == 0 || m_rows == 0) {
        col = 0;
        row = 0;
        return;
    }
    col = static_cast<int>(std::floor(p.x / m_cell));
    row = static_cast<int>(std::floor(p.y / m_cell));
    col = std::clamp(col, 0, static_cast<int>(m_cols) - 1);
    row = std::clamp(row, 0, static_cast<int>(m_rows) - 1);
}

std::uint32_t SpatialGrid::cell_at(hive::core::Vec2 p) const noexcept {
    int col = 0;
    int row = 0;
    cell_coords(p, col, row);
    return static_cast<std::uint32_t>(row) * m_cols + static_cast<std::uint32_t>(col);
}

void SpatialGrid::clear() {
    m_cell = 1.0f;
    m_cols = 0;
    m_rows = 0;
    m_positions = {};
    m_offsets.clear();
    m_indices.clear();
    m_counts.clear();
    m_cursor.clear();
}

void SpatialGrid::rebuild(std::span<const hive::core::Vec2> positions, float cell) {
    HIVE_ASSERT(cell > 0.0f);
    m_cell = cell;
    m_positions = positions;
    m_cols = ceil_div_positive(kWorldWidth, cell);
    m_rows = ceil_div_positive(kWorldHeight, cell);
    const std::uint32_t ncells = m_cols * m_rows;

    m_counts.assign(ncells, 0);
    for (const hive::core::Vec2& pos : positions) {
        m_counts[cell_at(pos)] += 1;
    }

    m_offsets.resize(static_cast<std::size_t>(ncells) + 1);
    m_offsets[0] = 0;
    for (std::uint32_t i = 0; i < ncells; ++i) {
        m_offsets[i + 1] = m_offsets[i] + m_counts[i];
    }

    m_indices.resize(positions.size());
    m_cursor = m_offsets;
    for (std::uint32_t i = 0; i < positions.size(); ++i) {
        const std::uint32_t cell_id = cell_at(positions[i]);
        m_indices[m_cursor[cell_id]] = i;
        m_cursor[cell_id] += 1;
    }
}

std::span<const std::uint32_t> SpatialGrid::indices() const noexcept {
    return m_indices;
}

std::uint32_t SpatialGrid::cell_count() const noexcept {
    return m_cols * m_rows;
}

std::uint32_t SpatialGrid::occupant_count() const noexcept {
    return static_cast<std::uint32_t>(m_indices.size());
}

std::uint32_t SpatialGrid::cols() const noexcept {
    return m_cols;
}

std::uint32_t SpatialGrid::rows() const noexcept {
    return m_rows;
}

std::size_t SpatialGrid::index_capacity() const noexcept {
    return m_indices.capacity();
}

} // namespace hive::demo
