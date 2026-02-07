#pragma once

/// @file game_database.hpp
/// @brief GameDatabase wrapping kcenon database_system for game-specific
///        database access with connection pooling and RAII transactions.
///
/// Provides synchronous and asynchronous query execution, prepared statements,
/// transaction support with RAII guards, and a configurable connection pool.
/// Part of the Database System Adapter (SDS-MOD-005).

#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "cgs/foundation/game_result.hpp"

namespace cgs::foundation {

// ── Type aliases for database values ────────────────────────────────────────

/// Sentinel type representing SQL NULL.
struct DbNull {};

/// A single column value in a query result row.
using DbValue = std::variant<DbNull, std::string, std::int64_t, double, bool>;

/// A single row: column name → value.
using DbRow = std::unordered_map<std::string, DbValue>;

/// Complete result set from a SELECT query.
using QueryResult = std::vector<DbRow>;

// ── Database types ──────────────────────────────────────────────────────────

/// Supported database backend types.
enum class DatabaseType : uint8_t {
    PostgreSQL,
    MySQL,
    SQLite
};

/// Configuration for GameDatabase connection pool.
struct DatabaseConfig {
    std::string connectionString;
    DatabaseType dbType = DatabaseType::PostgreSQL;
    uint32_t minConnections = 2;
    uint32_t maxConnections = 10;
    std::chrono::seconds connectionTimeout{30};
};

// ── PreparedStatement ───────────────────────────────────────────────────────

/// A parameterized SQL statement with named parameter binding.
///
/// Parameters are specified as $name placeholders in the SQL string and bound
/// via the bind() methods. Call resolve() to obtain the final SQL with
/// parameters substituted.
///
/// Example:
/// @code
///   auto stmt = db.prepare("SELECT * FROM players WHERE level > $min_level");
///   if (stmt.hasValue()) {
///       stmt.value().bindInt("min_level", 10);
///       auto result = db.execute(stmt.value());
///   }
/// @endcode
class PreparedStatement {
public:
    explicit PreparedStatement(std::string sql);

    /// Bind a string parameter.
    PreparedStatement& bindString(std::string_view name, std::string value);

    /// Bind an integer parameter.
    PreparedStatement& bindInt(std::string_view name, std::int64_t value);

    /// Bind a floating-point parameter.
    PreparedStatement& bindDouble(std::string_view name, double value);

    /// Bind a boolean parameter.
    PreparedStatement& bindBool(std::string_view name, bool value);

    /// Bind a NULL parameter.
    PreparedStatement& bindNull(std::string_view name);

    /// Get the original SQL template.
    [[nodiscard]] std::string_view sql() const noexcept;

    /// Resolve the SQL template with all bound parameters substituted.
    [[nodiscard]] std::string resolve() const;

    /// Clear all bound parameters.
    void clearBindings();

private:
    std::string sql_;
    std::unordered_map<std::string, DbValue> params_;
};

// ── Transaction ─────────────────────────────────────────────────────────────

/// RAII transaction guard.
///
/// Automatically rolls back on destruction if neither commit() nor rollback()
/// has been called. Queries executed through the transaction use the same
/// underlying connection for consistency.
///
/// Example:
/// @code
///   auto txn = db.beginTransaction();
///   if (txn.hasValue()) {
///       txn.value().execute("INSERT INTO items VALUES (...)");
///       txn.value().execute("UPDATE players SET ...");
///       txn.value().commit();
///   }
/// @endcode
class Transaction {
public:
    ~Transaction();

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    Transaction(Transaction&&) noexcept;
    Transaction& operator=(Transaction&&) noexcept;

    /// Commit the transaction.
    [[nodiscard]] GameResult<void> commit();

    /// Rollback the transaction.
    [[nodiscard]] GameResult<void> rollback();

    /// Execute a query within this transaction.
    [[nodiscard]] GameResult<QueryResult> query(std::string_view sql);

    /// Execute a command (INSERT/UPDATE/DELETE) within this transaction.
    [[nodiscard]] GameResult<uint64_t> execute(std::string_view sql);

    /// Check if the transaction is still active (not committed/rolled back).
    [[nodiscard]] bool isActive() const noexcept;

private:
    friend class GameDatabase;
    struct Impl;
    explicit Transaction(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

// ── GameDatabase ────────────────────────────────────────────────────────────

/// Database adapter wrapping kcenon's database_system with connection pooling.
///
/// Manages a pool of database connections and provides synchronous and
/// asynchronous query execution, prepared statements, and RAII transactions.
/// Uses PIMPL to hide all kcenon implementation details.
///
/// Example:
/// @code
///   GameDatabase db;
///   DatabaseConfig config;
///   config.connectionString = "host=localhost dbname=gamedb";
///   config.dbType = DatabaseType::PostgreSQL;
///   config.minConnections = 2;
///   config.maxConnections = 8;
///
///   auto result = db.connect(config);
///   if (result.hasValue()) {
///       auto rows = db.query("SELECT * FROM players LIMIT 10");
///   }
/// @endcode
class GameDatabase {
public:
    GameDatabase();
    ~GameDatabase();

    GameDatabase(const GameDatabase&) = delete;
    GameDatabase& operator=(const GameDatabase&) = delete;
    GameDatabase(GameDatabase&&) noexcept;
    GameDatabase& operator=(GameDatabase&&) noexcept;

    // ── Connection management ───────────────────────────────────────────

    /// Connect to the database and initialize the connection pool.
    [[nodiscard]] GameResult<void> connect(const DatabaseConfig& config);

    /// Disconnect all pooled connections.
    void disconnect();

    /// Check if the pool has at least one active connection.
    [[nodiscard]] bool isConnected() const noexcept;

    // ── Synchronous queries ─────────────────────────────────────────────

    /// Execute a SELECT query and return the result set.
    [[nodiscard]] GameResult<QueryResult> query(std::string_view sql);

    /// Execute a command (INSERT/UPDATE/DELETE/DDL) and return affected rows.
    [[nodiscard]] GameResult<uint64_t> execute(std::string_view sql);

    // ── Asynchronous queries ────────────────────────────────────────────

    /// Execute a SELECT query asynchronously and return a future.
    [[nodiscard]] std::future<GameResult<QueryResult>> queryAsync(
        std::string_view sql);

    // ── Prepared statements ─────────────────────────────────────────────

    /// Create a prepared statement from an SQL template.
    [[nodiscard]] GameResult<PreparedStatement> prepare(std::string_view sql);

    /// Execute a prepared statement and return the result set.
    [[nodiscard]] GameResult<QueryResult> execute(const PreparedStatement& stmt);

    // ── Transactions ────────────────────────────────────────────────────

    /// Begin a new transaction with a dedicated connection.
    [[nodiscard]] GameResult<Transaction> beginTransaction();

    // ── Pool information ────────────────────────────────────────────────

    /// Number of connections currently in use.
    [[nodiscard]] std::size_t activeConnections() const noexcept;

    /// Total number of connections in the pool.
    [[nodiscard]] std::size_t poolSize() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cgs::foundation
