#pragma once

/// @file gateway_session_manager.hpp
/// @brief Authenticated client session lifecycle management for the Gateway.
///
/// Tracks connected clients through their lifecycle:
/// Unauthenticated → Authenticated → (optionally Migrating) → Disconnecting.
///
/// @see SRS-SVC-002.1
/// @see SRS-SVC-002.4

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "cgs/foundation/game_result.hpp"
#include "cgs/foundation/types.hpp"
#include "cgs/service/gateway_types.hpp"

namespace cgs::service {

/// Thread-safe manager for gateway client sessions.
///
/// Provides creation, authentication, migration, and removal of client
/// sessions. Used by GatewayServer to track all connected clients.
///
/// Example:
/// @code
///   GatewaySessionManager mgr(10000);
///   auto sid = SessionId(42);
///   mgr.createSession(sid, "192.168.1.1");
///   mgr.authenticateSession(sid, claims, userId);
///   auto session = mgr.getSession(sid);
/// @endcode
class GatewaySessionManager {
public:
    /// Construct with maximum session capacity.
    explicit GatewaySessionManager(uint32_t maxSessions);

    /// Create a new unauthenticated session.
    /// Returns false if the max capacity is reached.
    [[nodiscard]] bool createSession(cgs::foundation::SessionId sessionId,
                                     std::string remoteAddress);

    /// Promote a session to authenticated state.
    [[nodiscard]] bool authenticateSession(cgs::foundation::SessionId sessionId,
                                           TokenClaims claims,
                                           uint64_t userId);

    /// Transition a session to migrating state.
    [[nodiscard]] bool beginMigration(cgs::foundation::SessionId sessionId,
                                      std::string targetService);

    /// Complete migration: update the session's current service.
    [[nodiscard]] bool completeMigration(cgs::foundation::SessionId sessionId);

    /// Remove a session (disconnect).
    void removeSession(cgs::foundation::SessionId sessionId);

    /// Get a snapshot of a session. Returns nullopt if not found.
    [[nodiscard]] std::optional<ClientSession> getSession(
        cgs::foundation::SessionId sessionId) const;

    /// Update the last activity timestamp for a session.
    void touchSession(cgs::foundation::SessionId sessionId);

    /// Update the current service for a session.
    [[nodiscard]] bool setCurrentService(cgs::foundation::SessionId sessionId,
                                         std::string service);

    /// Get sessions in a specific state.
    [[nodiscard]] std::vector<ClientSession> getSessionsByState(
        ClientState state) const;

    /// Get session count.
    [[nodiscard]] std::size_t sessionCount() const;

    /// Get session count by state.
    [[nodiscard]] std::size_t sessionCount(ClientState state) const;

    /// Find sessions that have been idle longer than the given duration.
    [[nodiscard]] std::vector<cgs::foundation::SessionId> findIdleSessions(
        std::chrono::seconds idleTimeout) const;

    /// Find unauthenticated sessions older than the given duration.
    [[nodiscard]] std::vector<cgs::foundation::SessionId> findExpiredAuthSessions(
        std::chrono::seconds authTimeout) const;

private:
    uint32_t maxSessions_;
    mutable std::mutex mutex_;
    std::unordered_map<cgs::foundation::SessionId, ClientSession> sessions_;
};

} // namespace cgs::service
