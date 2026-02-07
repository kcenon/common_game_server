#pragma once

/// @file component_type_id.hpp
/// @brief RTTI-free compile-time component type identification.
///
/// Each distinct component type `T` receives a unique integer ID the first
/// time `ComponentType<T>::Id()` is called.  The IDs are generated via an
/// atomic counter, so they are safe to use from multiple threads although
/// the exact numeric values are not deterministic across runs.

#include <atomic>
#include <cstdint>

namespace cgs::ecs {

/// Integer type used to identify component types at runtime.
using ComponentTypeId = uint32_t;

/// Sentinel value meaning "no type".
constexpr ComponentTypeId kInvalidComponentTypeId = static_cast<ComponentTypeId>(-1);

namespace detail {

/// Global atomic counter for generating unique ComponentTypeId values.
inline ComponentTypeId nextComponentTypeId() noexcept {
    static std::atomic<ComponentTypeId> counter{0};
    return counter.fetch_add(1, std::memory_order_relaxed);
}

} // namespace detail

/// Obtain the unique ComponentTypeId for type `T`.
///
/// The ID is lazily assigned on first access and remains stable for the
/// lifetime of the process.  No RTTI (`typeid`) is used.
///
/// Usage:
/// @code
///   auto id = ComponentType<HealthComponent>::Id();
/// @endcode
template <typename T>
struct ComponentType {
    static ComponentTypeId Id() noexcept {
        static const ComponentTypeId value = detail::nextComponentTypeId();
        return value;
    }
};

} // namespace cgs::ecs
