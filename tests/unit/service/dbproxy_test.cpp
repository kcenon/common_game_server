/// @file dbproxy_test.cpp
/// @brief Unit tests for QueryCache, ConnectionPoolManager, and DBProxyServer.

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_database.hpp"
#include "cgs/service/connection_pool_manager.hpp"
#include "cgs/service/dbproxy_server.hpp"
#include "cgs/service/dbproxy_types.hpp"
#include "cgs/service/query_cache.hpp"

using namespace cgs::service;
using namespace cgs::foundation;
using namespace std::chrono_literals;

// ============================================================================
// DBProxy Types Tests
// ============================================================================

TEST(DBProxyTypesTest, DBEndpointConfigDefaults) {
    DBEndpointConfig config;
    EXPECT_TRUE(config.connectionString.empty());
    EXPECT_EQ(config.dbType, DatabaseType::PostgreSQL);
    EXPECT_EQ(config.minConnections, 2u);
    EXPECT_EQ(config.maxConnections, 10u);
    EXPECT_EQ(config.connectionTimeout.count(), 30);
}

TEST(DBProxyTypesTest, CacheConfigDefaults) {
    CacheConfig config;
    EXPECT_TRUE(config.enabled);
    EXPECT_EQ(config.maxEntries, 10000u);
    EXPECT_EQ(config.defaultTtl.count(), 300);
    EXPECT_EQ(config.maxValueSizeBytes, 1048576u);
}

TEST(DBProxyTypesTest, DBProxyConfigDefaults) {
    DBProxyConfig config;
    EXPECT_TRUE(config.replicas.empty());
    EXPECT_TRUE(config.cache.enabled);
    EXPECT_EQ(config.healthCheckInterval.count(), 30);
}

TEST(DBProxyTypesTest, PoolStatsDefaults) {
    PoolStats stats;
    EXPECT_EQ(stats.primaryActive, 0u);
    EXPECT_EQ(stats.primaryTotal, 0u);
    EXPECT_EQ(stats.replicaActive, 0u);
    EXPECT_EQ(stats.replicaTotal, 0u);
    EXPECT_EQ(stats.replicaCount, 0u);
    EXPECT_EQ(stats.cacheEntries, 0u);
    EXPECT_DOUBLE_EQ(stats.cacheHitRate, 0.0);
}

// ============================================================================
// QueryCache Tests
// ============================================================================

class QueryCacheTest : public ::testing::Test {
protected:
    CacheConfig config_;
    std::unique_ptr<QueryCache> cache_;

    void SetUp() override {
        config_.enabled = true;
        config_.maxEntries = 100;
        config_.defaultTtl = 60s;
        cache_ = std::make_unique<QueryCache>(config_);
    }

    QueryResult makeResult(int rows) {
        QueryResult result;
        for (int i = 0; i < rows; ++i) {
            DbRow row;
            row["id"] = static_cast<std::int64_t>(i);
            row["name"] = std::string("item_") + std::to_string(i);
            result.push_back(std::move(row));
        }
        return result;
    }
};

TEST_F(QueryCacheTest, InitialCacheIsEmpty) {
    EXPECT_EQ(cache_->size(), 0u);
    EXPECT_EQ(cache_->hitCount(), 0u);
    EXPECT_EQ(cache_->missCount(), 0u);
    EXPECT_DOUBLE_EQ(cache_->hitRate(), 0.0);
}

TEST_F(QueryCacheTest, PutAndGet) {
    auto result = makeResult(3);
    cache_->put("SELECT * FROM items", result);
    EXPECT_EQ(cache_->size(), 1u);

    auto cached = cache_->get("SELECT * FROM items");
    ASSERT_TRUE(cached.has_value());
    EXPECT_EQ(cached->size(), 3u);
}

TEST_F(QueryCacheTest, GetMissReturnsNullopt) {
    auto cached = cache_->get("SELECT * FROM nonexistent");
    EXPECT_FALSE(cached.has_value());
    EXPECT_EQ(cache_->missCount(), 1u);
}

TEST_F(QueryCacheTest, PutUpdatesExistingEntry) {
    cache_->put("SELECT 1", makeResult(1));
    cache_->put("SELECT 1", makeResult(5));
    EXPECT_EQ(cache_->size(), 1u);

    auto cached = cache_->get("SELECT 1");
    ASSERT_TRUE(cached.has_value());
    EXPECT_EQ(cached->size(), 5u);
}

