/// @file game_database.cpp
/// @brief GameDatabase implementation wrapping kcenon database_system.

#include "cgs/foundation/game_database.hpp"

// kcenon database_system headers (hidden behind PIMPL)
#include <database_manager.h>
#include <core/database_backend.h>
#include <core/database_context.h>
#include <database_types.h>

#include <algorithm>
#include <condition_variable>
#include <mutex>

namespace cgs::foundation {

// ---------------------------------------------------------------------------
// Helper: map CGS DatabaseType to kcenon database_types
// ---------------------------------------------------------------------------

static ::database::database_types toKcenon(DatabaseType type) {
    switch (type) {
        case DatabaseType::PostgreSQL: return ::database::database_types::postgres;
        case DatabaseType::MySQL:      return ::database::database_types::mysql;
        case DatabaseType::SQLite:     return ::database::database_types::sqlite;
    }
    return ::database::database_types::postgres;
}

// ---------------------------------------------------------------------------
// Helper: convert kcenon database_result to CGS QueryResult
// ---------------------------------------------------------------------------

static QueryResult convertResult(
    const ::database::core::database_result& kcResult) {
    QueryResult result;
    result.reserve(kcResult.size());

    for (const auto& kcRow : kcResult) {
        DbRow row;
        for (const auto& [col, val] : kcRow) {
            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::nullptr_t>) {
                    row[col] = DbNull{};
                } else if constexpr (std::is_same_v<T, std::string>) {
                    row[col] = arg;
                } else if constexpr (std::is_same_v<T, std::int64_t>) {
                    row[col] = arg;
                } else if constexpr (std::is_same_v<T, double>) {
                    row[col] = arg;
                } else if constexpr (std::is_same_v<T, bool>) {
                    row[col] = arg;
                } else {
                    row[col] = DbNull{};
                }
            }, val);
        }
        result.push_back(std::move(row));
    }
    return result;
}

// ---------------------------------------------------------------------------
// PreparedStatement
// ---------------------------------------------------------------------------

PreparedStatement::PreparedStatement(std::string sql)
    : sql_(std::move(sql)) {}

PreparedStatement& PreparedStatement::bindString(
    std::string_view name, std::string value) {
    params_[std::string(name)] = std::move(value);
    return *this;
}

PreparedStatement& PreparedStatement::bindInt(
    std::string_view name, std::int64_t value) {
    params_[std::string(name)] = value;
    return *this;
}

PreparedStatement& PreparedStatement::bindDouble(
    std::string_view name, double value) {
    params_[std::string(name)] = value;
    return *this;
}

PreparedStatement& PreparedStatement::bindBool(
    std::string_view name, bool value) {
    params_[std::string(name)] = value;
    return *this;
}

PreparedStatement& PreparedStatement::bindNull(std::string_view name) {
    params_[std::string(name)] = DbNull{};
    return *this;
}

std::string_view PreparedStatement::sql() const noexcept {
    return sql_;
}

std::string PreparedStatement::resolve() const {
    std::string resolved = sql_;

    for (const auto& [name, val] : params_) {
        std::string placeholder = "$" + name;
        std::string replacement;

        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, DbNull>) {
                replacement = "NULL";
            } else if constexpr (std::is_same_v<T, std::string>) {
                // Escape single quotes for SQL safety
                std::string escaped;
                escaped.reserve(arg.size() + 2);
                escaped += '\'';
                for (char c : arg) {
                    if (c == '\'') {
                        escaped += "''";
                    } else {
                        escaped += c;
                    }
                }
                escaped += '\'';
                replacement = std::move(escaped);
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                replacement = std::to_string(arg);
            } else if constexpr (std::is_same_v<T, double>) {
                replacement = std::to_string(arg);
            } else if constexpr (std::is_same_v<T, bool>) {
                replacement = arg ? "TRUE" : "FALSE";
            }
        }, val);

        // Replace all occurrences of $name in the SQL string
        std::size_t pos = 0;
        while ((pos = resolved.find(placeholder, pos)) != std::string::npos) {
            // Ensure we match the full parameter name (not a prefix)
            auto endPos = pos + placeholder.size();
            if (endPos < resolved.size() && (std::isalnum(resolved[endPos]) ||
                                              resolved[endPos] == '_')) {
                pos = endPos;
                continue;
            }
            resolved.replace(pos, placeholder.size(), replacement);
            pos += replacement.size();
        }
    }

    return resolved;
}

