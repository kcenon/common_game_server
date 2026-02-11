#pragma once

/// @file gateway_types.hpp
/// @brief Core type definitions for the Gateway Server.
///
/// Defines configuration, client session state, route entries, and
/// gateway-specific opcode constants.
///
/// @see SRS-SVC-002
/// @see SDS-MOD-041

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "cgs/foundation/types.hpp"
#include "cgs/service/auth_types.hpp"

namespace cgs::service {

// -- Client state -------------------------------------------------------------

/// Connection lifecycle state for a gateway client.
enum class ClientState : uint8_t {
    /// Connected but not yet authenticated.
    Unauthenticated,
    /// Token validated, ready for message routing.
    Authenticated,
    /// Being transferred to another game server.
    Migrating,
    /// Disconnecting.
    Disconnecting
};

/// Return the string name for a client state.
constexpr std::string_view clientStateName(ClientState state) {
    switch (state) {
        case ClientState::Unauthenticated: return "Unauthenticated";
        case ClientState::Authenticated:   return "Authenticated";
        case ClientState::Migrating:       return "Migrating";
        case ClientState::Disconnecting:   return "Disconnecting";
    }
    return "Unknown";
}

// -- Client session -----------------------------------------------------------

/// Authenticated client session tracked by the gateway.
struct ClientSession {
    /// Network-level session identifier.
    cgs::foundation::SessionId sessionId;

    /// Current connection state.
    ClientState state = ClientState::Unauthenticated;

    /// Decoded JWT claims (populated after authentication).
    TokenClaims claims;

    /// User ID extracted from claims.subject (0 if unauthenticated).
    uint64_t userId = 0;

    /// Client IP address for rate limiting and logging.
    std::string remoteAddress;

    /// Identifier of the downstream service currently handling this client.
    std::string currentService;

    /// When the connection was established.
    std::chrono::steady_clock::time_point connectedAt{};

    /// When the last message was received.
    std::chrono::steady_clock::time_point lastActivity{};
};

// -- Route table types --------------------------------------------------------

/// Mapping from an opcode range to a downstream service.
struct RouteEntry {
    /// First opcode in the range (inclusive).
    uint16_t opcodeMin = 0;

    /// Last opcode in the range (inclusive).
    uint16_t opcodeMax = 0;

    /// Downstream service identifier (e.g., "game", "lobby", "chat").
    std::string service;

    /// Whether this route requires authentication.
    bool requiresAuth = true;
};

// -- Gateway opcodes ----------------------------------------------------------

/// Reserved gateway-level opcodes (0x0000-0x00FF).
namespace GatewayOpcode {
    /// Client → Gateway: authenticate with JWT access token.
    constexpr uint16_t Authenticate = 0x0001;

    /// Gateway → Client: authentication result.
    constexpr uint16_t AuthResult = 0x0002;

    /// Gateway → Client: server transfer notification.
    constexpr uint16_t ServerTransfer = 0x0010;

    /// Client → Gateway: migration acknowledgement.
    constexpr uint16_t MigrationAck = 0x0011;

    /// Gateway → Client: heartbeat ping.
    constexpr uint16_t Ping = 0x00FE;

    /// Client → Gateway: heartbeat pong.
    constexpr uint16_t Pong = 0x00FF;
} // namespace GatewayOpcode

// -- Configuration ------------------------------------------------------------

/// Configuration for the Gateway Server.
struct GatewayConfig {
    /// TCP listener port.
    uint16_t tcpPort = 8080;

    /// WebSocket listener port.
    uint16_t webSocketPort = 8081;

    /// Maximum time allowed for authentication after connecting.
    std::chrono::seconds authTimeout{10};

    /// Token bucket: maximum burst capacity per client.
    uint32_t rateLimitCapacity = 100;

    /// Token bucket: tokens refilled per second per client.
    uint32_t rateLimitRefillRate = 50;

    /// Maximum concurrent connections.
    uint32_t maxConnections = 10000;

    /// Idle timeout before disconnecting an authenticated client.
    std::chrono::seconds idleTimeout{300};
};

} // namespace cgs::service
