/// @file dbproxy_server.cpp
/// @brief DBProxyServer implementation orchestrating cache, pool, and routing.

#include "cgs/service/dbproxy_server.hpp"

#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_error.hpp"
#include "cgs/service/connection_pool_manager.hpp"
#include "cgs/service/query_cache.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <future>
#include <mutex>
#include <string>

namespace cgs::service {

using cgs::foundation::ErrorCode;
using cgs::foundation::GameError;
using cgs::foundation::GameResult;
using cgs::foundation::PreparedStatement;
using cgs::foundation::QueryResult;

// ── Helpers ─────────────────────────────────────────────────────────────────

namespace {

/// Check if a SQL statement is a read-only query (SELECT).
bool isReadQuery(std::string_view sql) {
    // Skip leading whitespace.
    auto it = sql.begin();
    while (it != sql.end() && std::isspace(static_cast<unsigned char>(*it))) {
        ++it;
    }

    // Check for SELECT (case-insensitive).
    constexpr std::string_view kSelect = "SELECT";
    auto remaining = std::string_view(it, sql.end());

    if (remaining.size() < kSelect.size()) {
        return false;
    }

    for (std::size_t i = 0; i < kSelect.size(); ++i) {
        if (std::toupper(static_cast<unsigned char>(remaining[i])) != kSelect[i]) {
            return false;
        }
    }

    return true;
}

/// Extract table names from a write SQL statement for cache invalidation.
/// Recognizes INSERT INTO <table>, UPDATE <table>, DELETE FROM <table>,
/// and ALTER/DROP/TRUNCATE TABLE <table>.
///
/// Returns the first table name found (sufficient for invalidation).
std::string extractTableName(std::string_view sql) {
    // Normalize to uppercase for keyword matching.
    auto upper = std::string(sql);
    std::transform(
        upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return std::toupper(c); });

    // Patterns to search for.
    struct Pattern {
        std::string_view keyword;
        std::size_t skipWords;  // Words to skip after keyword.
    };

    static const Pattern patterns[] = {
        {"INSERT INTO ", 0},
        {"UPDATE ", 0},
        {"DELETE FROM ", 0},
        {"ALTER TABLE ", 0},
        {"DROP TABLE ", 0},
        {"TRUNCATE TABLE ", 0},
        {"TRUNCATE ", 0},
    };

    for (const auto& pattern : patterns) {
        auto pos = upper.find(pattern.keyword);
        if (pos == std::string::npos) {
            continue;
        }

        // Move past the keyword.
        auto nameStart = pos + pattern.keyword.size();

        // Skip optional whitespace.
        while (nameStart < sql.size() && std::isspace(static_cast<unsigned char>(sql[nameStart]))) {
            ++nameStart;
        }

        // Extract the table name (until whitespace, parenthesis, or semicolon).
        auto nameEnd = nameStart;
        while (nameEnd < sql.size()) {
            char c = sql[nameEnd];
            if (std::isspace(static_cast<unsigned char>(c)) || c == '(' || c == ';' || c == ',') {
                break;
            }
            ++nameEnd;
        }

        if (nameEnd > nameStart) {
            return std::string(sql.substr(nameStart, nameEnd - nameStart));
        }
    }

    return {};
}

}  // anonymous namespace

// ── Impl ────────────────────────────────────────────────────────────────────

struct DBProxyServer::Impl {
    DBProxyConfig config;

    ConnectionPoolManager poolManager;
    QueryCache cache;

    std::atomic<uint64_t> totalQueryCount{0};
    std::atomic<bool> running{false};

    explicit Impl(DBProxyConfig cfg)
        : config(std::move(cfg)), poolManager(config), cache(config.cache) {}
};

// ── Construction / destruction / move ───────────────────────────────────────

