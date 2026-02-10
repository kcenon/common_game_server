#pragma once

/// @file spatial_index.hpp
/// @brief Grid-based spatial partitioning index.
///
/// SpatialIndex divides 2D world space into uniform cells and provides
/// efficient O(1) insert/update/remove and radius-based nearest-neighbor
/// queries.  Only the X and Z coordinates are used (Y is "up").
///
/// @see SRS-GML-003.3
/// @see SDS-MOD-022

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cgs/ecs/entity.hpp"
#include "cgs/game/math_types.hpp"
#include "cgs/game/world_types.hpp"

namespace cgs::game {

/// Grid cell coordinate.
struct CellCoord {
    int32_t x = 0;
    int32_t y = 0;

    constexpr auto operator<=>(const CellCoord&) const = default;
};

} // namespace cgs::game

/// Hash support for CellCoord.
template <>
struct std::hash<cgs::game::CellCoord> {
    std::size_t operator()(const cgs::game::CellCoord& c) const noexcept {
        // Combine x and y hashes using a simple mixing function.
        auto h1 = std::hash<int32_t>{}(c.x);
        auto h2 = std::hash<int32_t>{}(c.y);
        return h1 ^ (h2 * 2654435761u);
    }
};

namespace cgs::game {

/// Grid-based spatial index for efficient entity queries.
///
/// Entities are placed into cells based on their X/Z world position.
/// The grid is sparse (only occupied cells are stored), making it
/// memory-efficient for large, mostly-empty worlds.
///
/// Thread safety: None.  External synchronization is required if
/// accessed from multiple threads.
class SpatialIndex {
public:
    /// Construct with a given cell size (world units per cell edge).
    explicit SpatialIndex(float cellSize = kDefaultCellSize);

    // -- Mutation -------------------------------------------------------

    /// Insert an entity at the given world position.
    ///
    /// If the entity is already tracked, this is equivalent to Update().
    void Insert(cgs::ecs::Entity entity, const Vector3& position);

    /// Update an entity's position in the index.
    ///
    /// If the entity has moved to a different cell, it is re-assigned.
    /// If the entity is not tracked, this is equivalent to Insert().
    void Update(cgs::ecs::Entity entity, const Vector3& newPosition);

    /// Remove an entity from the index.
    ///
    /// No-op if the entity is not currently tracked.
    void Remove(cgs::ecs::Entity entity);

    /// Remove all tracked entities.
    void Clear();

    // -- Queries --------------------------------------------------------

    /// Return all entities within @p radius world units of @p center.
    ///
    /// Uses the grid to limit the search to cells that overlap the
    /// query circle, then applies exact distance filtering.
    [[nodiscard]] std::vector<cgs::ecs::Entity>
    QueryRadius(const Vector3& center, float radius) const;

    /// Return all entities in the cell that contains world position @p pos.
    [[nodiscard]] std::vector<cgs::ecs::Entity>
    QueryPosition(const Vector3& pos) const;

    /// Return all entities in the cell at grid coordinate (x, y).
    [[nodiscard]] std::vector<cgs::ecs::Entity>
    QueryCell(int32_t x, int32_t y) const;

    // -- Accessors ------------------------------------------------------

    /// Number of tracked entities.
    [[nodiscard]] std::size_t Size() const noexcept { return entityCells_.size(); }

    /// Current cell size.
    [[nodiscard]] float CellSize() const noexcept { return cellSize_; }

    /// Check whether an entity is tracked.
    [[nodiscard]] bool Contains(cgs::ecs::Entity entity) const;

    /// Get the cell coordinate for a world position.
    [[nodiscard]] CellCoord WorldToCell(const Vector3& pos) const noexcept {
        return {
            static_cast<int32_t>(std::floor(pos.x / cellSize_)),
            static_cast<int32_t>(std::floor(pos.z / cellSize_))
        };
    }

private:
    /// Remove entity from its current cell (internal helper).
    void removeFromCell(cgs::ecs::Entity entity, CellCoord cell);

    /// Add entity to a cell (internal helper).
    void addToCell(cgs::ecs::Entity entity, CellCoord cell);

    float cellSize_;

    /// cell coord -> list of entities in that cell.
    std::unordered_map<CellCoord, std::vector<cgs::ecs::Entity>> cells_;

    /// entity -> current cell coord (for fast lookup on Update/Remove).
    std::unordered_map<cgs::ecs::Entity, CellCoord> entityCells_;
};

} // namespace cgs::game