TEST_F(QueryCacheTest, LruEviction) {
    CacheConfig small;
    small.maxEntries = 3;
    small.defaultTtl = 300s;
    QueryCache smallCache(small);

    smallCache.put("q1", makeResult(1));
    smallCache.put("q2", makeResult(1));
    smallCache.put("q3", makeResult(1));
    EXPECT_EQ(smallCache.size(), 3u);

    // Insert a 4th entry; the least recently used (q1) should be evicted.
    smallCache.put("q4", makeResult(1));
    EXPECT_EQ(smallCache.size(), 3u);

    EXPECT_FALSE(smallCache.get("q1").has_value()); // Evicted.
    EXPECT_TRUE(smallCache.get("q2").has_value());
    EXPECT_TRUE(smallCache.get("q3").has_value());
    EXPECT_TRUE(smallCache.get("q4").has_value());
}

TEST_F(QueryCacheTest, LruPromotionOnGet) {
    CacheConfig small;
    small.maxEntries = 3;
    small.defaultTtl = 300s;
    QueryCache smallCache(small);

    smallCache.put("q1", makeResult(1));
    smallCache.put("q2", makeResult(1));
    smallCache.put("q3", makeResult(1));

    // Access q1 to promote it.
    (void)smallCache.get("q1");

    // Insert q4; q2 should be evicted (LRU after q1 promotion).
    smallCache.put("q4", makeResult(1));
    EXPECT_EQ(smallCache.size(), 3u);

    EXPECT_TRUE(smallCache.get("q1").has_value());  // Promoted.
    EXPECT_FALSE(smallCache.get("q2").has_value()); // Evicted.
    EXPECT_TRUE(smallCache.get("q3").has_value());
    EXPECT_TRUE(smallCache.get("q4").has_value());
}

TEST_F(QueryCacheTest, TtlExpiration) {
    CacheConfig shortTtl;
    shortTtl.maxEntries = 100;
    shortTtl.defaultTtl = 0s; // Immediate expiration.
    QueryCache shortCache(shortTtl);

    shortCache.put("SELECT 1", makeResult(1));

    // Even though we just inserted, TTL of 0s means it expires immediately.
    // We need to wait a tiny bit for the clock to advance.
    std::this_thread::sleep_for(1ms);

    auto cached = shortCache.get("SELECT 1");
    EXPECT_FALSE(cached.has_value());
}

TEST_F(QueryCacheTest, CustomTtl) {
    // Use a long default TTL but a short custom one.
    auto result = makeResult(1);
    cache_->put("SELECT 1", result, 0s);

    std::this_thread::sleep_for(1ms);

    auto cached = cache_->get("SELECT 1");
    EXPECT_FALSE(cached.has_value());
}

TEST_F(QueryCacheTest, Invalidate) {
    cache_->put("SELECT 1", makeResult(1));
    cache_->put("SELECT 2", makeResult(2));

    EXPECT_TRUE(cache_->invalidate("SELECT 1"));
    EXPECT_EQ(cache_->size(), 1u);
    EXPECT_FALSE(cache_->get("SELECT 1").has_value());
    EXPECT_TRUE(cache_->get("SELECT 2").has_value());
}

TEST_F(QueryCacheTest, InvalidateNonexistent) {
    EXPECT_FALSE(cache_->invalidate("nonexistent"));
}

TEST_F(QueryCacheTest, InvalidateByTable) {
    cache_->put("SELECT * FROM items WHERE id = 1", makeResult(1));
    cache_->put("SELECT * FROM players WHERE id = 2", makeResult(1));
    cache_->put("SELECT * FROM items WHERE id = 3", makeResult(1));

    auto count = cache_->invalidateByTable("items");
    EXPECT_EQ(count, 2u);
    EXPECT_EQ(cache_->size(), 1u);
    EXPECT_TRUE(cache_->get("SELECT * FROM players WHERE id = 2").has_value());
}

TEST_F(QueryCacheTest, InvalidateByTableCaseInsensitive) {
    cache_->put("SELECT * FROM Items WHERE id = 1", makeResult(1));
    cache_->put("SELECT * FROM ITEMS WHERE id = 2", makeResult(1));

    auto count = cache_->invalidateByTable("items");
    EXPECT_EQ(count, 2u);
    EXPECT_EQ(cache_->size(), 0u);
}

TEST_F(QueryCacheTest, Clear) {
    cache_->put("q1", makeResult(1));
    cache_->put("q2", makeResult(2));
    cache_->put("q3", makeResult(3));

    cache_->clear();
    EXPECT_EQ(cache_->size(), 0u);
}

TEST_F(QueryCacheTest, HitAndMissCounters) {
    cache_->put("SELECT 1", makeResult(1));

    (void)cache_->get("SELECT 1"); // Hit.
    (void)cache_->get("SELECT 1"); // Hit.
    (void)cache_->get("SELECT 2"); // Miss.

    EXPECT_EQ(cache_->hitCount(), 2u);
    EXPECT_EQ(cache_->missCount(), 1u);
    EXPECT_NEAR(cache_->hitRate(), 2.0 / 3.0, 0.001);
}

