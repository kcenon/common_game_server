#pragma once

/// @file game_result.hpp
/// @brief GameResult<T> type alias for game-specific error handling.

#include "cgs/core/result.hpp"
#include "cgs/foundation/game_error.hpp"

namespace cgs::foundation {

/// Result type specialized with GameError for game framework operations.
///
/// Every adapter and game system method that can fail returns GameResult<T>
/// instead of throwing exceptions.
///
/// Example:
/// @code
///   GameResult<int> computeDamage(int base) {
///       if (base < 0) {
///           return GameResult<int>::err(
///               GameError(ErrorCode::InvalidArgument, "negative base damage"));
///       }
///       return GameResult<int>::ok(base * 2);
///   }
/// @endcode
template <typename T>
using GameResult = cgs::Result<T, GameError>;

}  // namespace cgs::foundation
