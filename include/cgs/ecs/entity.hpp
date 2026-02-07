#pragma once

/// @file entity.hpp
/// @brief Entity type for the ECS layer.
///
/// An entity is a lightweight 32-bit handle that combines a unique index
/// (24 bits, supporting up to ~16 million entities) with a version counter
/// (8 bits) for safe recycling.  The compact representation fits in a
/// single register and keeps sparse-set indexing cache-friendly.

#include <cstdint>
#include <functional>
#include <limits>

namespace cgs::ecs {

/// Compact entity handle: 24-bit index + 8-bit version packed into 32 bits.
///
/// The version field prevents the ABA problem when entity IDs are recycled.
/// EntityManager (Issue #10) will manage creation / destruction; this header
/// only defines the value type so that ComponentStorage can use it.
struct Entity {
    uint32_t raw = kInvalidRaw;

    // Bit layout constants.
    static constexpr uint32_t kIdBits = 24;
    static constexpr uint32_t kVersionBits = 8;
    static constexpr uint32_t kIdMask = (1u << kIdBits) - 1;          // 0x00FFFFFF
    static constexpr uint32_t kVersionShift = kIdBits;
    static constexpr uint32_t kInvalidRaw = std::numeric_limits<uint32_t>::max();
    static constexpr uint32_t kMaxId = kIdMask - 1;  // 0x00FFFFFE (reserve 0x00FFFFFF for invalid)

    /// Default-construct to the invalid sentinel.
    constexpr Entity() = default;

    /// Construct from an index and a version.
    constexpr Entity(uint32_t id, uint8_t version)
        : raw((static_cast<uint32_t>(version) << kVersionShift) | (id & kIdMask)) {}

    /// Extract the index portion (0 .. 16'777'214).
    [[nodiscard]] constexpr uint32_t id() const noexcept { return raw & kIdMask; }

    /// Extract the version portion (0 .. 255).
    [[nodiscard]] constexpr uint8_t version() const noexcept {
        return static_cast<uint8_t>(raw >> kVersionShift);
    }

    /// True when this handle refers to a potentially live entity.
    [[nodiscard]] constexpr bool isValid() const noexcept { return raw != kInvalidRaw; }

    /// Return the canonical invalid entity.
    [[nodiscard]] static constexpr Entity invalid() noexcept { return Entity{}; }

    constexpr auto operator<=>(const Entity&) const = default;
};

static_assert(sizeof(Entity) == 4, "Entity must be exactly 32 bits");

} // namespace cgs::ecs

// Hash support for unordered containers.
template <>
struct std::hash<cgs::ecs::Entity> {
    std::size_t operator()(const cgs::ecs::Entity& e) const noexcept {
        return std::hash<uint32_t>{}(e.raw);
    }
};
