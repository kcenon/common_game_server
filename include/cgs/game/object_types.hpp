#pragma once

/// @file object_types.hpp
/// @brief Enumerations and constants for the game object system.
///
/// @see SDS-MOD-020
/// @see SRS-GML-001

#include <cstdint>

namespace cgs::game {

/// Maximum number of numeric attributes in the Stats component.
constexpr std::size_t kMaxAttributes = 16;

/// Classification of game world objects.
enum class ObjectType : uint8_t {
    Player,     ///< Player-controlled character.
    Creature,   ///< AI-controlled NPC or monster.
    GameObject  ///< Static or interactive world object.
};

/// Movement state machine states.
enum class MovementState : uint8_t {
    Idle,     ///< Standing still.
    Walking,  ///< Moving at walk speed.
    Running,  ///< Moving at run speed.
    Falling   ///< Airborne / falling.
};

}  // namespace cgs::game
