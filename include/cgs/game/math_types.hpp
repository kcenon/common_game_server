#pragma once

/// @file math_types.hpp
/// @brief Lightweight math types for the game logic layer.
///
/// Provides Vector3 and Quaternion value types with basic arithmetic
/// operations.  These are intentionally simple POD-like structs that
/// pack tightly for cache-friendly ECS component storage.
///
/// @see SDS-MOD-020

#include <cmath>
#include <cstdint>

namespace cgs::game {

/// Three-component floating-point vector.
///
/// Used for position, direction, scale, and similar 3D quantities.
/// Supports element-wise arithmetic and common vector operations.
struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vector3() = default;
    constexpr Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

    // Arithmetic operators.
    constexpr Vector3 operator+(const Vector3& rhs) const noexcept {
        return {x + rhs.x, y + rhs.y, z + rhs.z};
    }
    constexpr Vector3 operator-(const Vector3& rhs) const noexcept {
        return {x - rhs.x, y - rhs.y, z - rhs.z};
    }
    constexpr Vector3 operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar, z * scalar};
    }

    constexpr Vector3& operator+=(const Vector3& rhs) noexcept {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    constexpr Vector3& operator-=(const Vector3& rhs) noexcept {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

    /// Dot product.
    [[nodiscard]] constexpr float Dot(const Vector3& rhs) const noexcept {
        return x * rhs.x + y * rhs.y + z * rhs.z;
    }

    /// Squared magnitude (avoids sqrt).
    [[nodiscard]] constexpr float LengthSquared() const noexcept { return Dot(*this); }

    /// Magnitude.
    [[nodiscard]] float Length() const noexcept { return std::sqrt(LengthSquared()); }

    /// Return a normalized copy, or zero vector if length is near zero.
    [[nodiscard]] Vector3 Normalized() const noexcept {
        const float len = Length();
        if (len < 1e-6f) {
            return {};
        }
        return {x / len, y / len, z / len};
    }

    /// The zero vector.
    [[nodiscard]] static constexpr Vector3 Zero() noexcept { return {}; }

    /// The unit vector (1,1,1).
    [[nodiscard]] static constexpr Vector3 One() noexcept { return {1.0f, 1.0f, 1.0f}; }

    constexpr auto operator<=>(const Vector3&) const = default;
};

/// Scalar * Vector3.
constexpr Vector3 operator*(float scalar, const Vector3& v) noexcept {
    return v * scalar;
}

/// Quaternion for rotation representation.
///
/// Stored as (w, x, y, z) where w is the scalar part.
/// Defaults to the identity rotation (1, 0, 0, 0).
struct Quaternion {
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Quaternion() = default;
    constexpr Quaternion(float w, float x, float y, float z) : w(w), x(x), y(y), z(z) {}

    /// The identity rotation.
    [[nodiscard]] static constexpr Quaternion Identity() noexcept { return {}; }

    constexpr auto operator<=>(const Quaternion&) const = default;
};

}  // namespace cgs::game
