#pragma once

/// @file components.hpp
/// @brief Game object ECS components: Transform, Identity, Stats, Movement.
///
/// Each struct is a plain data component designed for sparse-set storage
/// via ComponentStorage<T>.  Components are kept small and focused on a
/// single concern to maximize cache utilization during system iteration.
///
/// @see SRS-GML-001.1 .. SRS-GML-001.4
/// @see SDS-MOD-020

#include "cgs/game/math_types.hpp"
#include "cgs/game/object_types.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <string>

namespace cgs::game {

// ── Transform (SRS-GML-001.1) ───────────────────────────────────────────

/// Spatial transform: position, rotation, and scale in world space.
struct Transform {
    Vector3 position;
    Quaternion rotation;
    Vector3 scale{1.0f, 1.0f, 1.0f};
};

// ── Identity (SRS-GML-001.2) ────────────────────────────────────────────

/// Globally unique ID type for game objects.
using GUID = uint64_t;

/// Generate a process-unique GUID.
///
/// Uses an atomic counter so it is safe to call from any thread.
/// Values start at 1; 0 is reserved as the invalid/null GUID.
inline GUID GenerateGUID() noexcept {
    static std::atomic<uint64_t> counter{0};
    return counter.fetch_add(1, std::memory_order_relaxed) + 1;
}

/// Sentinel GUID representing "no object".
constexpr GUID kInvalidGUID = 0;

/// Object identity: globally unique ID, display name, classification,
/// and template entry ID.
struct Identity {
    GUID guid = kInvalidGUID;
    std::string name;
    ObjectType type = ObjectType::GameObject;
    uint32_t entry = 0;  ///< Template / prototype ID.
};

// ── Stats (SRS-GML-001.3) ───────────────────────────────────────────────

/// Numeric stats: health, mana, and a fixed-size attribute array.
///
/// Setter helpers clamp values to [0, max] to enforce invariants.
struct Stats {
    int32_t health = 0;
    int32_t maxHealth = 0;
    int32_t mana = 0;
    int32_t maxMana = 0;
    std::array<int32_t, kMaxAttributes> attributes{};

    /// Set health, clamping to [0, maxHealth].
    void SetHealth(int32_t value) noexcept {
        health = std::clamp(value, static_cast<int32_t>(0), maxHealth);
    }

    /// Set mana, clamping to [0, maxMana].
    void SetMana(int32_t value) noexcept {
        mana = std::clamp(value, static_cast<int32_t>(0), maxMana);
    }
};

// ── Movement (SRS-GML-001.4) ────────────────────────────────────────────

/// Movement dynamics: speed, direction, and state.
///
/// `speed` is the effective speed after modifiers.
/// `baseSpeed` is the unmodified speed value.
struct Movement {
    float speed = 0.0f;
    float baseSpeed = 0.0f;
    Vector3 direction;
    MovementState state = MovementState::Idle;

    /// Apply a multiplicative modifier to base speed.
    void ApplySpeedModifier(float modifier) noexcept { speed = baseSpeed * modifier; }

    /// Reset effective speed to base speed.
    void ResetSpeed() noexcept { speed = baseSpeed; }
};

}  // namespace cgs::game