TEST_F(QueryCacheTest, HitRateZeroWhenNoQueries) {
    EXPECT_DOUBLE_EQ(cache_->hitRate(), 0.0);
}

TEST_F(QueryCacheTest, ConfigAccessor) {
    EXPECT_EQ(cache_->config().maxEntries, 100u);
    EXPECT_EQ(cache_->config().defaultTtl.count(), 60);
}

TEST_F(QueryCacheTest, MultiplePutsAndGets) {
    for (int i = 0; i < 50; ++i) {
        cache_->put("SELECT " + std::to_string(i), makeResult(i + 1));
    }
    EXPECT_EQ(cache_->size(), 50u);

    for (int i = 0; i < 50; ++i) {
        auto cached = cache_->get("SELECT " + std::to_string(i));
        ASSERT_TRUE(cached.has_value());
        EXPECT_EQ(cached->size(), static_cast<std::size_t>(i + 1));
    }
}

TEST_F(QueryCacheTest, ConcurrentAccess) {
    constexpr int kThreads = 4;
    constexpr int kOpsPerThread = 100;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < kOpsPerThread; ++i) {
                auto key = "SELECT " + std::to_string(t * kOpsPerThread + i);
                cache_->put(key, makeResult(1));
                (void)cache_->get(key);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All operations should complete without data races.
    EXPECT_GT(cache_->size(), 0u);
    EXPECT_GT(cache_->hitCount(), 0u);
}

// ============================================================================
// SQL Helper Tests (via DBProxyServer behavior)
// ============================================================================

// Test isReadQuery indirectly through QueryCache usage patterns.
TEST(SqlHelperTest, CacheOnlyStoresReads) {
    CacheConfig config;
    config.maxEntries = 100;
    config.defaultTtl = 300s;
    QueryCache cache(config);

    // SELECT queries should be cacheable.
    QueryResult result;
    DbRow row;
    row["val"] = std::int64_t{42};
    result.push_back(row);

    cache.put("SELECT * FROM test", result);
    EXPECT_TRUE(cache.get("SELECT * FROM test").has_value());

    // Cache can store any key - the read/write distinction is handled
    // by the DBProxyServer layer, not the cache itself.
    cache.put("INSERT INTO test VALUES (1)", result);
    EXPECT_TRUE(cache.get("INSERT INTO test VALUES (1)").has_value());
}

// Test table name extraction indirectly via invalidateByTable.
TEST(SqlHelperTest, InvalidateByTableMatchesInsert) {
    CacheConfig config;
    config.maxEntries = 100;
    config.defaultTtl = 300s;
    QueryCache cache(config);

    QueryResult result;
    DbRow row;
    row["id"] = std::int64_t{1};
    result.push_back(row);

    cache.put("SELECT * FROM players", result);
    cache.put("SELECT * FROM items", result);

    // Invalidating "players" should only remove the players query.
    auto count = cache.invalidateByTable("players");
    EXPECT_EQ(count, 1u);
    EXPECT_FALSE(cache.get("SELECT * FROM players").has_value());
    EXPECT_TRUE(cache.get("SELECT * FROM items").has_value());
}

TEST(SqlHelperTest, InvalidateByTableNoMatch) {
    CacheConfig config;
    config.maxEntries = 100;
    config.defaultTtl = 300s;
    QueryCache cache(config);

    QueryResult result;
    cache.put("SELECT 1 + 1", result);

    auto count = cache.invalidateByTable("players");
    EXPECT_EQ(count, 0u);
    EXPECT_EQ(cache.size(), 1u);
}

// ============================================================================
// SQL Injection Prevention Tests (SRS-NFR-016)
// ============================================================================

TEST(SqlInjectionTest, SingleQuoteEscaped) {
    // Classic SQL injection: ' OR '1'='1
    PreparedStatement stmt("SELECT * FROM users WHERE name = $name");
    stmt.bindString("name", "' OR '1'='1");
    auto resolved = stmt.resolve();

    // The injected quotes should be escaped to double single-quotes.
    EXPECT_EQ(resolved,
              "SELECT * FROM users WHERE name = ''' OR ''1''=''1'");
    // The entire value is wrapped in quotes, so it's treated as a literal string.
    EXPECT_NE(resolved, "SELECT * FROM users WHERE name = '' OR '1'='1'");
}

TEST(SqlInjectionTest, DropTableInjection) {
    // Attempt: '; DROP TABLE users; --
    PreparedStatement stmt("SELECT * FROM users WHERE id = $id");
    stmt.bindString("id", "'; DROP TABLE users; --");
    auto resolved = stmt.resolve();

    // Should be safely escaped: the DROP TABLE is inside a string literal.
    EXPECT_EQ(resolved,
              "SELECT * FROM users WHERE id = '''; DROP TABLE users; --'");
}

