#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

#include "cgs/foundation/database_adapter.hpp"
#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_error.hpp"

using namespace cgs::foundation;

// ===========================================================================
// ErrorCode: Database subsystem lookup
// ===========================================================================

TEST(DatabaseErrorCodeTest, SubsystemLookup) {
    EXPECT_EQ(errorSubsystem(ErrorCode::DatabaseError), "Database");
    EXPECT_EQ(errorSubsystem(ErrorCode::QueryFailed), "Database");
    EXPECT_EQ(errorSubsystem(ErrorCode::TransactionFailed), "Database");
    EXPECT_EQ(errorSubsystem(ErrorCode::ConnectionPoolExhausted), "Database");
    EXPECT_EQ(errorSubsystem(ErrorCode::ConnectionPoolTimeout), "Database");
    EXPECT_EQ(errorSubsystem(ErrorCode::NotConnected), "Database");
    EXPECT_EQ(errorSubsystem(ErrorCode::PreparedStatementFailed), "Database");
}

TEST(DatabaseErrorCodeTest, GameErrorSubsystem) {
    GameError err(ErrorCode::QueryFailed, "test error");
    EXPECT_EQ(err.subsystem(), "Database");
    EXPECT_EQ(err.message(), "test error");
}

// ===========================================================================
// DatabaseConfig: default values
// ===========================================================================

TEST(DatabaseConfigTest, DefaultValues) {
    DatabaseConfig config;
    EXPECT_TRUE(config.connectionString.empty());
    EXPECT_EQ(config.dbType, DatabaseType::PostgreSQL);
    EXPECT_EQ(config.minConnections, 2u);
    EXPECT_EQ(config.maxConnections, 10u);
    EXPECT_EQ(config.connectionTimeout, std::chrono::seconds(30));
}

TEST(DatabaseConfigTest, CustomValues) {
    DatabaseConfig config;
    config.connectionString = "host=localhost dbname=gamedb";
    config.dbType = DatabaseType::SQLite;
    config.minConnections = 1;
    config.maxConnections = 5;
    config.connectionTimeout = std::chrono::seconds(10);

    EXPECT_EQ(config.connectionString, "host=localhost dbname=gamedb");
    EXPECT_EQ(config.dbType, DatabaseType::SQLite);
    EXPECT_EQ(config.minConnections, 1u);
    EXPECT_EQ(config.maxConnections, 5u);
    EXPECT_EQ(config.connectionTimeout, std::chrono::seconds(10));
}

// ===========================================================================
// PreparedStatement: parameter binding and resolve
// ===========================================================================

TEST(PreparedStatementTest, BasicResolve) {
    PreparedStatement stmt("SELECT * FROM players WHERE id = $id");
    stmt.bindInt("id", 42);
    EXPECT_EQ(stmt.resolve(), "SELECT * FROM players WHERE id = 42");
}

TEST(PreparedStatementTest, StringBinding) {
    PreparedStatement stmt("SELECT * FROM players WHERE name = $name");
    stmt.bindString("name", "Alice");
    EXPECT_EQ(stmt.resolve(), "SELECT * FROM players WHERE name = 'Alice'");
}

TEST(PreparedStatementTest, StringBindingEscapesSingleQuotes) {
    PreparedStatement stmt("SELECT * FROM players WHERE name = $name");
    stmt.bindString("name", "O'Brien");
    EXPECT_EQ(stmt.resolve(),
              "SELECT * FROM players WHERE name = 'O''Brien'");
}

TEST(PreparedStatementTest, BoolBinding) {
    PreparedStatement stmt("SELECT * FROM players WHERE active = $active");
    stmt.bindBool("active", true);
    EXPECT_EQ(stmt.resolve(), "SELECT * FROM players WHERE active = TRUE");
}

TEST(PreparedStatementTest, NullBinding) {
    PreparedStatement stmt("UPDATE players SET guild = $guild WHERE id = $id");
    stmt.bindNull("guild");
    stmt.bindInt("id", 1);
    EXPECT_EQ(stmt.resolve(),
              "UPDATE players SET guild = NULL WHERE id = 1");
}

TEST(PreparedStatementTest, DoubleBinding) {
    PreparedStatement stmt("SELECT * FROM zones WHERE radius > $r");
    stmt.bindDouble("r", 3.14);
    auto resolved = stmt.resolve();
    // std::to_string(3.14) may produce "3.140000"
    EXPECT_TRUE(resolved.find("3.14") != std::string::npos);
}

TEST(PreparedStatementTest, MultipleParameters) {
    PreparedStatement stmt(
        "SELECT * FROM players WHERE level >= $min AND level <= $max");
    stmt.bindInt("min", 10);
    stmt.bindInt("max", 50);
    EXPECT_EQ(stmt.resolve(),
              "SELECT * FROM players WHERE level >= 10 AND level <= 50");
}

TEST(PreparedStatementTest, NoParameters) {
    PreparedStatement stmt("SELECT COUNT(*) FROM players");
    EXPECT_EQ(stmt.resolve(), "SELECT COUNT(*) FROM players");
}

TEST(PreparedStatementTest, UnboundParameterLeftAsIs) {
    PreparedStatement stmt("SELECT * FROM players WHERE id = $id");
    // Don't bind $id
    EXPECT_EQ(stmt.resolve(), "SELECT * FROM players WHERE id = $id");
}

