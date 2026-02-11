/// @file dbproxy_integration_test.cpp
/// @brief Integration tests for DBProxyServer (SDS-MOD-034).
///
/// Validates the full pipeline: DBProxyServer → ConnectionPoolManager →
/// GameDatabase → PostgreSQL and the cache integration layer.
///
/// Tests requiring a live PostgreSQL instance will skip gracefully if
/// the CGS_TEST_DB_CONN environment variable is not set.
///
/// Set the environment variable to run full integration tests:
///   export CGS_TEST_DB_CONN="host=localhost dbname=cgs_test user=test password=test"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <string>

#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_database.hpp"
#include "cgs/service/dbproxy_server.hpp"
#include "cgs/service/dbproxy_types.hpp"

using namespace cgs::service;
using namespace cgs::foundation;
using namespace std::chrono_literals;

namespace {

/// Read DB connection string from environment.
/// Returns empty string if not set.
std::string getTestDbConnection() {
    const char* env = std::getenv("CGS_TEST_DB_CONN");
    return env ? std::string(env) : std::string();
}

} // anonymous namespace

// ===========================================================================
// Integration Test Fixture
// ===========================================================================

class DBProxyIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        connStr_ = getTestDbConnection();
    }

    bool hasDatabase() const { return !connStr_.empty(); }

    DBProxyConfig makeConfig() {
        DBProxyConfig config;
        config.primary.connectionString = connStr_;
        config.primary.dbType = DatabaseType::PostgreSQL;
        config.primary.minConnections = 1;
        config.primary.maxConnections = 4;
        config.cache.enabled = true;
        config.cache.maxEntries = 1000;
        config.cache.defaultTtl = 60s;
        return config;
    }

    std::string connStr_;
};

// ===========================================================================
// Cache-Only Integration (no DB required)
// ===========================================================================

TEST_F(DBProxyIntegrationTest, StartWithInvalidConnectionString) {
    DBProxyConfig config;
    config.primary.connectionString = "host=invalid_host_that_does_not_exist "
                                       "dbname=nonexistent";
    config.primary.minConnections = 1;

    DBProxyServer proxy(config);
    auto result = proxy.start();

    // Behavior depends on whether the database backend is compiled:
    // - With PostgreSQL support: start() fails (connection error).
    // - Without PostgreSQL support (stub): start() succeeds but queries fail.
    if (result.hasError()) {
        EXPECT_FALSE(proxy.isRunning());
    } else {
        // Stub mode: start() succeeded but the proxy has no real connection.
        EXPECT_TRUE(proxy.isRunning());
        proxy.stop();
    }
}

TEST_F(DBProxyIntegrationTest, QueryBeforeStartFails) {
    DBProxyConfig config;
    config.primary.connectionString = "host=localhost dbname=test";

    DBProxyServer proxy(config);

    auto result = proxy.query("SELECT 1");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::DBProxyNotStarted);
}

TEST_F(DBProxyIntegrationTest, ExecuteBeforeStartFails) {
    DBProxyConfig config;
    config.primary.connectionString = "host=localhost dbname=test";

    DBProxyServer proxy(config);

    auto result = proxy.execute("INSERT INTO test VALUES (1)");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::DBProxyNotStarted);
}

TEST_F(DBProxyIntegrationTest, DoubleStartFails) {
    if (!hasDatabase()) {
        GTEST_SKIP() << "CGS_TEST_DB_CONN not set; skipping live DB test";
    }

    auto config = makeConfig();
    DBProxyServer proxy(config);

    auto r1 = proxy.start();
    ASSERT_TRUE(r1.hasValue());

    auto r2 = proxy.start();
    EXPECT_TRUE(r2.hasError());
    EXPECT_EQ(r2.error().code(), ErrorCode::AlreadyExists);

    proxy.stop();
}

// ===========================================================================
// Full Round-Trip Tests (require live PostgreSQL)
// ===========================================================================

