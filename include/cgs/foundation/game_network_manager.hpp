#pragma once

/// @file game_network_manager.hpp
/// @brief GameNetworkManager wrapping kcenon network_system for game-specific
///        TCP/UDP/WebSocket communication.
///
/// Provides protocol-agnostic server management with message framing, session
/// tracking, and opcode-based dispatch. Part of the Network System Adapter
/// (SDS-MOD-004).

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "cgs/foundation/game_result.hpp"
#include "cgs/foundation/signal.hpp"
#include "cgs/foundation/types.hpp"

namespace cgs::foundation {

/// Supported network transport protocols.
enum class Protocol : uint8_t {
    TCP,
    UDP,
    WebSocket
};

/// Return the string name for a protocol.
constexpr std::string_view protocolName(Protocol proto) {
    switch (proto) {
        case Protocol::TCP:       return "TCP";
        case Protocol::UDP:       return "UDP";
        case Protocol::WebSocket: return "WebSocket";
    }
    return "Unknown";
}

/// Application-level message with opcode and binary payload.
///
/// Wire format (serialize/deserialize):
///   [4 bytes: total length (network order)]
///   [2 bytes: opcode (network order)]
///   [N bytes: payload]
struct NetworkMessage {
    uint16_t opcode = 0;
    std::vector<uint8_t> payload;

    /// Serialize to wire format: [4-byte length][2-byte opcode][payload].
    [[nodiscard]] std::vector<uint8_t> serialize() const;

    /// Deserialize from wire bytes. Returns nullopt on malformed input.
    [[nodiscard]] static std::optional<NetworkMessage> deserialize(
        const uint8_t* data, std::size_t size);

    /// Convenience overload.
    [[nodiscard]] static std::optional<NetworkMessage> deserialize(
        const std::vector<uint8_t>& data);
};

/// Callback invoked when a message with a specific opcode arrives.
using MessageHandler = std::function<void(SessionId, const NetworkMessage&)>;

/// Snapshot of a connected session's state.
struct SessionInfo {
    SessionId id;
    Protocol protocol;
    std::string remoteAddress;
    std::chrono::steady_clock::time_point connectedAt;
    std::chrono::steady_clock::time_point lastActivity;
};

/// Protocol-agnostic network manager wrapping kcenon's network_system.
///
/// Manages TCP/UDP/WebSocket servers, tracks sessions, and dispatches incoming
/// messages to registered opcode handlers. Uses PIMPL to hide all kcenon
/// implementation details from the public API.
///
/// Signals are provided for connection lifecycle events:
/// - onConnected:    SessionId of the newly connected client
/// - onDisconnected: SessionId of the disconnected client
/// - onError:        (SessionId, ErrorCode) when an error occurs on a session
///
/// Example:
/// @code
///   GameNetworkManager net;
///   net.onConnected.connect([](SessionId sid) {
///       std::cout << "session " << sid.value() << " connected\n";
///   });
///   net.registerHandler(0x01, [](SessionId sid, const NetworkMessage& msg) {
///       // handle login message
///   });
///   auto result = net.listen(8080, Protocol::TCP);
///   if (!result) { /* handle error */ }
/// @endcode
class GameNetworkManager {
public:
    GameNetworkManager();
    ~GameNetworkManager();

    // Non-copyable, movable.
    GameNetworkManager(const GameNetworkManager&) = delete;
    GameNetworkManager& operator=(const GameNetworkManager&) = delete;
    GameNetworkManager(GameNetworkManager&&) noexcept;
    GameNetworkManager& operator=(GameNetworkManager&&) noexcept;

    // ── Server lifecycle ────────────────────────────────────────────────────

    /// Start listening on the given port for the specified protocol.
    [[nodiscard]] GameResult<void> listen(uint16_t port, Protocol protocol);

    /// Stop the server for a specific protocol.
    [[nodiscard]] GameResult<void> stop(Protocol protocol);

    /// Stop all protocol servers.
    void stopAll();

    // ── Session I/O ─────────────────────────────────────────────────────────

    /// Send a framed NetworkMessage to a specific session.
    [[nodiscard]] GameResult<void> send(SessionId session, const NetworkMessage& msg);

    /// Broadcast a framed NetworkMessage to all connected sessions.
    [[nodiscard]] GameResult<void> broadcast(const NetworkMessage& msg);

    /// Close a specific session.
    void close(SessionId session);

    // ── Message handlers ────────────────────────────────────────────────────

    /// Register a handler for a specific opcode.
    void registerHandler(uint16_t opcode, MessageHandler handler);

    /// Remove the handler for a specific opcode.
    void unregisterHandler(uint16_t opcode);

    // ── Queries ─────────────────────────────────────────────────────────────

    /// Get information about a specific session.
    [[nodiscard]] std::optional<SessionInfo> sessionInfo(SessionId id) const;

    /// Get the total number of connected sessions across all protocols.
    [[nodiscard]] std::size_t sessionCount() const;

    // ── Signals ─────────────────────────────────────────────────────────────

    Signal<SessionId> onConnected;
    Signal<SessionId> onDisconnected;
    Signal<SessionId, ErrorCode> onError;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cgs::foundation
