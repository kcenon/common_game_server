#pragma once

/// @file combat_types.hpp
/// @brief Enumerations and constants for the combat system.
///
/// @see SDS-MOD-021
/// @see SRS-GML-002

#include <cstdint>

namespace cgs::game {

/// Maximum number of concurrent auras on a single entity.
constexpr std::size_t kMaxAuras = 32;

/// Maximum stacks for a single aura instance.
constexpr int32_t kMaxAuraStacks = 99;

/// Spell-casting state machine.
enum class CastState : uint8_t {
    Idle,        ///< Not casting anything.
    Casting,     ///< Cast bar progressing.
    Channeling,  ///< Channeled spell in progress.
    Complete,    ///< Cast finished successfully.
    Interrupted  ///< Cast was interrupted.
};

/// Damage type classification for resistance calculations.
enum class DamageType : uint8_t {
    Physical,  ///< Mitigated by armor.
    Magic,     ///< Mitigated by magic resistance.
    Fire,      ///< Fire elemental damage.
    Frost,     ///< Frost elemental damage.
    Nature,    ///< Nature elemental damage.
    Shadow,    ///< Shadow damage.
    Holy       ///< Holy damage.
};

/// Number of distinct damage types (for array sizing).
constexpr std::size_t kDamageTypeCount = 7;

}  // namespace cgs::game
