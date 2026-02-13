#pragma once

/// @file token_store.hpp
/// @brief Refresh token persistence interface and in-memory implementation.
///
/// Abstracts refresh token storage so the AuthServer can work with any
/// backend (in-memory, Redis, SQL database, etc.).
///
/// @see SRS-SVC-001.4
/// @see SRS-SVC-001.5

#include "cgs/service/auth_types.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace cgs::service {

/// Abstract interface for refresh token persistence.
///
/// Implementations must be thread-safe when shared across threads.
class ITokenStore {
public:
    virtual ~ITokenStore() = default;

    /// Store a new refresh token record.
    virtual void store(RefreshTokenRecord record) = 0;

    /// Find a refresh token by its value.
    [[nodiscard]] virtual std::optional<RefreshTokenRecord> find(std::string_view token) const = 0;

    /// Revoke a specific refresh token. Returns false if not found.
    virtual bool revoke(std::string_view token) = 0;

    /// Revoke all refresh tokens belonging to a user.
    virtual void revokeAllForUser(uint64_t userId) = 0;

    /// Remove expired tokens to reclaim memory.
    virtual void removeExpired() = 0;
};

/// Thread-safe in-memory refresh token store for testing and development.
///
/// Production deployments should use a persistent store (Redis, database).
class InMemoryTokenStore : public ITokenStore {
public:
    void store(RefreshTokenRecord record) override;

    [[nodiscard]] std::optional<RefreshTokenRecord> find(std::string_view token) const override;

    bool revoke(std::string_view token) override;

    void revokeAllForUser(uint64_t userId) override;

    void removeExpired() override;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, RefreshTokenRecord> tokens_;
};

}  // namespace cgs::service
