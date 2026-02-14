/// @file query.cpp
/// @brief Anchor translation unit for the Query module (SDS-MOD-013).
///
/// Query<...> is fully template-based and implemented in the header.
/// This file exists to provide a library target for the build system
/// and to anchor the module in the linker.

#include "cgs/ecs/query.hpp"

namespace cgs::ecs {
// Intentionally empty — all Query logic is in the header template.
}  // namespace cgs::ecs