TEST(PreparedStatementTest, ClearBindings) {
    PreparedStatement stmt("SELECT * FROM players WHERE id = $id");
    stmt.bindInt("id", 42);
    EXPECT_EQ(stmt.resolve(), "SELECT * FROM players WHERE id = 42");

    stmt.clearBindings();
    EXPECT_EQ(stmt.resolve(), "SELECT * FROM players WHERE id = $id");
}

TEST(PreparedStatementTest, SqlAccessor) {
    PreparedStatement stmt("SELECT 1");
    EXPECT_EQ(stmt.sql(), "SELECT 1");
}

TEST(PreparedStatementTest, ChainedBinding) {
    PreparedStatement stmt("INSERT INTO t VALUES ($a, $b, $c)");
    stmt.bindInt("a", 1).bindString("b", "x").bindBool("c", false);
    EXPECT_EQ(stmt.resolve(), "INSERT INTO t VALUES (1, 'x', FALSE)");
}

TEST(PreparedStatementTest, PrefixSafeParameterMatching) {
    // $min should not match $min_level
    PreparedStatement stmt(
        "SELECT * FROM p WHERE level > $min AND exp > $min_level");
    stmt.bindInt("min", 5);
    stmt.bindInt("min_level", 100);
    auto resolved = stmt.resolve();
    EXPECT_TRUE(resolved.find("level > 5") != std::string::npos);
    EXPECT_TRUE(resolved.find("exp > 100") != std::string::npos);
}

// ===========================================================================
// GameDatabase: construction and move
// ===========================================================================

TEST(GameDatabaseTest, DefaultConstruction) {
    GameDatabase db;
    EXPECT_FALSE(db.isConnected());
    EXPECT_EQ(db.poolSize(), 0u);
    EXPECT_EQ(db.activeConnections(), 0u);
}

TEST(GameDatabaseTest, MoveConstruction) {
    GameDatabase a;
    GameDatabase b(std::move(a));
    EXPECT_FALSE(b.isConnected());
}

TEST(GameDatabaseTest, MoveAssignment) {
    GameDatabase a;
    GameDatabase b;
    b = std::move(a);
    EXPECT_FALSE(b.isConnected());
}

// ===========================================================================
// GameDatabase: operations when not connected
// ===========================================================================

TEST(GameDatabaseTest, QueryWhenNotConnected) {
    GameDatabase db;
    auto result = db.query("SELECT 1");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::NotConnected);
}

TEST(GameDatabaseTest, ExecuteWhenNotConnected) {
    GameDatabase db;
    auto result = db.execute("INSERT INTO t VALUES (1)");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::NotConnected);
}

TEST(GameDatabaseTest, PrepareWhenNotConnected) {
    GameDatabase db;
    auto result = db.prepare("SELECT $id");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::NotConnected);
}

TEST(GameDatabaseTest, BeginTransactionWhenNotConnected) {
    GameDatabase db;
    auto result = db.beginTransaction();
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::NotConnected);
}

// ===========================================================================
// GameDatabase: disconnect when not connected (no-op)
// ===========================================================================

TEST(GameDatabaseTest, DisconnectWhenNotConnected) {
    GameDatabase db;
    db.disconnect(); // Should not crash
    EXPECT_FALSE(db.isConnected());
}

// ===========================================================================
// Transaction: inactive operations
// ===========================================================================

TEST(TransactionTest, MoveConstruction) {
    // Verify Transaction is move-constructible (compile-time check)
    static_assert(std::is_move_constructible_v<Transaction>);
    static_assert(!std::is_copy_constructible_v<Transaction>);
}

// ===========================================================================
// DbValue: variant type checks
// ===========================================================================

TEST(DbValueTest, NullType) {
    DbValue val = DbNull{};
    EXPECT_TRUE(std::holds_alternative<DbNull>(val));
}

TEST(DbValueTest, StringType) {
    DbValue val = std::string("hello");
    EXPECT_TRUE(std::holds_alternative<std::string>(val));
    EXPECT_EQ(std::get<std::string>(val), "hello");
}

TEST(DbValueTest, IntType) {
    DbValue val = std::int64_t{42};
    EXPECT_TRUE(std::holds_alternative<std::int64_t>(val));
    EXPECT_EQ(std::get<std::int64_t>(val), 42);
}

TEST(DbValueTest, DoubleType) {
    DbValue val = 3.14;
    EXPECT_TRUE(std::holds_alternative<double>(val));
    EXPECT_DOUBLE_EQ(std::get<double>(val), 3.14);
}

TEST(DbValueTest, BoolType) {
    DbValue val = true;
    EXPECT_TRUE(std::holds_alternative<bool>(val));
    EXPECT_TRUE(std::get<bool>(val));
}

// ===========================================================================
// Aggregate header test
// ===========================================================================

TEST(DatabaseAdapterTest, AggregateHeaderIncludesAll) {
    // Verify that database_adapter.hpp includes GameDatabase and related types
    GameDatabase db;
    PreparedStatement stmt("SELECT 1");
    DatabaseConfig config;
    EXPECT_FALSE(db.isConnected());
    EXPECT_EQ(stmt.sql(), "SELECT 1");
    EXPECT_EQ(config.minConnections, 2u);
}
