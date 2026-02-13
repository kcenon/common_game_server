#pragma once

/// @file input_validator.hpp
/// @brief Reusable input validation utilities for the auth service.
///
/// Provides RFC 5322 subset email validation, password complexity checks,
/// and username sanitization to harden against injection and abuse.
///
/// @see SRS-NFR-015

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

namespace cgs::service {

/// Result of a validation check.
struct ValidationResult {
    bool valid;
    std::string message;

    explicit operator bool() const noexcept { return valid; }

    static ValidationResult ok() { return {true, {}}; }
    static ValidationResult fail(std::string msg) {
        return {false, std::move(msg)};
    }
};

/// Stateless input validation utilities.
///
/// All functions are static and thread-safe. Input length limits are enforced
/// to prevent buffer-based DoS attacks.
class InputValidator {
public:
    // -- Limits ---------------------------------------------------------------

    static constexpr std::size_t kMaxEmailLength = 254;     // RFC 5321
    static constexpr std::size_t kMaxLocalPartLength = 64;  // RFC 5321
    static constexpr std::size_t kMaxDomainLength = 253;    // RFC 5321
    static constexpr std::size_t kMaxDomainLabelLength = 63;

    static constexpr std::size_t kMinUsernameLength = 3;
    static constexpr std::size_t kMaxUsernameLength = 32;

    static constexpr std::size_t kMaxPasswordLength = 128;

    // -- Email ----------------------------------------------------------------

    /// Validate email against RFC 5322 subset.
    ///
    /// Checks: length limits, single '@', local part characters and dot rules,
    /// domain label structure and character rules.
    [[nodiscard]] static inline ValidationResult validateEmail(
        std::string_view email) {
        if (email.empty()) {
            return ValidationResult::fail("email must not be empty");
        }
        if (email.size() > kMaxEmailLength) {
            return ValidationResult::fail("email exceeds maximum length");
        }

        auto atPos = email.find('@');
        if (atPos == std::string_view::npos || atPos == 0) {
            return ValidationResult::fail("email must contain '@'");
        }
        // Only one '@' allowed.
        if (email.find('@', atPos + 1) != std::string_view::npos) {
            return ValidationResult::fail("email must contain exactly one '@'");
        }

        auto local = email.substr(0, atPos);
        auto domain = email.substr(atPos + 1);

        // -- Local part validation --
        if (local.size() > kMaxLocalPartLength) {
            return ValidationResult::fail(
                "email local part exceeds 64 characters");
        }
        if (local.front() == '.' || local.back() == '.') {
            return ValidationResult::fail(
                "email local part must not start or end with '.'");
        }
        if (local.find("..") != std::string_view::npos) {
            return ValidationResult::fail(
                "email local part must not contain consecutive dots");
        }
        // RFC 5322 atext: alphanumeric + !#$%&'*+/=?^_`{|}~-.
        for (char c : local) {
            if (std::isalnum(static_cast<unsigned char>(c))) continue;
            if (isLocalSpecialChar(c)) continue;
            return ValidationResult::fail(
                "email local part contains invalid character");
        }

        // -- Domain validation --
        if (domain.empty() || domain.size() > kMaxDomainLength) {
            return ValidationResult::fail("email domain is invalid");
        }
        if (domain.front() == '.' || domain.back() == '.') {
            return ValidationResult::fail(
                "email domain must not start or end with '.'");
        }
        if (domain.find("..") != std::string_view::npos) {
            return ValidationResult::fail(
                "email domain must not contain consecutive dots");
        }
        // Must have at least one dot (TLD).
        if (domain.find('.') == std::string_view::npos) {
            return ValidationResult::fail(
                "email domain must have at least one dot");
        }

        // Validate each label.
        std::size_t labelStart = 0;
        while (labelStart < domain.size()) {
            auto dotPos = domain.find('.', labelStart);
            auto labelEnd =
                (dotPos == std::string_view::npos) ? domain.size() : dotPos;
            auto label = domain.substr(labelStart, labelEnd - labelStart);

            if (label.empty() || label.size() > kMaxDomainLabelLength) {
                return ValidationResult::fail("email domain label is invalid");
            }
            if (label.front() == '-' || label.back() == '-') {
                return ValidationResult::fail(
                    "email domain label must not start or end with '-'");
            }
            for (char c : label) {
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-') {
                    return ValidationResult::fail(
                        "email domain contains invalid character");
                }
            }

            labelStart = labelEnd + 1;
        }

        return ValidationResult::ok();
    }