void PreparedStatement::clearBindings() {
    params_.clear();
}

// ---------------------------------------------------------------------------
// Connection pool entry
// ---------------------------------------------------------------------------

struct PooledConnection {
    std::shared_ptr<::database::database_context> context;
    std::shared_ptr<::database::database_manager> manager;
    bool inUse = false;
};

// ---------------------------------------------------------------------------
// Transaction::Impl
// ---------------------------------------------------------------------------

struct Transaction::Impl {
    std::shared_ptr<::database::database_manager> manager;
    // Callback to return the connection when the transaction ends
    std::function<void(::database::database_manager*)> returnConnection;
    bool active = true;
    bool ownsConnection = true;
};

Transaction::Transaction(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

Transaction::~Transaction() {
    if (impl_ && impl_->active) {
        // Auto-rollback on destruction
        (void)impl_->manager->rollback_transaction();
        impl_->active = false;
    }
    if (impl_ && impl_->ownsConnection && impl_->returnConnection) {
        impl_->returnConnection(impl_->manager.get());
    }
}

Transaction::Transaction(Transaction&& other) noexcept = default;
Transaction& Transaction::operator=(Transaction&& other) noexcept {
    if (this != &other) {
        if (impl_ && impl_->active) {
            (void)impl_->manager->rollback_transaction();
        }
        if (impl_ && impl_->ownsConnection && impl_->returnConnection) {
            impl_->returnConnection(impl_->manager.get());
        }
        impl_ = std::move(other.impl_);
    }
    return *this;
}

GameResult<void> Transaction::commit() {
    if (!impl_ || !impl_->active) {
        return GameResult<void>::err(
            GameError(ErrorCode::TransactionFailed, "transaction not active"));
    }

    auto result = impl_->manager->commit_transaction();
    impl_->active = false;

    if (!result.is_ok()) {
        return GameResult<void>::err(
            GameError(ErrorCode::TransactionFailed,
                      "commit failed: " + result.error().message));
    }
    return GameResult<void>::ok();
}

GameResult<void> Transaction::rollback() {
    if (!impl_ || !impl_->active) {
        return GameResult<void>::err(
            GameError(ErrorCode::TransactionFailed, "transaction not active"));
    }

    auto result = impl_->manager->rollback_transaction();
    impl_->active = false;

    if (!result.is_ok()) {
        return GameResult<void>::err(
            GameError(ErrorCode::TransactionFailed,
                      "rollback failed: " + result.error().message));
    }
    return GameResult<void>::ok();
}

GameResult<QueryResult> Transaction::query(std::string_view sql) {
    if (!impl_ || !impl_->active) {
        return GameResult<QueryResult>::err(
            GameError(ErrorCode::TransactionFailed, "transaction not active"));
    }

    auto result = impl_->manager->select_query_result(std::string(sql));
    if (!result.is_ok()) {
        return GameResult<QueryResult>::err(
            GameError(ErrorCode::QueryFailed, result.error().message));
    }

    return GameResult<QueryResult>::ok(convertResult(result.value()));
}

GameResult<uint64_t> Transaction::execute(std::string_view sql) {
    if (!impl_ || !impl_->active) {
        return GameResult<uint64_t>::err(
            GameError(ErrorCode::TransactionFailed, "transaction not active"));
    }

    auto result = impl_->manager->execute_query_result(std::string(sql));
    if (!result.is_ok()) {
        return GameResult<uint64_t>::err(
            GameError(ErrorCode::QueryFailed, result.error().message));
    }

    // execute_query_result returns VoidResult; affected row count not directly
    // available, return 0 as a sentinel.
    return GameResult<uint64_t>::ok(0);
}

bool Transaction::isActive() const noexcept {
    return impl_ && impl_->active;
}

// ---------------------------------------------------------------------------
// GameDatabase::Impl
// ---------------------------------------------------------------------------

struct GameDatabase::Impl {
    DatabaseConfig config;

    std::vector<PooledConnection> pool;
    mutable std::mutex poolMutex;
    std::condition_variable poolCv;
    std::atomic<bool> connected{false};

    // Checkout a connection from the pool (blocking with timeout)
    std::shared_ptr<::database::database_manager> checkout() {
        std::unique_lock lock(poolMutex);
        auto deadline = std::chrono::steady_clock::now() + config.connectionTimeout;

        while (true) {
            for (auto& conn : pool) {
                if (!conn.inUse) {
                    conn.inUse = true;
                    return conn.manager;
                }
            }

            // Try to grow pool if under max
            if (pool.size() < config.maxConnections) {
                auto conn = createConnection();
                if (conn.manager) {
                    conn.inUse = true;
                    auto mgr = conn.manager;
                    pool.push_back(std::move(conn));
                    return mgr;
                }
            }

            // Wait for a connection to be returned
            if (poolCv.wait_until(lock, deadline) == std::cv_status::timeout) {
                return nullptr; // Pool exhausted
            }
        }
    }

    // Return a connection to the pool
    void checkin(::database::database_manager* mgr) {
        std::lock_guard lock(poolMutex);
        for (auto& conn : pool) {
            if (conn.manager.get() == mgr) {
                conn.inUse = false;
                poolCv.notify_one();
                return;
            }
        }
    }

    // Create a new pooled connection
    PooledConnection createConnection() {
        PooledConnection conn;
        conn.context = std::make_shared<::database::database_context>();
        conn.manager = std::make_shared<::database::database_manager>(conn.context);

        if (!conn.manager->set_mode(toKcenon(config.dbType))) {
            conn.manager.reset();
            return conn;
        }

        auto result = conn.manager->connect_result(config.connectionString);
        if (!result.is_ok()) {
            conn.manager.reset();
            return conn;
        }

        conn.inUse = false;
        return conn;
    }

    std::size_t countActive() const {
        std::lock_guard lock(poolMutex);
        return static_cast<std::size_t>(std::count_if(
            pool.begin(), pool.end(),
            [](const PooledConnection& c) { return c.inUse; }));
    }
};

// ---------------------------------------------------------------------------
// GameDatabase construction / destruction / move
// ---------------------------------------------------------------------------

GameDatabase::GameDatabase()
    : impl_(std::make_unique<Impl>()) {}

GameDatabase::~GameDatabase() {
    if (impl_) {
        disconnect();
    }
}

GameDatabase::GameDatabase(GameDatabase&&) noexcept = default;

GameDatabase& GameDatabase::operator=(GameDatabase&& other) noexcept {
    if (this != &other) {
        if (impl_) {
            disconnect();
        }
        impl_ = std::move(other.impl_);
    }
    return *this;
}

// ---------------------------------------------------------------------------
// connect()
// ---------------------------------------------------------------------------

GameResult<void> GameDatabase::connect(const DatabaseConfig& config) {
    if (impl_->connected.load()) {
        return GameResult<void>::err(
            GameError(ErrorCode::AlreadyExists, "already connected"));
    }

    impl_->config = config;

    // Create minimum number of connections
    for (uint32_t i = 0; i < config.minConnections; ++i) {
        auto conn = impl_->createConnection();
        if (!conn.manager) {
            // Disconnect any connections we already made
            disconnect();
            return GameResult<void>::err(
                GameError(ErrorCode::DatabaseError,
                          "failed to create connection " +
                              std::to_string(i + 1) + "/" +
                              std::to_string(config.minConnections)));
        }
        impl_->pool.push_back(std::move(conn));
    }

    impl_->connected.store(true);
    return GameResult<void>::ok();
}

// ---------------------------------------------------------------------------
// disconnect()
// ---------------------------------------------------------------------------

void GameDatabase::disconnect() {
    impl_->connected.store(false);

    std::lock_guard lock(impl_->poolMutex);
    for (auto& conn : impl_->pool) {
        if (conn.manager) {
            (void)conn.manager->disconnect_result();
        }
    }
    impl_->pool.clear();
}

// ---------------------------------------------------------------------------
// isConnected()
// ---------------------------------------------------------------------------

bool GameDatabase::isConnected() const noexcept {
    return impl_->connected.load();
}

// ---------------------------------------------------------------------------
// query()
// ---------------------------------------------------------------------------

GameResult<QueryResult> GameDatabase::query(std::string_view sql) {
    if (!impl_->connected.load()) {
        return GameResult<QueryResult>::err(
            GameError(ErrorCode::NotConnected, "not connected to database"));
    }

    auto mgr = impl_->checkout();
    if (!mgr) {
        return GameResult<QueryResult>::err(
            GameError(ErrorCode::ConnectionPoolExhausted,
                      "no available connections in pool"));
    }

    auto result = mgr->select_query_result(std::string(sql));
    impl_->checkin(mgr.get());

    if (!result.is_ok()) {
        return GameResult<QueryResult>::err(
            GameError(ErrorCode::QueryFailed, result.error().message));
    }

    return GameResult<QueryResult>::ok(convertResult(result.value()));
}

// ---------------------------------------------------------------------------
// execute()
// ---------------------------------------------------------------------------

GameResult<uint64_t> GameDatabase::execute(std::string_view sql) {
    if (!impl_->connected.load()) {
        return GameResult<uint64_t>::err(
            GameError(ErrorCode::NotConnected, "not connected to database"));
    }

    auto mgr = impl_->checkout();
    if (!mgr) {
        return GameResult<uint64_t>::err(
            GameError(ErrorCode::ConnectionPoolExhausted,
                      "no available connections in pool"));
    }

    auto result = mgr->execute_query_result(std::string(sql));
    impl_->checkin(mgr.get());

    if (!result.is_ok()) {
        return GameResult<uint64_t>::err(
            GameError(ErrorCode::QueryFailed, result.error().message));
    }

    return GameResult<uint64_t>::ok(0);
}

// ---------------------------------------------------------------------------
// queryAsync()
// ---------------------------------------------------------------------------

std::future<GameResult<QueryResult>> GameDatabase::queryAsync(
    std::string_view sql) {
    auto sqlStr = std::string(sql);
    return std::async(std::launch::async,
        [this, sqlStr = std::move(sqlStr)]() -> GameResult<QueryResult> {
            return query(sqlStr);
        });
}

// ---------------------------------------------------------------------------
// prepare()
// ---------------------------------------------------------------------------

GameResult<PreparedStatement> GameDatabase::prepare(std::string_view sql) {
    if (!impl_->connected.load()) {
        return GameResult<PreparedStatement>::err(
            GameError(ErrorCode::NotConnected, "not connected to database"));
    }

    return GameResult<PreparedStatement>::ok(
        PreparedStatement(std::string(sql)));
}

// ---------------------------------------------------------------------------
// execute(PreparedStatement)
// ---------------------------------------------------------------------------

GameResult<QueryResult> GameDatabase::execute(const PreparedStatement& stmt) {
    auto resolved = stmt.resolve();
    return query(resolved);
}

// ---------------------------------------------------------------------------
// beginTransaction()
// ---------------------------------------------------------------------------

GameResult<Transaction> GameDatabase::beginTransaction() {
    if (!impl_->connected.load()) {
        return GameResult<Transaction>::err(
            GameError(ErrorCode::NotConnected, "not connected to database"));
    }

    auto mgr = impl_->checkout();
    if (!mgr) {
        return GameResult<Transaction>::err(
            GameError(ErrorCode::ConnectionPoolExhausted,
                      "no available connections in pool"));
    }

    auto result = mgr->begin_transaction();
    if (!result.is_ok()) {
        impl_->checkin(mgr.get());
        return GameResult<Transaction>::err(
            GameError(ErrorCode::TransactionFailed,
                      "failed to begin transaction: " + result.error().message));
    }

    auto txnImpl = std::make_unique<Transaction::Impl>();
    txnImpl->manager = mgr;
    txnImpl->active = true;
    txnImpl->ownsConnection = true;
    // Capture raw Impl pointer for the return-to-pool callback
    auto* implPtr = impl_.get();
    txnImpl->returnConnection = [implPtr](::database::database_manager* m) {
        implPtr->checkin(m);
    };

    return GameResult<Transaction>::ok(
        Transaction(std::move(txnImpl)));
}

// ---------------------------------------------------------------------------
// Pool information
// ---------------------------------------------------------------------------

std::size_t GameDatabase::activeConnections() const noexcept {
    return impl_->countActive();
}

std::size_t GameDatabase::poolSize() const noexcept {
    std::lock_guard lock(impl_->poolMutex);
    return impl_->pool.size();
}

} // namespace cgs::foundation
