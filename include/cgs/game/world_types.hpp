#pragma once

/// @file world_types.hpp
/// @brief Enumerations and constants for the world system.
///
/// @see SDS-MOD-022
/// @see SRS-GML-003

#include <cstdint>

namespace cgs::game {

/// Maximum number of zones a single map instance can contain.
constexpr std::size_t kMaxZonesPerMap = 256;

/// Default spatial grid cell size in world units.
constexpr float kDefaultCellSize = 32.0f;

/// Default visibility range for interest management (world units).
constexpr float kDefaultVisibilityRange = 100.0f;

/// Map instance type classification.
enum class MapType : uint8_t {
    OpenWorld,    ///< Persistent, shared world map.
    Dungeon,      ///< Instanced, group-based dungeon.
    Battleground  ///< PvP battleground instance.
};

/// Zone type classification for area-based rules.
enum class ZoneType : uint8_t {
    Normal,    ///< Standard gameplay area.
    PvP,       ///< Player-vs-player combat enabled.
    Safe,      ///< No combat allowed (sanctuary).
    Contested  ///< Faction-contested territory.
};

/// Bitfield flags for zone properties.
enum class ZoneFlags : uint32_t {
    None         = 0,
    NoCombat     = 1u << 0,  ///< Combat is disabled.
    NoMount      = 1u << 1,  ///< Mounts are not allowed.
    NoFly        = 1u << 2,  ///< Flying is not allowed.
    Sanctuary    = 1u << 3,  ///< Full protection zone.
    Resting      = 1u << 4,  ///< Grants rested XP bonus.
    FreeForAll   = 1u << 5,  ///< PvP with no faction rules.
    Indoor       = 1u << 6   ///< Interior area (no weather/sky).
};

/// Bitwise OR for ZoneFlags.
constexpr ZoneFlags operator|(ZoneFlags lhs, ZoneFlags rhs) noexcept {
    return static_cast<ZoneFlags>(
        static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

/// Bitwise AND for ZoneFlags.
constexpr ZoneFlags operator&(ZoneFlags lhs, ZoneFlags rhs) noexcept {
    return static_cast<ZoneFlags>(
        static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

/// Check whether a specific flag is set.
constexpr bool HasFlag(ZoneFlags flags, ZoneFlags flag) noexcept {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

/// Result of a map transition request.
enum class TransitionResult : uint8_t {
    Success,       ///< Transition completed successfully.
    InvalidMap,    ///< Target map does not exist.
    InvalidZone,   ///< Target zone does not exist on the map.
    EntityNotFound ///< Source entity not found in spatial index.
};

} // namespace cgs::game