    // -- Password -------------------------------------------------------------

    /// Validate password complexity.
    ///
    /// Requires: minimum length, at most kMaxPasswordLength, and at least one
    /// each of uppercase, lowercase, digit, and special character.
    [[nodiscard]] static inline ValidationResult validatePassword(
        std::string_view password, uint32_t minLength) {
        if (password.size() < static_cast<std::size_t>(minLength)) {
            return ValidationResult::fail(
                "password must be at least " + std::to_string(minLength) +
                " characters");
        }
        if (password.size() > kMaxPasswordLength) {
            return ValidationResult::fail(
                "password must not exceed " +
                std::to_string(kMaxPasswordLength) + " characters");
        }

        bool hasUpper = false;
        bool hasLower = false;
        bool hasDigit = false;
        bool hasSpecial = false;

        for (char c : password) {
            auto uc = static_cast<unsigned char>(c);
            if (std::isupper(uc)) hasUpper = true;
            else if (std::islower(uc)) hasLower = true;
            else if (std::isdigit(uc)) hasDigit = true;
            else hasSpecial = true;
        }

        if (!hasUpper) {
            return ValidationResult::fail(
                "password must contain at least one uppercase letter");
        }
        if (!hasLower) {
            return ValidationResult::fail(
                "password must contain at least one lowercase letter");
        }
        if (!hasDigit) {
            return ValidationResult::fail(
                "password must contain at least one digit");
        }
        if (!hasSpecial) {
            return ValidationResult::fail(
                "password must contain at least one special character");
        }

        return ValidationResult::ok();
    }

    // -- Username -------------------------------------------------------------

    /// Validate username format.
    ///
    /// Rules: 3-32 characters, starts with a letter, allowed characters are
    /// [a-zA-Z0-9._-], no consecutive special characters, not a reserved name.
    [[nodiscard]] static inline ValidationResult validateUsername(
        std::string_view username) {
        if (username.size() < kMinUsernameLength) {
            return ValidationResult::fail(
                "username must be at least " +
                std::to_string(kMinUsernameLength) + " characters");
        }
        if (username.size() > kMaxUsernameLength) {
            return ValidationResult::fail(
                "username must not exceed " +
                std::to_string(kMaxUsernameLength) + " characters");
        }

        // Must start with a letter.
        if (!std::isalpha(static_cast<unsigned char>(username.front()))) {
            return ValidationResult::fail("username must start with a letter");
        }

        // Check allowed characters and consecutive specials.
        bool prevSpecial = false;
        for (char c : username) {
            auto uc = static_cast<unsigned char>(c);
            bool isSpecial = (c == '.' || c == '_' || c == '-');

            if (!std::isalnum(uc) && !isSpecial) {
                return ValidationResult::fail(
                    "username contains invalid character");
            }
            if (isSpecial && prevSpecial) {
                return ValidationResult::fail(
                    "username must not contain consecutive special characters");
            }
            prevSpecial = isSpecial;
        }

        // Must not end with a special character.
        if (username.back() == '.' || username.back() == '_' ||
            username.back() == '-') {
            return ValidationResult::fail(
                "username must not end with a special character");
        }

        // Reserved names check (case-insensitive).
        if (isReservedUsername(username)) {
            return ValidationResult::fail("username is reserved");
        }

        return ValidationResult::ok();
    }

private:
    /// Characters allowed in the email local part (RFC 5322 atext specials).
    static constexpr bool isLocalSpecialChar(char c) noexcept {
        switch (c) {
            case '.': case '!': case '#': case '$': case '%': case '&':
            case '\'': case '*': case '+': case '/': case '=': case '?':
            case '^': case '_': case '`': case '{': case '|': case '}':
            case '~': case '-':
                return true;
            default:
                return false;
        }
    }

    /// Check if a username matches a reserved name (case-insensitive).
    [[nodiscard]] static inline bool isReservedUsername(
        std::string_view username) {
        static constexpr std::array<std::string_view, 12> kReserved = {{
            "admin", "administrator", "root", "system", "moderator", "mod",
            "support", "help", "server", "guest", "test", "null"
        }};

        // Build lowercase copy for comparison.
        std::string lower(username.size(), '\0');
        std::transform(username.begin(), username.end(), lower.begin(),
                        [](unsigned char c) {
                            return static_cast<char>(std::tolower(c));
                        });

        for (auto reserved : kReserved) {
            if (lower == reserved) return true;
        }
        return false;
    }
};

} // namespace cgs::service
