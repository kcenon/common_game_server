#pragma once

/// @file query_cache.hpp
/// @brief Thread-safe LRU query cache with TTL expiration.
///
/// Caches SELECT query results keyed by the SQL string.
/// Supports configurable TTL, size limits, and table-based
/// invalidation for write-through consistency.
///
/// @see SRS-SVC-005.3
/// @see SDS-MOD-034

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "cgs/foundation/game_database.hpp"
#include "cgs/service/dbproxy_types.hpp"

namespace cgs::service {

/// Thread-safe LRU query cache with TTL-based expiration.
///
/// Usage:
/// @code
///   CacheConfig config;
///   config.maxEntries = 1000;
///   config.defaultTtl = std::chrono::seconds(60);
///   QueryCache cache(config);
///
///   cache.put("SELECT * FROM items", result);
///   auto cached = cache.get("SELECT * FROM items");
///   if (cached) { /* use cached result */ }
///
///   cache.invalidateByTable("items"); // On write to items table
/// @endcode
class QueryCache {
public:
    explicit QueryCache(CacheConfig config);
    ~QueryCache();

    QueryCache(const QueryCache&) = delete;
    QueryCache& operator=(const QueryCache&) = delete;
    QueryCache(QueryCache&&) noexcept;
    QueryCache& operator=(QueryCache&&) noexcept;

    /// Look up a cached query result.
    ///
    /// @return The cached result if found and not expired, nullopt otherwise.
    [[nodiscard]] std::optional<cgs::foundation::QueryResult> get(
        std::string_view sql);

    /// Store a query result in the cache.
    void put(std::string_view sql,
             const cgs::foundation::QueryResult& result);

    /// Store a query result with a custom TTL.
    void put(std::string_view sql,
             const cgs::foundation::QueryResult& result,
             std::chrono::seconds ttl);

    /// Invalidate a specific cached query.
    ///
    /// @return true if the entry was found and removed.
    bool invalidate(std::string_view sql);

    /// Invalidate all cached queries that reference the given table name.
    ///
    /// Uses a simple substring match on the SQL key to detect table
    /// references. This is a conservative approach that may invalidate
    /// more entries than strictly necessary, ensuring correctness.
    ///
    /// @return Number of entries invalidated.
    std::size_t invalidateByTable(std::string_view tableName);

    /// Remove all entries from the cache.
    void clear();

    /// Get the current number of cached entries.
    [[nodiscard]] std::size_t size() const;

    /// Get cumulative hit count.
    [[nodiscard]] uint64_t hitCount() const;

    /// Get cumulative miss count.
    [[nodiscard]] uint64_t missCount() const;

    /// Get cache hit rate (0.0 to 1.0).
    [[nodiscard]] double hitRate() const;

    /// Get the cache configuration.
    [[nodiscard]] const CacheConfig& config() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cgs::service
