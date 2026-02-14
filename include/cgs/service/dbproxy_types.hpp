#pragma once

/// @file dbproxy_types.hpp
/// @brief Core types for the Database Proxy Service.
///
/// Defines configuration for connection pooling, query caching,
/// read replica routing, and query statistics.
///
/// @see SRS-SVC-005
/// @see SDS-MOD-034

#include "cgs/foundation/game_database.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace cgs::service {

/// Configuration for a single database endpoint (primary or replica).
struct DBEndpointConfig {
    std::string connectionString;
    cgs::foundation::DatabaseType dbType = cgs::foundation::DatabaseType::PostgreSQL;
    uint32_t minConnections = 2;
    uint32_t maxConnections = 10;
    std::chrono::seconds connectionTimeout{30};
};

/// Configuration for the LRU query cache.
struct CacheConfig {
    bool enabled = true;                      ///< Enable/disable caching.
    std::size_t maxEntries = 10000;           ///< Maximum cached queries.
    std::chrono::seconds defaultTtl{300};     ///< Default TTL (5 minutes).
    std::size_t maxValueSizeBytes = 1048576;  ///< Max size per entry (1 MB).
};

/// Configuration for the DBProxy service.
struct DBProxyConfig {
    DBEndpointConfig primary;                      ///< Primary (read-write) database.
    std::vector<DBEndpointConfig> replicas;        ///< Read replicas (optional).
    CacheConfig cache;                             ///< Query cache settings.
    std::chrono::seconds healthCheckInterval{30};  ///< Health check period.
};

/// Statistics for a single query execution.
struct QueryStats {
    double latencyMs = 0.0;          ///< Query execution latency in milliseconds.
    bool cacheHit = false;           ///< Whether the result came from cache.
    bool routed_to_replica = false;  ///< Whether routed to a read replica.
};

/// Snapshot of DBProxy pool utilization.
struct PoolStats {
    std::size_t primaryActive = 0;  ///< Active connections on primary.
    std::size_t primaryTotal = 0;   ///< Total connections on primary.
    std::size_t replicaActive = 0;  ///< Active connections across replicas.
    std::size_t replicaTotal = 0;   ///< Total connections across replicas.
    std::size_t replicaCount = 0;   ///< Number of replica endpoints.
    std::size_t cacheEntries = 0;   ///< Current cache entry count.
    double cacheHitRate = 0.0;      ///< Cache hit rate (0.0 - 1.0).
};

}  // namespace cgs::service
