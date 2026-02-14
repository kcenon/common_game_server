#pragma once

/// @file password_hasher.hpp
/// @brief Password hashing and verification using SHA-256 with salt.
///
/// Provides secure password storage by generating a random salt per user
/// and computing SHA-256(salt + password).  The interface is designed to be
/// swappable with bcrypt or argon2id implementations via a foundation
/// crypto adapter in production deployments.
///
/// @see SRS-SVC-001.6

#include <string>
#include <string_view>
#include <utility>

namespace cgs::service {

/// Result of a password hashing operation.
struct HashedPassword {
    std::string hash;
    std::string salt;
};

/// Password hashing utility using SHA-256 + random salt.
///
/// Example:
/// @code
///   PasswordHasher hasher;
///   auto hashed = hasher.hash("my_password");
///   bool ok = hasher.verify("my_password", hashed.hash, hashed.salt);
/// @endcode
class PasswordHasher {
public:
    /// Hash a plaintext password with a newly generated random salt.
    [[nodiscard]] HashedPassword hash(std::string_view password) const;

    /// Hash a plaintext password with the given salt (for verification).
    [[nodiscard]] std::string hashWithSalt(std::string_view password, std::string_view salt) const;

    /// Verify a plaintext password against a stored hash and salt.
    [[nodiscard]] bool verify(std::string_view password,
                              std::string_view storedHash,
                              std::string_view salt) const;

    /// Generate a cryptographically random salt (hex-encoded).
    [[nodiscard]] static std::string generateSalt();
};

}  // namespace cgs::service
