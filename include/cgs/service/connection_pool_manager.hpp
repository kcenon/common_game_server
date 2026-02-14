#pragma once

/// @file connection_pool_manager.hpp
/// @brief Manages primary and read-replica database connection pools.
///
/// Provides query routing: write operations go to the primary,
/// SELECT queries are distributed across read replicas using
/// round-robin. Falls back to primary if no replicas are available.
///
/// @see SRS-SVC-005.1, SRS-SVC-005.4
/// @see SDS-MOD-034

#include "cgs/foundation/game_result.hpp"
#include "cgs/service/dbproxy_types.hpp"

#include <memory>
#include <string_view>

namespace cgs::service {

/// Manages primary + replica connection pools with query routing.
///
/// Usage:
/// @code
///   DBProxyConfig config;
///   config.primary.connectionString = "host=primary dbname=game";
///   config.replicas.push_back({"host=replica1 dbname=game"});
///
///   ConnectionPoolManager pool(config);
///   auto r = pool.start();
///
///   // SELECT queries are routed to replicas.
///   auto result = pool.query("SELECT * FROM players");
///
///   // Write queries go to primary.
///   auto affected = pool.execute("UPDATE players SET level = 10");
/// @endcode
class ConnectionPoolManager {
public:
    explicit ConnectionPoolManager(DBProxyConfig config);
    ~ConnectionPoolManager();

    ConnectionPoolManager(const ConnectionPoolManager&) = delete;
    ConnectionPoolManager& operator=(const ConnectionPoolManager&) = delete;
    ConnectionPoolManager(ConnectionPoolManager&&) noexcept;
    ConnectionPoolManager& operator=(ConnectionPoolManager&&) noexcept;

    /// Connect to primary and all replica databases.
    [[nodiscard]] cgs::foundation::GameResult<void> start();

    /// Disconnect all pools.
    void stop();

    /// Check if the primary connection is alive.
    [[nodiscard]] bool isConnected() const noexcept;

    /// Execute a SELECT query, routing to a replica if available.
    [[nodiscard]] cgs::foundation::GameResult<cgs::foundation::QueryResult> query(
        std::string_view sql);

    /// Execute a write command (INSERT/UPDATE/DELETE) on the primary.
    [[nodiscard]] cgs::foundation::GameResult<uint64_t> execute(std::string_view sql);

    /// Execute a parameterized SELECT query, routing to a replica if available.
    ///
    /// Uses PreparedStatement for SQL injection prevention (SRS-NFR-016).
    /// The statement is resolved to safe SQL before execution.
    [[nodiscard]] cgs::foundation::GameResult<cgs::foundation::QueryResult> query(
        const cgs::foundation::PreparedStatement& stmt);

    /// Execute a parameterized write command on the primary.
    ///
    /// Uses PreparedStatement for SQL injection prevention (SRS-NFR-016).
    [[nodiscard]] cgs::foundation::GameResult<uint64_t> execute(
        const cgs::foundation::PreparedStatement& stmt);

    /// Get the number of available replicas.
    [[nodiscard]] std::size_t replicaCount() const;

    /// Get pool statistics.
    [[nodiscard]] PoolStats stats() const;

    /// Get the configuration.
    [[nodiscard]] const DBProxyConfig& config() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace cgs::service
