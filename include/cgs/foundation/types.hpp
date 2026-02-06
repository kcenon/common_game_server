#pragma once

/// @file types.hpp
/// @brief Game-specific type aliases and strong ID types.

#include <cstdint>
#include <functional>

namespace cgs::foundation {

/// Tag-based strong typedef for type-safe ID values.
///
/// Prevents accidental mixing of different ID types (e.g., EntityId and PlayerId)
/// at compile time while keeping the same underlying representation.
///
/// @tparam Tag A unique tag type to distinguish different ID types.
/// @tparam T The underlying integral type.
template <typename Tag, typename T = uint64_t>
class StrongId {
public:
    constexpr StrongId() = default;
    constexpr explicit StrongId(T value) : value_(value) {}

    [[nodiscard]] constexpr T value() const noexcept { return value_; }
    [[nodiscard]] constexpr bool isValid() const noexcept { return value_ != 0; }

    constexpr auto operator<=>(const StrongId&) const = default;

private:
    T value_ = 0;
};

// Tag types for strong IDs
struct EntityIdTag {};
struct PlayerIdTag {};
struct SessionIdTag {};

/// Unique identifier for ECS entities.
using EntityId = StrongId<EntityIdTag>;

/// Unique identifier for player accounts.
using PlayerId = StrongId<PlayerIdTag>;

/// Unique identifier for network sessions.
using SessionId = StrongId<SessionIdTag>;

/// Invalid/null sentinel for any ID type.
template <typename Tag, typename T>
constexpr StrongId<Tag, T> NULL_ID{};

} // namespace cgs::foundation

// Hash support for use in unordered containers.
template <typename Tag, typename T>
struct std::hash<cgs::foundation::StrongId<Tag, T>> {
    std::size_t operator()(const cgs::foundation::StrongId<Tag, T>& id) const noexcept {
        return std::hash<T>{}(id.value());
    }
};
