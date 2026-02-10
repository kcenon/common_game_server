#pragma once

/// @file token_provider.hpp
/// @brief JWT token generation and validation using HMAC-SHA256.
///
/// Generates RFC 7519 compliant JWT tokens with HS256 signing.
/// The token format is: base64url(header).base64url(payload).base64url(signature)
///
/// @see SRS-SVC-001.3
/// @see SRS-SVC-001.4

#include <string>
#include <string_view>

#include "cgs/foundation/game_result.hpp"
#include "cgs/service/auth_types.hpp"

namespace cgs::service {

/// JWT token provider for access and refresh token management.
///
/// Access tokens are signed JWTs containing user claims.
/// Refresh tokens are opaque secure-random hex strings.
///
/// Example:
/// @code
///   TokenProvider provider("my-secret-key-32-bytes-minimum!!");
///   auto token = provider.generateAccessToken(claims, std::chrono::seconds{900});
///   auto result = provider.validateAccessToken(token);
///   if (result.hasValue()) {
///       auto& decoded = result.value();
///   }
/// @endcode
class TokenProvider {
public:
    /// Construct with the HMAC signing key.
    explicit TokenProvider(std::string signingKey);

    /// Generate a signed JWT access token from the given claims.
    [[nodiscard]] std::string generateAccessToken(
        const TokenClaims& claims,
        std::chrono::seconds expiry) const;

    /// Validate and decode a JWT access token.
    ///
    /// Returns the decoded claims on success, or a GameError on failure
    /// (expired, malformed, invalid signature).
    [[nodiscard]] cgs::foundation::GameResult<TokenClaims> validateAccessToken(
        std::string_view token) const;

    /// Generate a cryptographically random refresh token (hex string).
    [[nodiscard]] static std::string generateRefreshToken();

private:
    std::string signingKey_;
};

} // namespace cgs::service
