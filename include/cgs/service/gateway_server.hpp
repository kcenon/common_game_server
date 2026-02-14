#pragma once

/// @file gateway_server.hpp
/// @brief Gateway Server for client connection management, authentication
///        delegation, opcode-based message routing, and connection migration.
///
/// GatewayServer is the entry point for all client traffic. It validates
/// authentication tokens via AuthServer, routes messages to downstream
/// services based on opcode ranges, and handles server-transfer migrations.
///
/// @see SRS-SVC-002
/// @see SDS-MOD-041

#include "cgs/foundation/game_result.hpp"
#include "cgs/foundation/types.hpp"
#include "cgs/service/gateway_types.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace cgs::service {

class AuthServer;

// -- Action types returned by message handling --------------------------------

/// The kind of action the gateway decides after processing a message.
enum class GatewayActionType : uint8_t {
    /// Forward the message payload to a downstream service.
    Forward,
    /// Reply directly to the client (e.g., auth result, pong).
    Reply,
    /// Drop the message silently.
    Drop
};

/// Describes what should happen after the gateway processes a client message.
struct GatewayAction {
    GatewayActionType type = GatewayActionType::Drop;

    /// Target downstream service (for Forward).
    std::string targetService;

    /// Reply opcode (for Reply).
    uint16_t replyOpcode = 0;

    /// Reply payload bytes (for Reply).
    std::vector<uint8_t> replyPayload;

    /// Human-readable reason (for Drop).
    std::string reason;
};

// -- Statistics ---------------------------------------------------------------

/// Runtime statistics snapshot for the gateway.
struct GatewayStats {
    std::size_t totalConnections = 0;
    std::size_t authenticatedConnections = 0;
    std::size_t unauthenticatedConnections = 0;
    std::size_t migratingConnections = 0;
    uint64_t messagesRouted = 0;
    uint64_t messagesDropped = 0;
    uint64_t authSuccessCount = 0;
    uint64_t authFailureCount = 0;
    uint64_t rateLimitHits = 0;
};

// -- Gateway Server -----------------------------------------------------------

/// Gateway Server for client connection management and message routing.
///
/// Usage:
/// @code
///   auto authServer = std::make_shared<AuthServer>(...);
///   GatewayConfig config;
///   GatewayServer gateway(config, authServer);
///   gateway.addRoute(0x0100, 0x01FF, "game");
///   gateway.addRoute(0x0200, 0x02FF, "lobby");
///   auto r = gateway.start();
///
///   // On client connect:
///   gateway.handleConnect(sessionId, "192.168.1.10");
///
///   // On client message:
///   auto action = gateway.handleMessage(sessionId, opcode, payload);
///   // action tells the network layer what to do (forward, reply, drop).
///
///   // On server transfer:
///   gateway.initiateServerTransfer(sessionId, "game-server-2");
/// @endcode
class GatewayServer {
public:
    GatewayServer(GatewayConfig config, std::shared_ptr<AuthServer> authServer);

    ~GatewayServer();

    GatewayServer(const GatewayServer&) = delete;
    GatewayServer& operator=(const GatewayServer&) = delete;
    GatewayServer(GatewayServer&&) noexcept;
    GatewayServer& operator=(GatewayServer&&) noexcept;

    // -- Lifecycle ------------------------------------------------------------

    /// Start the gateway server.
    [[nodiscard]] cgs::foundation::GameResult<void> start();

    /// Stop the gateway server and disconnect all sessions.
    void stop();

    /// Check if the server is running.
    [[nodiscard]] bool isRunning() const noexcept;

    // -- Route configuration --------------------------------------------------

    /// Register an opcode range to route to a downstream service.
    void addRoute(uint16_t opcodeMin,
                  uint16_t opcodeMax,
                  std::string service,
                  bool requiresAuth = true);

    // -- Connection handling --------------------------------------------------

    /// Handle a new client connection.
    [[nodiscard]] cgs::foundation::GameResult<void> handleConnect(
        cgs::foundation::SessionId sessionId, std::string remoteAddress);

    /// Handle a client disconnection.
    void handleDisconnect(cgs::foundation::SessionId sessionId);

    /// Process an incoming message and return the routing decision.
    [[nodiscard]] cgs::foundation::GameResult<GatewayAction> handleMessage(
        cgs::foundation::SessionId sessionId, uint16_t opcode, std::vector<uint8_t> payload);

    // -- Migration ------------------------------------------------------------

    /// Initiate a server transfer for an authenticated client.
    [[nodiscard]] cgs::foundation::GameResult<void> initiateServerTransfer(
        cgs::foundation::SessionId sessionId, std::string targetService);

    // -- Maintenance ----------------------------------------------------------

    /// Disconnect sessions that have been idle longer than the configured
    /// timeout. Returns the list of disconnected session IDs.
    [[nodiscard]] std::vector<cgs::foundation::SessionId> cleanupIdleSessions();

    /// Disconnect unauthenticated sessions that exceeded the auth timeout.
    /// Returns the list of disconnected session IDs.
    [[nodiscard]] std::vector<cgs::foundation::SessionId> cleanupExpiredAuth();

    // -- Statistics -----------------------------------------------------------

    /// Get a snapshot of current gateway statistics.
    [[nodiscard]] GatewayStats stats() const;

    /// Get the configuration.
    [[nodiscard]] const GatewayConfig& config() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace cgs::service
