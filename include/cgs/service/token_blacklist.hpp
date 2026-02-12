#pragma once

/// @file token_blacklist.hpp
/// @brief Access token blacklist with TTL-based auto-cleanup.
///
/// Maintains a set of revoked JWT IDs (jti) to prevent use of
/// compromised or logged-out access tokens before their natural expiry.
/// Thread-safe with std::shared_mutex for read-heavy workloads.
///
/// @see SRS-NFR-014

#include <chrono>
#include <cstddef>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace cgs::service {

/// In-memory access token blacklist keyed by JWT ID (jti).
///
/// Tokens are automatically removed after their expiry time,
/// preventing unbounded memory growth.
///
/// Example:
/// @code
///   TokenBlacklist blacklist(std::chrono::seconds{300});
///   blacklist.revoke("token-jti-123", expiresAt);
///   assert(blacklist.isRevoked("token-jti-123"));
/// @endcode
class TokenBlacklist {
public:
    /// Construct with the interval between automatic cleanup passes.
    explicit TokenBlacklist(std::chrono::seconds cleanupInterval);

    /// Add a token to the blacklist.
    ///
    /// @param jti       The JWT ID claim value.
    /// @param expiresAt When the token naturally expires (for auto-cleanup).
    void revoke(std::string_view jti,
                std::chrono::system_clock::time_point expiresAt);

    /// Check if a token is blacklisted.
    [[nodiscard]] bool isRevoked(std::string_view jti) const;

    /// Remove entries whose expiry has passed. Returns number removed.
    std::size_t cleanup();

    /// Number of entries currently in the blacklist.
    [[nodiscard]] std::size_t size() const;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string,
                       std::chrono::system_clock::time_point> entries_;
    std::chrono::seconds cleanupInterval_;
    std::chrono::system_clock::time_point lastCleanup_;
};

} // namespace cgs::service
