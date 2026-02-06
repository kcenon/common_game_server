#pragma once

/// @file game_error.hpp
/// @brief Game-specific error type used with Result<T, GameError>.

#include <any>
#include <string>
#include <string_view>
#include <utility>

#include "cgs/foundation/error_code.hpp"

namespace cgs::foundation {

/// Rich error type carrying an error code, human-readable message,
/// and optional type-erased context data for debugging.
class GameError {
public:
    GameError() = default;

    explicit GameError(ErrorCode code)
        : code_(code) {}

    GameError(ErrorCode code, std::string message)
        : code_(code), message_(std::move(message)) {}

    GameError(ErrorCode code, std::string message, std::any context)
        : code_(code), message_(std::move(message)), context_(std::move(context)) {}

    /// The categorized error code.
    [[nodiscard]] ErrorCode code() const noexcept { return code_; }

    /// Human-readable error description.
    [[nodiscard]] std::string_view message() const noexcept { return message_; }

    /// The subsystem that produced this error.
    [[nodiscard]] std::string_view subsystem() const noexcept {
        return errorSubsystem(code_);
    }

    /// Access typed context data (returns nullptr if type mismatch or empty).
    template <typename T>
    [[nodiscard]] const T* context() const noexcept {
        return std::any_cast<T>(&context_);
    }

    /// Check whether this error carries context data.
    [[nodiscard]] bool hasContext() const noexcept { return context_.has_value(); }

    /// Check if this represents a success (no error).
    [[nodiscard]] bool isSuccess() const noexcept {
        return code_ == ErrorCode::Success;
    }

private:
    ErrorCode code_ = ErrorCode::Unknown;
    std::string message_;
    std::any context_;
};

} // namespace cgs::foundation
