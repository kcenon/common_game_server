#pragma once

/// @file world_components.hpp
/// @brief World ECS components: MapInstance, Zone, MapMembership, VisibilityRange.
///
/// Each struct is a plain data component designed for sparse-set storage
/// via ComponentStorage<T>.  The WorldSystem operates on these components
/// to manage maps, zones, spatial queries, and interest management.
///
/// @see SRS-GML-003.1 .. SRS-GML-003.5
/// @see SDS-MOD-022

#include <cstdint>

#include "cgs/ecs/entity.hpp"
#include "cgs/game/world_types.hpp"

namespace cgs::game {

// -- MapInstance (SRS-GML-003.1) ---------------------------------------------

/// Represents a map instance in the world.
///
/// Attached to a "map entity" that serves as the logical anchor for the
/// map.  Multiple instances of the same mapId can coexist (e.g. dungeon
/// instances).
struct MapInstance {
    uint32_t mapId = 0;
    uint32_t instanceId = 0;
    MapType type = MapType::OpenWorld;
};

// -- Zone (SRS-GML-003.2) ---------------------------------------------------

/// Represents a zone (area) within a map instance.
///
/// Zones define area-based rules (PvP, Safe, etc.) and carry property
/// flags that systems can query for gameplay logic.
struct Zone {
    uint32_t zoneId = 0;
    ZoneType type = ZoneType::Normal;
    ZoneFlags flags = ZoneFlags::None;
    cgs::ecs::Entity mapEntity;  ///< Map instance this zone belongs to.
};

// -- MapMembership -----------------------------------------------------------

/// Tags an entity (player, creature, etc.) as belonging to a specific
/// map instance.  The WorldSystem uses this to decide which spatial
/// index to track the entity in.
struct MapMembership {
    cgs::ecs::Entity mapEntity;  ///< The map instance entity.
    uint32_t zoneId = 0;         ///< Current zone within the map.
};

// -- VisibilityRange (SRS-GML-003.4) ----------------------------------------

/// Per-entity visibility radius for interest management.
///
/// Only entities within this radius of a "viewer" entity receive
/// update packets.  If this component is absent the system uses
/// kDefaultVisibilityRange.
struct VisibilityRange {
    float range = kDefaultVisibilityRange;
};

} // namespace cgs::game
