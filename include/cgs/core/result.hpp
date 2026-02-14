#pragma once

/// @file result.hpp
/// @brief Result<T,E> type for explicit error handling without exceptions.

#include <string>
#include <variant>

namespace cgs {

/// Error information for Result type.
struct Error {
    int code = 0;
    std::string message;

    Error() = default;
    explicit Error(std::string msg) : code(-1), message(std::move(msg)) {}
    Error(int c, std::string msg) : code(c), message(std::move(msg)) {}
};

/// Result type for explicit error propagation.
///
/// Replaces exceptions with a type-safe, explicit error handling pattern.
/// Every function that can fail returns Result<T, E> instead of throwing.
///
/// @tparam T The success value type.
/// @tparam E The error type (defaults to cgs::Error).
///
/// Example:
/// @code
///   auto result = parseConfig("server.yaml");
///   if (result.hasValue()) {
///       auto config = result.value();
///   } else {
///       log(result.error().message);
///   }
/// @endcode
template <typename T, typename E = Error>
class Result {
public:
    /// Construct a success result.
    static Result ok(T value) { return Result(std::move(value)); }

    /// Construct an error result.
    static Result err(E error) { return Result(std::move(error)); }

    /// Check if this result holds a value.
    [[nodiscard]] bool hasValue() const noexcept { return std::holds_alternative<T>(data_); }

    /// Check if this result holds an error.
    [[nodiscard]] bool hasError() const noexcept { return std::holds_alternative<E>(data_); }

    /// Implicit conversion to bool (true if success).
    explicit operator bool() const noexcept { return hasValue(); }

    /// Access the success value (undefined behavior if error).
    [[nodiscard]] const T& value() const& { return std::get<T>(data_); }
    [[nodiscard]] T& value() & { return std::get<T>(data_); }
    [[nodiscard]] T&& value() && { return std::get<T>(std::move(data_)); }

    /// Access the error (undefined behavior if success).
    [[nodiscard]] const E& error() const& { return std::get<E>(data_); }
    [[nodiscard]] E& error() & { return std::get<E>(data_); }

    /// Access value or return a default.
    [[nodiscard]] T valueOr(T defaultValue) const& {
        return hasValue() ? value() : std::move(defaultValue);
    }

private:
    explicit Result(T value) : data_(std::move(value)) {}
    explicit Result(E error) : data_(std::move(error)) {}

    std::variant<T, E> data_;
};

/// Specialization for void success type.
template <typename E>
class Result<void, E> {
public:
    static Result ok() { return Result(true); }
    static Result err(E error) { return Result(std::move(error)); }

    [[nodiscard]] bool hasValue() const noexcept { return success_; }
    [[nodiscard]] bool hasError() const noexcept { return !success_; }
    explicit operator bool() const noexcept { return success_; }

    [[nodiscard]] const E& error() const& { return error_; }

private:
    explicit Result(bool) : success_(true) {}
    explicit Result(E error) : success_(false), error_(std::move(error)) {}

    bool success_ = false;
    E error_;
};

}  // namespace cgs
