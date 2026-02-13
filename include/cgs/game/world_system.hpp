#pragma once

/// @file world_system.hpp
/// @brief WorldSystem: per-tick world management.
///
/// Maintains spatial indices for each map instance, updates entity
/// positions in the index, performs interest management queries,
/// and handles map transitions.
///
/// @see SRS-GML-003.3, SRS-GML-003.4, SRS-GML-003.5
/// @see SDS-MOD-022

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/entity.hpp"
#include "cgs/ecs/system_scheduler.hpp"
#include "cgs/game/components.hpp"
#include "cgs/game/spatial_index.hpp"
#include "cgs/game/world_components.hpp"

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace cgs::game {

/// System that manages world spatial indexing, interest management,
/// and map transitions.
///
/// Execution each tick:
///   1. Synchronize entity positions into per-map spatial indices.
///   2. (Interest management queries are available via GetVisibleEntities.)
///
/// Map transitions are performed on-demand via TransferEntity().
class WorldSystem final : public cgs::ecs::ISystem {
public:
    WorldSystem(cgs::ecs::ComponentStorage<Transform>& transforms,
                cgs::ecs::ComponentStorage<MapMembership>& memberships,
                cgs::ecs::ComponentStorage<MapInstance>& mapInstances,
                cgs::ecs::ComponentStorage<VisibilityRange>& visibilityRanges,
                cgs::ecs::ComponentStorage<Zone>& zones,
                float cellSize = kDefaultCellSize);

    void Execute(float deltaTime) override;

    [[nodiscard]] cgs::ecs::SystemStage GetStage() const override {
        return cgs::ecs::SystemStage::PreUpdate;
    }

    [[nodiscard]] std::string_view GetName() const override { return "WorldSystem"; }

    [[nodiscard]] cgs::ecs::SystemAccessInfo GetAccessInfo() const override;

    // -- Spatial queries ------------------------------------------------

    /// Return all entities visible to @p viewer (within its visibility
    /// range, in the same map instance).
    ///
    /// Returns an empty vector if the viewer has no Transform or
    /// MapMembership.
    [[nodiscard]] std::vector<cgs::ecs::Entity> GetVisibleEntities(cgs::ecs::Entity viewer) const;

    /// Return all entities in a given radius from a world position
    /// within a specific map instance entity.
    [[nodiscard]] std::vector<cgs::ecs::Entity> QueryRadius(cgs::ecs::Entity mapEntity,
                                                            const Vector3& center,
                                                            float radius) const;

    // -- Map transitions (SRS-GML-003.5) --------------------------------

    /// Transfer an entity from its current map to a new map instance
    /// at the given destination position.
    ///
    /// The entity's Transform, MapMembership, and spatial index entries
    /// are updated atomically.
    ///
    /// @return TransitionResult indicating success or failure reason.
    [[nodiscard]] TransitionResult TransferEntity(cgs::ecs::Entity entity,
                                                  cgs::ecs::Entity targetMapEntity,
                                                  const Vector3& destination);

    // -- Accessors ------------------------------------------------------

    /// Get the spatial index for a given map instance entity.
    ///
    /// @return Pointer to the index, or nullptr if the map has no index.
    [[nodiscard]] const SpatialIndex* GetSpatialIndex(cgs::ecs::Entity mapEntity) const;

    /// Number of tracked map spatial indices.
    [[nodiscard]] std::size_t MapCount() const noexcept { return spatialIndices_.size(); }

    // -- Zone queries ---------------------------------------------------

    /// Get the zone flags for an entity's current zone.
    ///
    /// Looks up the entity's MapMembership to find its zoneId,
    /// then searches for a matching Zone component.
    /// Returns ZoneFlags::None if no zone is found.
    [[nodiscard]] ZoneFlags GetEntityZoneFlags(cgs::ecs::Entity entity) const;

private:
    /// Synchronize all entities with MapMembership into their
    /// respective spatial indices.
    void synchronizePositions();

    cgs::ecs::ComponentStorage<Transform>& transforms_;
    cgs::ecs::ComponentStorage<MapMembership>& memberships_;
    cgs::ecs::ComponentStorage<MapInstance>& mapInstances_;
    cgs::ecs::ComponentStorage<VisibilityRange>& visibilityRanges_;
    cgs::ecs::ComponentStorage<Zone>& zones_;

    /// Per-map-entity spatial index.
    std::unordered_map<cgs::ecs::Entity, SpatialIndex> spatialIndices_;

    float cellSize_;
};

}  // namespace cgs::game
