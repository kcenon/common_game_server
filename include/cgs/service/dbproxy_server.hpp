#pragma once

/// @file dbproxy_server.hpp
/// @brief Database Proxy Server for connection pooling, query routing,
///        query caching, and read replica support.
///
/// DBProxyServer is the main entry point for game servers to execute
/// database queries. It provides:
/// - Connection pooling with configurable pool sizes.
/// - LRU query cache with TTL for read-heavy data (templates, configs).
/// - Cache invalidation on write operations.
/// - Read replica routing for SELECT query load distribution.
/// - Connection metrics: pool utilization, query latency histogram.
///
/// @see SRS-SVC-005
/// @see SDS-MOD-034

#include "cgs/foundation/game_database.hpp"
#include "cgs/foundation/game_result.hpp"
#include "cgs/service/dbproxy_types.hpp"

#include <future>
#include <memory>
#include <string_view>

namespace cgs::service {

/// Database Proxy Server.
///
/// Usage:
/// @code
///   DBProxyConfig config;
///   config.primary.connectionString = "host=db dbname=game";
///   config.replicas.push_back({"host=replica1 dbname=game"});
///   config.cache.enabled = true;
///   config.cache.defaultTtl = std::chrono::seconds(60);
///
///   DBProxyServer proxy(config);
///   auto r = proxy.start();
///
///   // Reads may be served from cache or routed to replicas.
///   auto result = proxy.query("SELECT * FROM item_templates");
///
///   // Writes go to primary, and invalidate related cache entries.
///   proxy.execute("UPDATE players SET level = 10 WHERE id = 1");
/// @endcode
class DBProxyServer {
public:
    explicit DBProxyServer(DBProxyConfig config);
    ~DBProxyServer();

    DBProxyServer(const DBProxyServer&) = delete;
    DBProxyServer& operator=(const DBProxyServer&) = delete;
    DBProxyServer(DBProxyServer&&) noexcept;
    DBProxyServer& operator=(DBProxyServer&&) noexcept;

    // ── Lifecycle ───────────────────────────────────────────────────────

    /// Start the proxy: connect to primary and replicas.
    [[nodiscard]] cgs::foundation::GameResult<void> start();

    /// Stop the proxy: disconnect all pools and clear cache.
    void stop();

    /// Check if the proxy is running.
    [[nodiscard]] bool isRunning() const noexcept;

    // ── Query execution ─────────────────────────────────────────────────

    /// Execute a SELECT query.
    ///
    /// The query is first checked against the cache. On cache miss,
    /// it is routed to a read replica (if available) or the primary.
    /// Results are cached for future reads.
    [[nodiscard]] cgs::foundation::GameResult<cgs::foundation::QueryResult> query(
        std::string_view sql);

    /// Execute a write command (INSERT/UPDATE/DELETE/DDL).
    ///
    /// Always routed to the primary. Automatically invalidates cache
    /// entries that reference tables mentioned in the SQL.
    [[nodiscard]] cgs::foundation::GameResult<uint64_t> execute(std::string_view sql);

    /// Execute a SELECT query asynchronously.
    [[nodiscard]] std::future<cgs::foundation::GameResult<cgs::foundation::QueryResult>> queryAsync(
        std::string_view sql);

    // ── Parameterized query execution (SRS-NFR-016) ──────────────────────

    /// Execute a parameterized SELECT query using PreparedStatement.
    ///
    /// Prevents SQL injection by resolving bound parameters with proper
    /// escaping. Cache lookup uses the resolved SQL string.
    [[nodiscard]] cgs::foundation::GameResult<cgs::foundation::QueryResult> query(
        const cgs::foundation::PreparedStatement& stmt);

    /// Execute a parameterized write command using PreparedStatement.
    ///
    /// Prevents SQL injection by resolving bound parameters with proper
    /// escaping. Cache invalidation uses the resolved SQL.
    [[nodiscard]] cgs::foundation::GameResult<uint64_t> execute(
        const cgs::foundation::PreparedStatement& stmt);

    /// Execute a parameterized SELECT query asynchronously.
    [[nodiscard]] std::future<cgs::foundation::GameResult<cgs::foundation::QueryResult>> queryAsync(
        const cgs::foundation::PreparedStatement& stmt);

    // ── Cache management ────────────────────────────────────────────────

    /// Manually invalidate all cache entries for a specific table.
    std::size_t invalidateCache(std::string_view tableName);

    /// Clear the entire cache.
    void clearCache();

    /// Get current cache entry count.
    [[nodiscard]] std::size_t cacheSize() const;

    /// Get cache hit rate (0.0 to 1.0).
    [[nodiscard]] double cacheHitRate() const;

    // ── Statistics ──────────────────────────────────────────────────────

    /// Get pool and cache statistics.
    [[nodiscard]] PoolStats poolStats() const;

    /// Get total number of queries executed.
    [[nodiscard]] uint64_t totalQueries() const;

    /// Get the configuration.
    [[nodiscard]] const DBProxyConfig& config() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace cgs::service
