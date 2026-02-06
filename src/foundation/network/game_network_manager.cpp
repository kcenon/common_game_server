/// @file game_network_manager.cpp
/// @brief GameNetworkManager implementation wrapping kcenon network_system.

#include "cgs/foundation/game_network_manager.hpp"

// kcenon facade headers (hidden behind PIMPL)
#include <kcenon/network/facade/tcp_facade.h>
#include <kcenon/network/facade/udp_facade.h>
#include <kcenon/network/facade/websocket_facade.h>
#include <kcenon/network/interfaces/i_protocol_server.h>
#include <kcenon/network/interfaces/i_session.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <shared_mutex>
#include <unordered_map>

namespace cgs::foundation {

// ---------------------------------------------------------------------------
// NetworkMessage serialization
// ---------------------------------------------------------------------------

std::vector<uint8_t> NetworkMessage::serialize() const {
    // Wire format: [4-byte total length][2-byte opcode][payload]
    const uint32_t totalLen =
        static_cast<uint32_t>(sizeof(uint32_t) + sizeof(uint16_t) + payload.size());

    std::vector<uint8_t> buf;
    buf.resize(totalLen);

    // Network byte order (big-endian)
    buf[0] = static_cast<uint8_t>((totalLen >> 24) & 0xFF);
    buf[1] = static_cast<uint8_t>((totalLen >> 16) & 0xFF);
    buf[2] = static_cast<uint8_t>((totalLen >> 8) & 0xFF);
    buf[3] = static_cast<uint8_t>(totalLen & 0xFF);

    buf[4] = static_cast<uint8_t>((opcode >> 8) & 0xFF);
    buf[5] = static_cast<uint8_t>(opcode & 0xFF);

    if (!payload.empty()) {
        std::memcpy(buf.data() + 6, payload.data(), payload.size());
    }

    return buf;
}

std::optional<NetworkMessage> NetworkMessage::deserialize(
    const uint8_t* data, std::size_t size) {
    // Minimum: 4 (length) + 2 (opcode) = 6 bytes
    constexpr std::size_t kHeaderSize = 6;
    if (size < kHeaderSize) {
        return std::nullopt;
    }

    // Read total length (network order)
    const uint32_t totalLen =
        (static_cast<uint32_t>(data[0]) << 24) |
        (static_cast<uint32_t>(data[1]) << 16) |
        (static_cast<uint32_t>(data[2]) << 8) |
        static_cast<uint32_t>(data[3]);

    if (totalLen < kHeaderSize || totalLen > size) {
        return std::nullopt;
    }

    NetworkMessage msg;
    msg.opcode = static_cast<uint16_t>(
        (static_cast<uint16_t>(data[4]) << 8) | data[5]);

    const auto payloadSize = totalLen - kHeaderSize;
    if (payloadSize > 0) {
        msg.payload.assign(data + kHeaderSize, data + kHeaderSize + payloadSize);
    }

    return msg;
}

std::optional<NetworkMessage> NetworkMessage::deserialize(
    const std::vector<uint8_t>& data) {
    return deserialize(data.data(), data.size());
}

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct GameNetworkManager::Impl {
    // Active protocol servers keyed by protocol
    std::unordered_map<Protocol,
                       std::shared_ptr<kcenon::network::interfaces::i_protocol_server>>
        servers;

    // Internal session data: maps our SessionId → kcenon session + metadata
    struct InternalSession {
        Protocol protocol;
        std::string kcSessionId;   // kcenon's string session ID
        std::shared_ptr<kcenon::network::interfaces::i_session> kcSession;
        SessionInfo info;
    };

    std::unordered_map<SessionId, InternalSession> sessions;
    // Reverse lookup: kcenon string ID → our SessionId
    std::unordered_map<std::string, SessionId> reverseMap;

    // Opcode → handler
    std::unordered_map<uint16_t, MessageHandler> handlers;

    std::atomic<uint64_t> nextSessionId{1};
    mutable std::shared_mutex sessionMutex;
    mutable std::shared_mutex handlerMutex;

    // Back-pointer for signal emission (non-owning, always valid during Impl lifetime)
    GameNetworkManager* owner = nullptr;

    // Allocate a new SessionId
    SessionId allocateSessionId() {
        return SessionId(nextSessionId.fetch_add(1, std::memory_order_relaxed));
    }

    // Create and configure a protocol server
    std::shared_ptr<kcenon::network::interfaces::i_protocol_server>
    createServer(Protocol protocol) {
        using namespace kcenon::network::facade;
        switch (protocol) {
            case Protocol::TCP: {
                tcp_facade facade;
                return facade.create_server({});
            }
            case Protocol::UDP: {
                udp_facade facade;
                return facade.create_server({});
            }
            case Protocol::WebSocket: {
                websocket_facade facade;
                return facade.create_server({});
            }
        }
        return nullptr;
    }

    // Wire up kcenon callbacks to our internal dispatch
    void setupCallbacks(
        std::shared_ptr<kcenon::network::interfaces::i_protocol_server>& server,
        Protocol protocol) {

        server->set_connection_callback(
            [this, protocol](
                std::shared_ptr<kcenon::network::interfaces::i_session> kcSession) {
                auto sid = allocateSessionId();
                auto kcId = std::string(kcSession->id());
                auto now = std::chrono::steady_clock::now();

                InternalSession internal;
                internal.protocol = protocol;
                internal.kcSessionId = kcId;
                internal.kcSession = kcSession;
                internal.info.id = sid;
                internal.info.protocol = protocol;
                internal.info.remoteAddress = kcId;
                internal.info.connectedAt = now;
                internal.info.lastActivity = now;

                {
                    std::unique_lock lock(sessionMutex);
                    sessions.emplace(sid, std::move(internal));
                    reverseMap.emplace(kcId, sid);
                }

                owner->onConnected.emit(sid);
            });

        server->set_receive_callback(
            [this](std::string_view kcId, const std::vector<uint8_t>& data) {
                SessionId sid;
                {
                    std::shared_lock lock(sessionMutex);
                    auto it = reverseMap.find(std::string(kcId));
                    if (it == reverseMap.end()) {
                        return;
                    }
                    sid = it->second;
                    // Update last activity
                    auto sit = sessions.find(sid);
                    if (sit != sessions.end()) {
                        sit->second.info.lastActivity =
                            std::chrono::steady_clock::now();
                    }
                }

                // Deserialize and dispatch to opcode handler
                auto msg = NetworkMessage::deserialize(data);
                if (!msg) {
                    owner->onError.emit(sid, ErrorCode::InvalidMessage);
                    return;
                }

                MessageHandler handler;
                {
                    std::shared_lock lock(handlerMutex);
                    auto it = handlers.find(msg->opcode);
                    if (it != handlers.end()) {
                        handler = it->second;
                    }
                }
                if (handler) {
                    handler(sid, *msg);
                }
            });

        server->set_disconnection_callback(
            [this](std::string_view kcId) {
                SessionId sid;
                {
                    std::unique_lock lock(sessionMutex);
                    auto it = reverseMap.find(std::string(kcId));
                    if (it == reverseMap.end()) {
                        return;
                    }
                    sid = it->second;
                    sessions.erase(sid);
                    reverseMap.erase(it);
                }

                owner->onDisconnected.emit(sid);
            });

        server->set_error_callback(
            [this](std::string_view kcId, std::error_code ec) {
                SessionId sid;
                {
                    std::shared_lock lock(sessionMutex);
                    auto it = reverseMap.find(std::string(kcId));
                    if (it == reverseMap.end()) {
                        return;
                    }
                    sid = it->second;
                }
                // Map system error to our ErrorCode
                auto code = ec ? ErrorCode::NetworkError : ErrorCode::Success;
                owner->onError.emit(sid, code);
            });
    }
};

// ---------------------------------------------------------------------------
// Construction / Destruction / Move
// ---------------------------------------------------------------------------

GameNetworkManager::GameNetworkManager()
    : impl_(std::make_unique<Impl>()) {
    impl_->owner = this;
}

GameNetworkManager::~GameNetworkManager() {
    if (impl_) {
        stopAll();
    }
}

GameNetworkManager::GameNetworkManager(GameNetworkManager&&) noexcept = default;

GameNetworkManager& GameNetworkManager::operator=(GameNetworkManager&& other) noexcept {
    if (this != &other) {
        if (impl_) {
            stopAll();
        }
        impl_ = std::move(other.impl_);
        if (impl_) {
            impl_->owner = this;
        }
    }
    return *this;
}

// ---------------------------------------------------------------------------
// listen()
// ---------------------------------------------------------------------------

GameResult<void> GameNetworkManager::listen(uint16_t port, Protocol protocol) {
    // Check if already listening on this protocol
    if (impl_->servers.count(protocol) > 0) {
        return GameResult<void>::err(
            GameError(ErrorCode::AlreadyExists,
                      std::string("already listening on ") +
                          std::string(protocolName(protocol))));
    }

    auto server = impl_->createServer(protocol);
    if (!server) {
        return GameResult<void>::err(
            GameError(ErrorCode::ListenFailed,
                      "failed to create server for " +
                          std::string(protocolName(protocol))));
    }

    impl_->setupCallbacks(server, protocol);

    auto result = server->start(port);
    if (result.is_err()) {
        return GameResult<void>::err(
            GameError(ErrorCode::ListenFailed,
                      "failed to start " + std::string(protocolName(protocol)) +
                          " server on port " + std::to_string(port)));
    }

    impl_->servers.emplace(protocol, std::move(server));
    return GameResult<void>::ok();
}

// ---------------------------------------------------------------------------
// stop()
// ---------------------------------------------------------------------------

GameResult<void> GameNetworkManager::stop(Protocol protocol) {
    auto it = impl_->servers.find(protocol);
    if (it == impl_->servers.end()) {
        return GameResult<void>::err(
            GameError(ErrorCode::NotFound,
                      std::string(protocolName(protocol)) + " server not running"));
    }

    (void)it->second->stop();

    // Clean up sessions for this protocol
    {
        std::unique_lock lock(impl_->sessionMutex);
        for (auto sit = impl_->sessions.begin(); sit != impl_->sessions.end();) {
            if (sit->second.protocol == protocol) {
                impl_->reverseMap.erase(sit->second.kcSessionId);
                sit = impl_->sessions.erase(sit);
            } else {
                ++sit;
            }
        }
    }

    impl_->servers.erase(it);
    return GameResult<void>::ok();
}

// ---------------------------------------------------------------------------
// stopAll()
// ---------------------------------------------------------------------------

void GameNetworkManager::stopAll() {
    for (auto& [proto, server] : impl_->servers) {
        (void)server->stop();
    }
    impl_->servers.clear();

    std::unique_lock lock(impl_->sessionMutex);
    impl_->sessions.clear();
    impl_->reverseMap.clear();
}

// ---------------------------------------------------------------------------
// send()
// ---------------------------------------------------------------------------

GameResult<void> GameNetworkManager::send(
    SessionId session, const NetworkMessage& msg) {
    std::shared_ptr<kcenon::network::interfaces::i_session> kcSession;
    {
        std::shared_lock lock(impl_->sessionMutex);
        auto it = impl_->sessions.find(session);
        if (it == impl_->sessions.end()) {
            return GameResult<void>::err(
                GameError(ErrorCode::SessionNotFound,
                          "session " + std::to_string(session.value()) +
                              " not found"));
        }
        kcSession = it->second.kcSession;
    }

    auto wireData = msg.serialize();
    auto result = kcSession->send(std::move(wireData));
    if (result.is_err()) {
        return GameResult<void>::err(
            GameError(ErrorCode::SendFailed,
                      "send failed for session " +
                          std::to_string(session.value())));
    }

    return GameResult<void>::ok();
}

// ---------------------------------------------------------------------------
// broadcast()
// ---------------------------------------------------------------------------

GameResult<void> GameNetworkManager::broadcast(const NetworkMessage& msg) {
    auto wireData = msg.serialize();

    std::shared_lock lock(impl_->sessionMutex);
    for (auto& [sid, internal] : impl_->sessions) {
        if (internal.kcSession && internal.kcSession->is_connected()) {
            // Copy for each send (kcenon takes by rvalue)
            auto copy = wireData;
            (void)internal.kcSession->send(std::move(copy));
        }
    }
    return GameResult<void>::ok();
}

// ---------------------------------------------------------------------------
// close()
// ---------------------------------------------------------------------------

void GameNetworkManager::close(SessionId session) {
    std::shared_ptr<kcenon::network::interfaces::i_session> kcSession;
    {
        std::shared_lock lock(impl_->sessionMutex);
        auto it = impl_->sessions.find(session);
        if (it == impl_->sessions.end()) {
            return;
        }
        kcSession = it->second.kcSession;
    }

    if (kcSession) {
        kcSession->close();
    }
    // Session removal happens in the disconnection callback
}

// ---------------------------------------------------------------------------
// registerHandler() / unregisterHandler()
// ---------------------------------------------------------------------------

void GameNetworkManager::registerHandler(uint16_t opcode, MessageHandler handler) {
    std::unique_lock lock(impl_->handlerMutex);
    impl_->handlers[opcode] = std::move(handler);
}

void GameNetworkManager::unregisterHandler(uint16_t opcode) {
    std::unique_lock lock(impl_->handlerMutex);
    impl_->handlers.erase(opcode);
}

// ---------------------------------------------------------------------------
// sessionInfo()
// ---------------------------------------------------------------------------

std::optional<SessionInfo> GameNetworkManager::sessionInfo(SessionId id) const {
    std::shared_lock lock(impl_->sessionMutex);
    auto it = impl_->sessions.find(id);
    if (it == impl_->sessions.end()) {
        return std::nullopt;
    }
    return it->second.info;
}

// ---------------------------------------------------------------------------
// sessionCount()
// ---------------------------------------------------------------------------

std::size_t GameNetworkManager::sessionCount() const {
    std::shared_lock lock(impl_->sessionMutex);
    return impl_->sessions.size();
}

} // namespace cgs::foundation