DBProxyServer::DBProxyServer(DBProxyConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

DBProxyServer::~DBProxyServer() {
    if (impl_ && impl_->running.load()) {
        stop();
    }
}

DBProxyServer::DBProxyServer(DBProxyServer&&) noexcept = default;
DBProxyServer& DBProxyServer::operator=(DBProxyServer&&) noexcept = default;

// ── start() ─────────────────────────────────────────────────────────────────

GameResult<void> DBProxyServer::start() {
    if (impl_->running.load()) {
        return GameResult<void>::err(
            GameError(ErrorCode::AlreadyExists, "DBProxy already running"));
    }

    auto result = impl_->poolManager.start();
    if (result.hasError()) {
        return result;
    }

    impl_->running.store(true);
    return GameResult<void>::ok();
}

// ── stop() ──────────────────────────────────────────────────────────────────

void DBProxyServer::stop() {
    impl_->running.store(false);
    impl_->poolManager.stop();
    impl_->cache.clear();
}

// ── isRunning() ─────────────────────────────────────────────────────────────

bool DBProxyServer::isRunning() const noexcept {
    return impl_->running.load();
}

// ── query() ─────────────────────────────────────────────────────────────────

GameResult<QueryResult> DBProxyServer::query(std::string_view sql) {
    if (!impl_->running.load()) {
        return GameResult<QueryResult>::err(
            GameError(ErrorCode::DBProxyNotStarted, "DBProxy not started"));
    }

    impl_->totalQueryCount.fetch_add(1, std::memory_order_relaxed);

    // Check cache first (only for read queries).
    if (impl_->config.cache.enabled && isReadQuery(sql)) {
        auto cached = impl_->cache.get(sql);
        if (cached) {
            return GameResult<QueryResult>::ok(std::move(*cached));
        }
    }

    // Execute via pool manager (handles replica routing for SELECTs).
    auto result = impl_->poolManager.query(sql);
    if (result.hasError()) {
        return result;
    }

    // Cache the result.
    if (impl_->config.cache.enabled && isReadQuery(sql)) {
        impl_->cache.put(sql, result.value());
    }

    return result;
}

// ── execute() ───────────────────────────────────────────────────────────────

GameResult<uint64_t> DBProxyServer::execute(std::string_view sql) {
    if (!impl_->running.load()) {
        return GameResult<uint64_t>::err(
            GameError(ErrorCode::DBProxyNotStarted, "DBProxy not started"));
    }

    impl_->totalQueryCount.fetch_add(1, std::memory_order_relaxed);

    // Execute on primary.
    auto result = impl_->poolManager.execute(sql);
    if (result.hasError()) {
        return result;
    }

    // Invalidate cache entries for the affected table.
    if (impl_->config.cache.enabled) {
        auto tableName = extractTableName(sql);
        if (!tableName.empty()) {
            impl_->cache.invalidateByTable(tableName);
        }
    }

    return result;
}

// ── queryAsync() ────────────────────────────────────────────────────────────

std::future<GameResult<QueryResult>> DBProxyServer::queryAsync(std::string_view sql) {
    auto sqlStr = std::string(sql);
    return std::async(
        std::launch::async,
        [this, sqlStr = std::move(sqlStr)]() -> GameResult<QueryResult> { return query(sqlStr); });
}

// ── query(PreparedStatement) ────────────────────────────────────────────────

GameResult<QueryResult> DBProxyServer::query(const PreparedStatement& stmt) {
    if (!impl_->running.load()) {
        return GameResult<QueryResult>::err(
            GameError(ErrorCode::DBProxyNotStarted, "DBProxy not started"));
    }

    impl_->totalQueryCount.fetch_add(1, std::memory_order_relaxed);

    // Resolve to safe SQL with escaped parameters.
    auto resolved = stmt.resolve();

    // Check cache using the resolved SQL.
    if (impl_->config.cache.enabled && isReadQuery(resolved)) {
        auto cached = impl_->cache.get(resolved);
        if (cached) {
            return GameResult<QueryResult>::ok(std::move(*cached));
        }
    }

    // Execute via pool manager.
    auto result = impl_->poolManager.query(stmt);
    if (result.hasError()) {
        return result;
    }

    // Cache the result using the resolved SQL as key.
    if (impl_->config.cache.enabled && isReadQuery(resolved)) {
        impl_->cache.put(resolved, result.value());
    }

    return result;
}

// ── execute(PreparedStatement) ──────────────────────────────────────────────

GameResult<uint64_t> DBProxyServer::execute(const PreparedStatement& stmt) {
    if (!impl_->running.load()) {
        return GameResult<uint64_t>::err(
            GameError(ErrorCode::DBProxyNotStarted, "DBProxy not started"));
    }

    impl_->totalQueryCount.fetch_add(1, std::memory_order_relaxed);

    // Execute on primary via pool manager.
    auto result = impl_->poolManager.execute(stmt);
    if (result.hasError()) {
        return result;
    }

    // Invalidate cache entries for the affected table.
    if (impl_->config.cache.enabled) {
        auto resolved = stmt.resolve();
        auto tableName = extractTableName(resolved);
        if (!tableName.empty()) {
            impl_->cache.invalidateByTable(tableName);
        }
    }

    return result;
}

// ── queryAsync(PreparedStatement) ───────────────────────────────────────────

std::future<GameResult<QueryResult>> DBProxyServer::queryAsync(const PreparedStatement& stmt) {
    // Copy the statement for the async lambda.
    auto stmtCopy = stmt;
    return std::async(std::launch::async,
                      [this, stmtCopy = std::move(stmtCopy)]() -> GameResult<QueryResult> {
                          return query(stmtCopy);
                      });
}

// ── Cache management ────────────────────────────────────────────────────────

std::size_t DBProxyServer::invalidateCache(std::string_view tableName) {
    return impl_->cache.invalidateByTable(tableName);
}

void DBProxyServer::clearCache() {
    impl_->cache.clear();
}

std::size_t DBProxyServer::cacheSize() const {
    return impl_->cache.size();
}

double DBProxyServer::cacheHitRate() const {
    return impl_->cache.hitRate();
}

// ── Statistics ──────────────────────────────────────────────────────────────

PoolStats DBProxyServer::poolStats() const {
    auto stats = impl_->poolManager.stats();
    stats.cacheEntries = impl_->cache.size();
    stats.cacheHitRate = impl_->cache.hitRate();
    return stats;
}

uint64_t DBProxyServer::totalQueries() const {
    return impl_->totalQueryCount.load(std::memory_order_relaxed);
}

const DBProxyConfig& DBProxyServer::config() const noexcept {
    return impl_->config;
}

}  // namespace cgs::service
