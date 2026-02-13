#pragma once

/// @file common_adapter.hpp
/// @brief Aggregate header for the Common System Adapter (SDS-MOD-001).
///
/// Provides game-specific error types, Result aliases, type-safe IDs,
/// dependency injection container, and configuration management.

#include "cgs/foundation/config_manager.hpp"
#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_error.hpp"
#include "cgs/foundation/game_result.hpp"
#include "cgs/foundation/service_locator.hpp"
#include "cgs/foundation/types.hpp"
