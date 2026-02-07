/// @file component_storage.cpp
/// @brief Non-template parts of the component storage module.
///
/// ComponentStorage<T> is predominantly header-only because it is a class
/// template.  This translation unit exists to give the CMake target a
/// compilable source file (required by some generators) and to anchor the
/// ComponentTypeId counter in a single shared library image.

#include "cgs/ecs/component_storage.hpp"

// Force instantiation of the global type-id counter so it is emitted in
// exactly this translation unit when linking as a shared library.
namespace cgs::ecs::detail {

static const auto kAnchor [[maybe_unused]] = nextComponentTypeId;

} // namespace cgs::ecs::detail
