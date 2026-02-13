#pragma once

/// @file user_repository.hpp
/// @brief User persistence interface and in-memory implementation.
///
/// Abstracts user storage so the AuthServer can work with any backend
/// (in-memory, SQL database via DatabaseAdapter, etc.).
///
/// @see SRS-SVC-001.1
/// @see SRS-SVC-001.2

#include "cgs/service/auth_types.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace cgs::service {

/// Abstract interface for user persistence.
///
/// Implementations must be thread-safe when shared across threads.
class IUserRepository {
public:
    virtual ~IUserRepository() = default;

    /// Find a user by their unique ID.
    [[nodiscard]] virtual std::optional<UserRecord> findById(uint64_t id) const = 0;

    /// Find a user by username (case-sensitive).
    [[nodiscard]] virtual std::optional<UserRecord> findByUsername(
        std::string_view username) const = 0;

    /// Find a user by email (case-sensitive).
    [[nodiscard]] virtual std::optional<UserRecord> findByEmail(std::string_view email) const = 0;

    /// Create a new user, returning the assigned ID.
    virtual uint64_t create(UserRecord record) = 0;

    /// Update an existing user record. Returns false if user not found.
    virtual bool update(const UserRecord& record) = 0;
};

/// Thread-safe in-memory user repository for testing and development.
///
/// Production deployments should use a DatabaseAdapter-backed implementation.
class InMemoryUserRepository : public IUserRepository {
public:
    [[nodiscard]] std::optional<UserRecord> findById(uint64_t id) const override;

    [[nodiscard]] std::optional<UserRecord> findByUsername(
        std::string_view username) const override;

    [[nodiscard]] std::optional<UserRecord> findByEmail(std::string_view email) const override;

    uint64_t create(UserRecord record) override;

    bool update(const UserRecord& record) override;

private:
    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, UserRecord> users_;
    uint64_t nextId_ = 1;
};

}  // namespace cgs::service