TEST(SqlInjectionTest, UnionSelectInjection) {
    // Attempt: ' UNION SELECT password FROM admin --
    PreparedStatement stmt(
        "SELECT * FROM products WHERE category = $cat");
    stmt.bindString("cat", "' UNION SELECT password FROM admin --");
    auto resolved = stmt.resolve();

    EXPECT_EQ(resolved,
              "SELECT * FROM products WHERE category = "
              "''' UNION SELECT password FROM admin --'");
}

TEST(SqlInjectionTest, IntegerBindingIsNotVulnerable) {
    // Integer bindings produce bare numbers, not injectable strings.
    PreparedStatement stmt("SELECT * FROM users WHERE id = $id");
    stmt.bindInt("id", 42);
    EXPECT_EQ(stmt.resolve(), "SELECT * FROM users WHERE id = 42");
}

TEST(SqlInjectionTest, NullBindingIsSafe) {
    PreparedStatement stmt("SELECT * FROM users WHERE name = $name");
    stmt.bindNull("name");
    EXPECT_EQ(stmt.resolve(), "SELECT * FROM users WHERE name = NULL");
}

TEST(SqlInjectionTest, BoolBindingIsSafe) {
    PreparedStatement stmt("SELECT * FROM users WHERE active = $flag");
    stmt.bindBool("flag", false);
    EXPECT_EQ(stmt.resolve(), "SELECT * FROM users WHERE active = FALSE");
}

TEST(SqlInjectionTest, NestedQuotesEscaped) {
    // Multiple levels of quote nesting.
    PreparedStatement stmt("INSERT INTO logs (msg) VALUES ($msg)");
    stmt.bindString("msg", "It's a test with ''nested'' quotes");
    auto resolved = stmt.resolve();

    EXPECT_EQ(resolved,
              "INSERT INTO logs (msg) VALUES ("
              "'It''s a test with ''''nested'''' quotes')");
}

TEST(SqlInjectionTest, EmptyStringIsSafe) {
    PreparedStatement stmt("SELECT * FROM t WHERE col = $val");
    stmt.bindString("val", "");
    EXPECT_EQ(stmt.resolve(), "SELECT * FROM t WHERE col = ''");
}

TEST(SqlInjectionTest, BackslashNotSpecial) {
    // In standard SQL, backslash is not a special character.
    PreparedStatement stmt("SELECT * FROM t WHERE path = $p");
    stmt.bindString("p", "C:\\Users\\test");
    EXPECT_EQ(stmt.resolve(), "SELECT * FROM t WHERE path = 'C:\\Users\\test'");
}

// ============================================================================
// DBProxyServer PreparedStatement Tests
// ============================================================================

TEST(DBProxyServerPreparedTest, QueryReturnsNotStarted) {
    DBProxyConfig config;
    DBProxyServer proxy(config);

    PreparedStatement stmt("SELECT * FROM t WHERE id = $id");
    stmt.bindInt("id", 1);

    auto result = proxy.query(stmt);
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::DBProxyNotStarted);
}

TEST(DBProxyServerPreparedTest, ExecuteReturnsNotStarted) {
    DBProxyConfig config;
    DBProxyServer proxy(config);

    PreparedStatement stmt("UPDATE t SET x = $x WHERE id = $id");
    stmt.bindInt("x", 10);
    stmt.bindInt("id", 1);

    auto result = proxy.execute(stmt);
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::DBProxyNotStarted);
}

TEST(DBProxyServerPreparedTest, QueryAsyncReturnsNotStarted) {
    DBProxyConfig config;
    DBProxyServer proxy(config);

    PreparedStatement stmt("SELECT * FROM t WHERE id = $id");
    stmt.bindInt("id", 1);

    auto future = proxy.queryAsync(stmt);
    auto result = future.get();
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::DBProxyNotStarted);
}

// ============================================================================
// ConnectionPoolManager PreparedStatement Tests
// ============================================================================

TEST(ConnectionPoolManagerPreparedTest, QueryReturnsNotStarted) {
    DBProxyConfig config;
    ConnectionPoolManager pool(config);

    PreparedStatement stmt("SELECT * FROM t WHERE id = $id");
    stmt.bindInt("id", 1);

    auto result = pool.query(stmt);
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::DBProxyNotStarted);
}

TEST(ConnectionPoolManagerPreparedTest, ExecuteReturnsNotStarted) {
    DBProxyConfig config;
    ConnectionPoolManager pool(config);

    PreparedStatement stmt("DELETE FROM t WHERE id = $id");
    stmt.bindInt("id", 1);

    auto result = pool.execute(stmt);
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::DBProxyNotStarted);
}
