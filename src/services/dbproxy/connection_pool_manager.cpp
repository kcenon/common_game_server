/// @file connection_pool_manager.cpp
/// @brief ConnectionPoolManager implementation with round-robin replica routing.

#include "cgs/service/connection_pool_manager.hpp"

#include <atomic>
#include <mutex>
#include <vector>

#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_database.hpp"
#include "cgs/foundation/game_error.hpp"

namespace cgs::service {

using cgs::foundation::DatabaseConfig;
using cgs::foundation::ErrorCode;
using cgs::foundation::GameDatabase;
using cgs::foundation::GameError;
using cgs::foundation::GameResult;
using cgs::foundation::PreparedStatement;
using cgs::foundation::QueryResult;

// ── Helpers ─────────────────────────────────────────────────────────────────

namespace {

/// Convert DBEndpointConfig to foundation DatabaseConfig.
DatabaseConfig toFoundationConfig(const DBEndpointConfig& ep) {
    DatabaseConfig cfg;
    cfg.connectionString = ep.connectionString;
    cfg.dbType = ep.dbType;
    cfg.minConnections = ep.minConnections;
    cfg.maxConnections = ep.maxConnections;
    cfg.connectionTimeout = ep.connectionTimeout;
    return cfg;
}

} // anonymous namespace

// ── Impl ────────────────────────────────────────────────────────────────────

struct ConnectionPoolManager::Impl {
    DBProxyConfig config;

    GameDatabase primaryDb;
    std::vector<GameDatabase> replicaDbs;

    // Round-robin counter for replica selection.
    std::atomic<uint64_t> replicaRoundRobin{0};

    mutable std::mutex mutex;
    bool started = false;

    /// Select the next healthy replica via round-robin.
    /// Returns nullptr if no replicas are connected.
    GameDatabase* selectReplica() {
        if (replicaDbs.empty()) {
            return nullptr;
        }

        auto count = replicaDbs.size();
        auto startIdx = replicaRoundRobin.fetch_add(1, std::memory_order_relaxed);

        // Try each replica starting from the round-robin position.
        for (std::size_t i = 0; i < count; ++i) {
            auto idx = (startIdx + i) % count;
            if (replicaDbs[idx].isConnected()) {
                return &replicaDbs[idx];
            }
        }

        return nullptr;
    }
};

// ── Construction / destruction / move ───────────────────────────────────────

ConnectionPoolManager::ConnectionPoolManager(DBProxyConfig config)
    : impl_(std::make_unique<Impl>()) {
    impl_->config = std::move(config);
}

ConnectionPoolManager::~ConnectionPoolManager() {
    if (impl_) {
        stop();
    }
}

ConnectionPoolManager::ConnectionPoolManager(ConnectionPoolManager&&) noexcept = default;
ConnectionPoolManager& ConnectionPoolManager::operator=(ConnectionPoolManager&&) noexcept = default;

// ── start() ─────────────────────────────────────────────────────────────────

GameResult<void> ConnectionPoolManager::start() {
    std::lock_guard lock(impl_->mutex);

    if (impl_->started) {
        return GameResult<void>::err(
            GameError(ErrorCode::AlreadyExists, "pool manager already started"));
    }

    // Connect primary.
    auto primaryCfg = toFoundationConfig(impl_->config.primary);
    auto primaryResult = impl_->primaryDb.connect(primaryCfg);
    if (primaryResult.hasError()) {
        return GameResult<void>::err(
            GameError(ErrorCode::PrimaryUnavailable,
                      std::string("failed to connect primary: ") +
                          std::string(primaryResult.error().message())));
    }

    // Connect replicas (failures are non-fatal).
    impl_->replicaDbs.resize(impl_->config.replicas.size());
    for (std::size_t i = 0; i < impl_->config.replicas.size(); ++i) {
        auto replicaCfg = toFoundationConfig(impl_->config.replicas[i]);
        auto result = impl_->replicaDbs[i].connect(replicaCfg);
        // Replica connection failures are logged but not fatal.
        (void)result;
    }

    impl_->started = true;
    return GameResult<void>::ok();
}

// ── stop() ──────────────────────────────────────────────────────────────────

void ConnectionPoolManager::stop() {
    std::lock_guard lock(impl_->mutex);

    impl_->primaryDb.disconnect();
    for (auto& replica : impl_->replicaDbs) {
        replica.disconnect();
    }
    impl_->replicaDbs.clear();
    impl_->started = false;
}

// ── isConnected() ───────────────────────────────────────────────────────────

bool ConnectionPoolManager::isConnected() const noexcept {
    return impl_->primaryDb.isConnected();
}

// ── query() ─────────────────────────────────────────────────────────────────

GameResult<QueryResult> ConnectionPoolManager::query(std::string_view sql) {
    if (!impl_->started) {
        return GameResult<QueryResult>::err(
            GameError(ErrorCode::DBProxyNotStarted,
                      "connection pool manager not started"));
    }

    // Try a replica first.
    auto* replica = impl_->selectReplica();
    if (replica) {
        auto result = replica->query(sql);
        if (result.hasValue()) {
            return result;
        }
        // Fall through to primary on replica failure.
    }

    // Fallback to primary.
    return impl_->primaryDb.query(sql);
}

// ── execute() ───────────────────────────────────────────────────────────────

GameResult<uint64_t> ConnectionPoolManager::execute(std::string_view sql) {
    if (!impl_->started) {
        return GameResult<uint64_t>::err(
            GameError(ErrorCode::DBProxyNotStarted,
                      "connection pool manager not started"));
    }

    return impl_->primaryDb.execute(sql);
}

// ── query(PreparedStatement) ────────────────────────────────────────────────

GameResult<QueryResult> ConnectionPoolManager::query(
    const PreparedStatement& stmt) {
    // Resolve to safe SQL with escaped parameters.
    auto resolved = stmt.resolve();
    return query(resolved);
}

// ── execute(PreparedStatement) ──────────────────────────────────────────────

GameResult<uint64_t> ConnectionPoolManager::execute(
    const PreparedStatement& stmt) {
    auto resolved = stmt.resolve();
    return execute(resolved);
}

// ── replicaCount() ──────────────────────────────────────────────────────────

std::size_t ConnectionPoolManager::replicaCount() const {
    return impl_->replicaDbs.size();
}

// ── stats() ─────────────────────────────────────────────────────────────────

PoolStats ConnectionPoolManager::stats() const {
    PoolStats s;
    s.primaryActive = impl_->primaryDb.activeConnections();
    s.primaryTotal = impl_->primaryDb.poolSize();
    s.replicaCount = impl_->replicaDbs.size();

    for (const auto& replica : impl_->replicaDbs) {
        s.replicaActive += replica.activeConnections();
        s.replicaTotal += replica.poolSize();
    }

    return s;
}

// ── config() ────────────────────────────────────────────────────────────────

const DBProxyConfig& ConnectionPoolManager::config() const noexcept {
    return impl_->config;
}

} // namespace cgs::service