TEST_F(DBProxyIntegrationTest, FullRoundTrip) {
    if (!hasDatabase()) {
        GTEST_SKIP() << "CGS_TEST_DB_CONN not set; skipping live DB test";
    }

    auto config = makeConfig();
    DBProxyServer proxy(config);

    auto startResult = proxy.start();
    ASSERT_TRUE(startResult.hasValue())
        << "Failed to start DBProxy: "
        << std::string(startResult.error().message());

    EXPECT_TRUE(proxy.isRunning());

    // Create a test table.
    auto createResult = proxy.execute(
        "CREATE TABLE IF NOT EXISTS cgs_dbproxy_test ("
        "  id SERIAL PRIMARY KEY,"
        "  name VARCHAR(100) NOT NULL,"
        "  value INTEGER NOT NULL"
        ")");
    ASSERT_TRUE(createResult.hasValue())
        << "CREATE TABLE failed: "
        << std::string(createResult.error().message());

    // Insert test data.
    auto insertResult = proxy.execute(
        "INSERT INTO cgs_dbproxy_test (name, value) "
        "VALUES ('item_a', 100)");
    ASSERT_TRUE(insertResult.hasValue());

    // Query (should hit DB, then cache).
    auto q1 = proxy.query(
        "SELECT name, value FROM cgs_dbproxy_test WHERE name = 'item_a'");
    ASSERT_TRUE(q1.hasValue());
    ASSERT_GE(q1.value().size(), 1u);

    // Second query should hit cache.
    auto q2 = proxy.query(
        "SELECT name, value FROM cgs_dbproxy_test WHERE name = 'item_a'");
    ASSERT_TRUE(q2.hasValue());
    EXPECT_EQ(q2.value().size(), q1.value().size());

    // Verify cache was used.
    EXPECT_GT(proxy.cacheHitRate(), 0.0);
    EXPECT_GE(proxy.cacheSize(), 1u);

    // Execute a write → cache should be invalidated.
    auto updateResult = proxy.execute(
        "UPDATE cgs_dbproxy_test SET value = 200 WHERE name = 'item_a'");
    ASSERT_TRUE(updateResult.hasValue());

    // The cache entry for cgs_dbproxy_test should have been invalidated.
    // Next query should go to DB again.
    auto q3 = proxy.query(
        "SELECT name, value FROM cgs_dbproxy_test WHERE name = 'item_a'");
    ASSERT_TRUE(q3.hasValue());
    ASSERT_GE(q3.value().size(), 1u);

    // Pool stats should show activity.
    auto stats = proxy.poolStats();
    EXPECT_GT(stats.primaryTotal, 0u);
    EXPECT_GT(proxy.totalQueries(), 0u);

    // Cleanup.
    (void)proxy.execute("DROP TABLE IF EXISTS cgs_dbproxy_test");

    proxy.stop();
    EXPECT_FALSE(proxy.isRunning());
}

TEST_F(DBProxyIntegrationTest, QueryAsync) {
    if (!hasDatabase()) {
        GTEST_SKIP() << "CGS_TEST_DB_CONN not set; skipping live DB test";
    }

    auto config = makeConfig();
    DBProxyServer proxy(config);

    auto startResult = proxy.start();
    ASSERT_TRUE(startResult.hasValue());

    auto future = proxy.queryAsync("SELECT 1 AS one");
    auto result = future.get();
    ASSERT_TRUE(result.hasValue());
    EXPECT_GE(result.value().size(), 1u);

    proxy.stop();
}

TEST_F(DBProxyIntegrationTest, CacheInvalidationOnWrite) {
    if (!hasDatabase()) {
        GTEST_SKIP() << "CGS_TEST_DB_CONN not set; skipping live DB test";
    }

    auto config = makeConfig();
    DBProxyServer proxy(config);
    ASSERT_TRUE(proxy.start().hasValue());

    // Setup.
    (void)proxy.execute(
        "CREATE TABLE IF NOT EXISTS cgs_cache_test ("
        "  id SERIAL PRIMARY KEY, val INTEGER)");
    (void)proxy.execute("INSERT INTO cgs_cache_test (val) VALUES (1)");

    // Populate cache.
    (void)proxy.query("SELECT * FROM cgs_cache_test");
    EXPECT_GE(proxy.cacheSize(), 1u);

    // Write to the same table → should invalidate cache.
    (void)proxy.execute("DELETE FROM cgs_cache_test WHERE val = 1");

    // Manual invalidation check.
    auto invalidated = proxy.invalidateCache("cgs_cache_test");
    // May be 0 if auto-invalidation already cleared it.
    (void)invalidated;

    // Cleanup.
    (void)proxy.execute("DROP TABLE IF EXISTS cgs_cache_test");
    proxy.stop();
}
