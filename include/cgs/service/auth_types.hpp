#pragma once

/// @file auth_types.hpp
/// @brief Core type definitions for the Authentication Server.
///
/// Defines user records, token structures, and configuration types
/// used throughout the auth service layer.
///
/// @see SRS-SVC-001
/// @see SDS-MOD-040

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace cgs::service {

// -- User model ---------------------------------------------------------------

/// Account status for a registered user.
enum class UserStatus : uint8_t { Active, Suspended, Deleted };

/// Stored user record containing hashed credentials.
///
/// Passwords are never stored in plaintext.  The password_hash and salt
/// fields are produced by PasswordHasher and verified through it.
struct UserRecord {
    uint64_t id = 0;
    std::string username;
    std::string email;
    std::string passwordHash;
    std::string salt;
    UserStatus status = UserStatus::Active;
    std::vector<std::string> roles;
    std::chrono::system_clock::time_point createdAt{};
    std::chrono::system_clock::time_point updatedAt{};
};

// -- Token structures ---------------------------------------------------------

/// Decoded JWT claims payload.
struct TokenClaims {
    std::string subject;                                ///< User ID ("sub").
    std::string username;                               ///< Username.
    std::vector<std::string> roles;                     ///< Granted roles.
    std::string jti;                                    ///< JWT ID for blacklisting.
    std::chrono::system_clock::time_point issuedAt{};   ///< Issued-at ("iat").
    std::chrono::system_clock::time_point expiresAt{};  ///< Expiry ("exp").
};

/// Access + refresh token pair returned on successful authentication.
struct TokenPair {
    std::string accessToken;
    std::string refreshToken;
    std::chrono::seconds accessExpiresIn{};
    std::chrono::seconds refreshExpiresIn{};
};

/// Stored refresh token record for revocation support.
struct RefreshTokenRecord {
    std::string token;
    uint64_t userId = 0;
    std::chrono::system_clock::time_point expiresAt{};
    bool revoked = false;
};

// -- Configuration ------------------------------------------------------------

/// JWT signing algorithm selection.
enum class JwtAlgorithm : uint8_t {
    HS256,  ///< HMAC-SHA256 (symmetric, default for backward compatibility).
    RS256   ///< RSA-SHA256 (asymmetric, recommended for production).
};

/// Configuration for the authentication service.
struct AuthConfig {
    /// Secret key for HMAC-SHA256 token signing (min 32 bytes recommended).
    std::string signingKey = "change-me-in-production";

    /// PEM-encoded RSA private key for RS256 signing (auth server only).
    std::string rsaPrivateKeyPem;

    /// PEM-encoded RSA public key for RS256 verification (all services).
    std::string rsaPublicKeyPem;

    /// JWT signing algorithm (default HS256 for backward compatibility).
    JwtAlgorithm jwtAlgorithm = JwtAlgorithm::HS256;

    /// Access token lifetime.
    std::chrono::seconds accessTokenExpiry{900};  // 15 minutes

    /// Refresh token lifetime.
    std::chrono::seconds refreshTokenExpiry{604800};  // 7 days

    /// Interval between blacklist cleanup passes.
    std::chrono::seconds blacklistCleanupInterval{300};  // 5 minutes

    /// Minimum password length.
    uint32_t minPasswordLength = 8;

    /// Maximum login attempts per key within the rate window.
    uint32_t rateLimitMaxAttempts = 5;

    /// Sliding window duration for rate limiting.
    std::chrono::seconds rateLimitWindow{60};  // 1 minute
};

// -- Credential input ---------------------------------------------------------

/// Credentials submitted for registration or login.
struct UserCredentials {
    std::string username;
    std::string email;
    std::string password;
};

}  // namespace cgs::service
