#pragma once

/// @file token_provider.hpp
/// @brief JWT token generation and validation with HS256 and RS256 support.
///
/// Generates RFC 7519 compliant JWT tokens. Supports both symmetric (HS256)
/// and asymmetric (RS256) signing algorithms. RS256 is recommended for
/// production to eliminate shared-key compromise risk.
///
/// @see SRS-SVC-001.3
/// @see SRS-SVC-001.4
/// @see SRS-NFR-014

#include "cgs/foundation/game_result.hpp"
#include "cgs/service/auth_types.hpp"

#include <string>
#include <string_view>

namespace cgs::service {

class TokenBlacklist;

/// JWT token provider for access and refresh token management.
///
/// Access tokens are signed JWTs containing user claims.
/// Refresh tokens are opaque secure-random hex strings.
///
/// Example (HS256, backward-compatible):
/// @code
///   AuthConfig config;
///   TokenProvider provider(config);
///   auto token = provider.generateAccessToken(claims, std::chrono::seconds{900});
///   auto result = provider.validateAccessToken(token);
/// @endcode
///
/// Example (RS256):
/// @code
///   AuthConfig config;
///   config.jwtAlgorithm = JwtAlgorithm::RS256;
///   config.rsaPrivateKeyPem = "-----BEGIN PRIVATE KEY-----\n...";
///   config.rsaPublicKeyPem = "-----BEGIN PUBLIC KEY-----\n...";
///   TokenProvider provider(config);
/// @endcode
class TokenProvider {
public:
    /// Construct with full auth configuration (supports HS256 and RS256).
    explicit TokenProvider(const AuthConfig& config);

    /// Generate a signed JWT access token from the given claims.
    ///
    /// Includes a unique `jti` (JWT ID) claim for blacklist referencing.
    [[nodiscard]] std::string generateAccessToken(const TokenClaims& claims,
                                                  std::chrono::seconds expiry) const;

    /// Validate and decode a JWT access token.
    ///
    /// Reads the `alg` field from the JWT header to determine the
    /// verification method. Checks the token blacklist if one is set.
    ///
    /// Returns the decoded claims on success, or a GameError on failure
    /// (expired, malformed, invalid signature, blacklisted).
    [[nodiscard]] cgs::foundation::GameResult<TokenClaims> validateAccessToken(
        std::string_view token) const;

    /// Set the token blacklist for access token revocation checking.
    void setBlacklist(TokenBlacklist* blacklist);

    /// Generate a cryptographically random refresh token (hex string).
    [[nodiscard]] static std::string generateRefreshToken();

private:
    std::string signingKey_;
    std::string rsaPrivateKeyPem_;
    std::string rsaPublicKeyPem_;
    JwtAlgorithm algorithm_;
    TokenBlacklist* blacklist_ = nullptr;
};

}  // namespace cgs::service
