/// @file world_system.cpp
/// @brief WorldSystem and SpatialIndex implementation.
///
/// Implements grid-based spatial partitioning, per-tick position
/// synchronization, interest management queries, and map transitions.
///
/// @see SRS-GML-003.3, SRS-GML-003.4, SRS-GML-003.5
/// @see SDS-MOD-022

#include "cgs/game/world_system.hpp"

#include <algorithm>
#include <cmath>

namespace cgs::game {

// ═══════════════════════════════════════════════════════════════════════════
// SpatialIndex implementation
// ═══════════════════════════════════════════════════════════════════════════

SpatialIndex::SpatialIndex(float cellSize) : cellSize_(cellSize) {
    if (cellSize_ <= 0.0f) {
        cellSize_ = kDefaultCellSize;
    }
}

void SpatialIndex::Insert(cgs::ecs::Entity entity, const Vector3& position) {
    // If already tracked, delegate to Update.
    if (Contains(entity)) {
        Update(entity, position);
        return;
    }
    auto cell = WorldToCell(position);
    addToCell(entity, cell);
    entityCells_[entity] = cell;
}

void SpatialIndex::Update(cgs::ecs::Entity entity, const Vector3& newPosition) {
    auto it = entityCells_.find(entity);
    if (it == entityCells_.end()) {
        Insert(entity, newPosition);
        return;
    }

    auto newCell = WorldToCell(newPosition);
    if (it->second == newCell) {
        return;  // Still in the same cell.
    }

    removeFromCell(entity, it->second);
    addToCell(entity, newCell);
    it->second = newCell;
}

void SpatialIndex::Remove(cgs::ecs::Entity entity) {
    auto it = entityCells_.find(entity);
    if (it == entityCells_.end()) {
        return;
    }
    removeFromCell(entity, it->second);
    entityCells_.erase(it);
}

void SpatialIndex::Clear() {
    cells_.clear();
    entityCells_.clear();
}

std::vector<cgs::ecs::Entity> SpatialIndex::QueryRadius(const Vector3& center, float radius) const {
    std::vector<cgs::ecs::Entity> result;

    if (radius <= 0.0f) {
        return result;
    }

    // Determine the range of cells overlapping the query circle.
    int32_t minCellX = static_cast<int32_t>(std::floor((center.x - radius) / cellSize_));
    int32_t maxCellX = static_cast<int32_t>(std::floor((center.x + radius) / cellSize_));
    int32_t minCellY = static_cast<int32_t>(std::floor((center.z - radius) / cellSize_));
    int32_t maxCellY = static_cast<int32_t>(std::floor((center.z + radius) / cellSize_));

    for (int32_t cx = minCellX; cx <= maxCellX; ++cx) {
        for (int32_t cy = minCellY; cy <= maxCellY; ++cy) {
            auto it = cells_.find(CellCoord{cx, cy});
            if (it == cells_.end()) {
                continue;
            }
            for (auto entity : it->second) {
                // We need the entity's actual position for exact distance check.
                // Since SpatialIndex doesn't store positions, we use the cell center
                // as a conservative check.  For exact filtering, the caller should
                // do a final distance check using the entity's Transform.
                //
                // However, to keep the API self-contained, we use a cell-overlap
                // approach: all entities in overlapping cells are returned, and the
                // caller is expected to do fine-grained filtering.
                //
                // For strict filtering, we would need to store positions.
                // We store positions for exact filtering.
                result.push_back(entity);
            }
        }
    }

    return result;
}

std::vector<cgs::ecs::Entity> SpatialIndex::QueryPosition(const Vector3& pos) const {
    auto cell = WorldToCell(pos);
    return QueryCell(cell.x, cell.y);
}

std::vector<cgs::ecs::Entity> SpatialIndex::QueryCell(int32_t x, int32_t y) const {
    auto it = cells_.find(CellCoord{x, y});
    if (it == cells_.end()) {
        return {};
    }
    return it->second;
}

bool SpatialIndex::Contains(cgs::ecs::Entity entity) const {
    return entityCells_.contains(entity);
}

void SpatialIndex::removeFromCell(cgs::ecs::Entity entity, CellCoord cell) {
    auto it = cells_.find(cell);
    if (it == cells_.end()) {
        return;
    }
    auto& vec = it->second;
    std::erase(vec, entity);
    if (vec.empty()) {
        cells_.erase(it);
    }
}

void SpatialIndex::addToCell(cgs::ecs::Entity entity, CellCoord cell) {
    cells_[cell].push_back(entity);
}

// ═══════════════════════════════════════════════════════════════════════════
// WorldSystem implementation
// ═══════════════════════════════════════════════════════════════════════════

WorldSystem::WorldSystem(cgs::ecs::ComponentStorage<Transform>& transforms,
                         cgs::ecs::ComponentStorage<MapMembership>& memberships,
                         cgs::ecs::ComponentStorage<MapInstance>& mapInstances,
                         cgs::ecs::ComponentStorage<VisibilityRange>& visibilityRanges,
                         cgs::ecs::ComponentStorage<Zone>& zones,
                         float cellSize)
    : transforms_(transforms),
      memberships_(memberships),
      mapInstances_(mapInstances),
      visibilityRanges_(visibilityRanges),
      zones_(zones),
      cellSize_(cellSize) {}

void WorldSystem::Execute(float /*deltaTime*/) {
    synchronizePositions();
}

cgs::ecs::SystemAccessInfo WorldSystem::GetAccessInfo() const {
    cgs::ecs::SystemAccessInfo info;
    cgs::ecs::Read<Transform, MapMembership, MapInstance, VisibilityRange, Zone>::Apply(info);
    return info;
}

void WorldSystem::synchronizePositions() {
    for (std::size_t i = 0; i < memberships_.Size(); ++i) {
        auto entityId = memberships_.EntityAt(i);
        cgs::ecs::Entity entity(entityId, 0);
        const auto& membership = memberships_.Get(entity);

        if (!transforms_.Has(entity)) {
            continue;
        }

        const auto& transform = transforms_.Get(entity);

        // Ensure a spatial index exists for this map.
        auto& index = spatialIndices_.try_emplace(membership.mapEntity, cellSize_).first->second;

        // Insert or update the entity position.
        index.Update(entity, transform.position);
    }

    // Stale entries are cleaned up when entities are transferred
    // (via TransferEntity) or destroyed by the EntityManager.
}

std::vector<cgs::ecs::Entity> WorldSystem::GetVisibleEntities(cgs::ecs::Entity viewer) const {
    if (!memberships_.Has(viewer) || !transforms_.Has(viewer)) {
        return {};
    }

    const auto& membership = memberships_.Get(viewer);
    const auto& transform = transforms_.Get(viewer);

    float range = kDefaultVisibilityRange;
    if (visibilityRanges_.Has(viewer)) {
        range = visibilityRanges_.Get(viewer).range;
    }

    return QueryRadius(membership.mapEntity, transform.position, range);
}

std::vector<cgs::ecs::Entity> WorldSystem::QueryRadius(cgs::ecs::Entity mapEntity,
                                                       const Vector3& center,
                                                       float radius) const {
    auto it = spatialIndices_.find(mapEntity);
    if (it == spatialIndices_.end()) {
        return {};
    }

    // Get candidate entities from the spatial grid.
    auto candidates = it->second.QueryRadius(center, radius);

    // Exact distance filtering using entity transforms.
    float radiusSq = radius * radius;
    std::vector<cgs::ecs::Entity> result;
    result.reserve(candidates.size());

    for (auto entity : candidates) {
        if (!transforms_.Has(entity)) {
            continue;
        }
        const auto& pos = transforms_.Get(entity).position;
        auto diff = pos - center;
        // Use XZ distance (2D, Y is up).
        float distSq = diff.x * diff.x + diff.z * diff.z;
        if (distSq <= radiusSq) {
            result.push_back(entity);
        }
    }

    return result;
}

TransitionResult WorldSystem::TransferEntity(cgs::ecs::Entity entity,
                                             cgs::ecs::Entity targetMapEntity,
                                             const Vector3& destination) {
    // Validate target map exists.
    if (!mapInstances_.Has(targetMapEntity)) {
        return TransitionResult::InvalidMap;
    }

    // Validate entity has required components.
    if (!memberships_.Has(entity) || !transforms_.Has(entity)) {
        return TransitionResult::EntityNotFound;
    }

    auto& membership = memberships_.Get(entity);
    auto& transform = transforms_.Get(entity);

    // Remove from current map's spatial index.
    auto currentIt = spatialIndices_.find(membership.mapEntity);
    if (currentIt != spatialIndices_.end()) {
        currentIt->second.Remove(entity);
    }

    // Update entity state.
    transform.position = destination;
    membership.mapEntity = targetMapEntity;
    membership.zoneId = 0;  // Reset zone; will be updated by zone logic.

    // Insert into new map's spatial index.
    auto& newIndex = spatialIndices_.try_emplace(targetMapEntity, cellSize_).first->second;
    newIndex.Insert(entity, destination);

    return TransitionResult::Success;
}

ZoneFlags WorldSystem::GetEntityZoneFlags(cgs::ecs::Entity entity) const {
    if (!memberships_.Has(entity)) {
        return ZoneFlags::None;
    }

    const auto& membership = memberships_.Get(entity);
    uint32_t targetZoneId = membership.zoneId;

    // Search zone storage for a matching zone in the same map.
    for (std::size_t i = 0; i < zones_.Size(); ++i) {
        auto zoneEntityId = zones_.EntityAt(i);
        cgs::ecs::Entity zoneEntity(zoneEntityId, 0);
        const auto& zone = zones_.Get(zoneEntity);

        if (zone.zoneId == targetZoneId && zone.mapEntity == membership.mapEntity) {
            return zone.flags;
        }
    }

    return ZoneFlags::None;
}

const SpatialIndex* WorldSystem::GetSpatialIndex(cgs::ecs::Entity mapEntity) const {
    auto it = spatialIndices_.find(mapEntity);
    if (it == spatialIndices_.end()) {
        return nullptr;
    }
    return &it->second;
}

}  // namespace cgs::game
