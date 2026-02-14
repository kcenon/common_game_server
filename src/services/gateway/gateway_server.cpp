/// @file gateway_server.cpp
/// @brief GatewayServer implementation orchestrating session management,
///        authentication delegation, message routing, and rate limiting.

#include "cgs/service/gateway_server.hpp"

#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_error.hpp"
#include "cgs/service/auth_server.hpp"
#include "cgs/service/gateway_session_manager.hpp"
#include "cgs/service/route_table.hpp"
#include "cgs/service/token_bucket.hpp"

#include <atomic>
#include <string>

namespace cgs::service {

using cgs::foundation::ErrorCode;
using cgs::foundation::GameError;
using cgs::foundation::GameResult;
using cgs::foundation::SessionId;

// -- Helpers ------------------------------------------------------------------

namespace {

GatewayAction makeReply(uint16_t opcode, std::vector<uint8_t> payload = {}) {
    GatewayAction action;
    action.type = GatewayActionType::Reply;
    action.replyOpcode = opcode;
    action.replyPayload = std::move(payload);
    return action;
}

GatewayAction makeForward(std::string service) {
    GatewayAction action;
    action.type = GatewayActionType::Forward;
    action.targetService = std::move(service);
    return action;
}

GatewayAction makeDrop(std::string reason) {
    GatewayAction action;
    action.type = GatewayActionType::Drop;
    action.reason = std::move(reason);
    return action;
}

std::vector<uint8_t> statusPayload(bool success) {
    return {success ? static_cast<uint8_t>(0x00) : static_cast<uint8_t>(0x01)};
}

}  // anonymous namespace

// -- Impl ---------------------------------------------------------------------

struct GatewayServer::Impl {
    GatewayConfig config;
    std::shared_ptr<AuthServer> authServer;

    GatewaySessionManager sessions;
    RouteTable routes;
    TokenBucket rateLimiter;

    std::atomic<bool> running{false};
    std::atomic<uint64_t> messagesRouted{0};
    std::atomic<uint64_t> messagesDropped{0};
    std::atomic<uint64_t> authSuccessCount{0};
    std::atomic<uint64_t> authFailureCount{0};
    std::atomic<uint64_t> rateLimitHits{0};

    explicit Impl(GatewayConfig cfg, std::shared_ptr<AuthServer> auth)
        : config(std::move(cfg)),
          authServer(std::move(auth)),
          sessions(config.maxConnections),
          rateLimiter(config.rateLimitCapacity, config.rateLimitRefillRate) {}

    /// Handle gateway-level opcodes (0x0000-0x00FF) internally.
    GameResult<GatewayAction> handleGatewayOpcode(SessionId sessionId,
                                                  uint16_t opcode,
                                                  std::vector<uint8_t> payload,
                                                  const ClientSession& session) {
        switch (opcode) {
            case GatewayOpcode::Authenticate: {
                if (session.state != ClientState::Unauthenticated) {
                    messagesDropped.fetch_add(1, std::memory_order_relaxed);
                    return GameResult<GatewayAction>::ok(makeDrop("already authenticated"));
                }

                std::string token(payload.begin(), payload.end());
                auto validationResult = authServer->validateToken(token);

                if (validationResult.hasError()) {
                    authFailureCount.fetch_add(1, std::memory_order_relaxed);
                    return GameResult<GatewayAction>::ok(
                        makeReply(GatewayOpcode::AuthResult, statusPayload(false)));
                }

                auto& claims = validationResult.value();
                uint64_t userId = 0;
                try {
                    userId = std::stoull(claims.subject);
                } catch (...) {
                    // Subject might not be numeric; use 0.
                }

                if (!sessions.authenticateSession(sessionId, claims, userId)) {
                    authFailureCount.fetch_add(1, std::memory_order_relaxed);
                    return GameResult<GatewayAction>::ok(
                        makeReply(GatewayOpcode::AuthResult, statusPayload(false)));
                }
                authSuccessCount.fetch_add(1, std::memory_order_relaxed);
                return GameResult<GatewayAction>::ok(
                    makeReply(GatewayOpcode::AuthResult, statusPayload(true)));
            }

            case GatewayOpcode::MigrationAck: {
                if (session.state != ClientState::Migrating) {
                    messagesDropped.fetch_add(1, std::memory_order_relaxed);
                    return GameResult<GatewayAction>::ok(makeDrop("not in migration state"));
                }

                (void)sessions.completeMigration(sessionId);
                return GameResult<GatewayAction>::ok(makeDrop("migration ack processed"));
            }

            case GatewayOpcode::Pong: {
                // Pong is a heartbeat response; activity already touched.
                return GameResult<GatewayAction>::ok(makeDrop("pong received"));
            }

            default: {
                messagesDropped.fetch_add(1, std::memory_order_relaxed);
                return GameResult<GatewayAction>::ok(makeDrop("unknown gateway opcode"));
            }
        }
    }
};

// -- Construction / destruction / move ----------------------------------------

GatewayServer::GatewayServer(GatewayConfig config, std::shared_ptr<AuthServer> authServer)
    : impl_(std::make_unique<Impl>(std::move(config), std::move(authServer))) {}

GatewayServer::~GatewayServer() {
    if (impl_ && impl_->running.load()) {
        stop();
    }
}

GatewayServer::GatewayServer(GatewayServer&&) noexcept = default;
GatewayServer& GatewayServer::operator=(GatewayServer&&) noexcept = default;

// -- Lifecycle ----------------------------------------------------------------

GameResult<void> GatewayServer::start() {
    if (impl_->running.load()) {
        return GameResult<void>::err(
            GameError(ErrorCode::GatewayAlreadyStarted, "gateway server is already running"));
    }

    impl_->running.store(true);
    return GameResult<void>::ok();
}

void GatewayServer::stop() {
    impl_->running.store(false);
}

bool GatewayServer::isRunning() const noexcept {
    return impl_->running.load();
}

// -- Route configuration ------------------------------------------------------

void GatewayServer::addRoute(uint16_t opcodeMin,
                             uint16_t opcodeMax,
                             std::string service,
                             bool requiresAuth) {
    impl_->routes.addRoute(opcodeMin, opcodeMax, std::move(service), requiresAuth);
}

// -- Connection handling ------------------------------------------------------

GameResult<void> GatewayServer::handleConnect(SessionId sessionId, std::string remoteAddress) {
    if (!impl_->running.load()) {
        return GameResult<void>::err(
            GameError(ErrorCode::GatewayNotStarted, "gateway server is not running"));
    }

    if (!impl_->sessions.createSession(sessionId, std::move(remoteAddress))) {
        return GameResult<void>::err(GameError(ErrorCode::ConnectionLimitReached,
                                               "connection limit reached or duplicate session"));
    }

    return GameResult<void>::ok();
}

void GatewayServer::handleDisconnect(SessionId sessionId) {
    auto session = impl_->sessions.getSession(sessionId);
    if (session.has_value()) {
        impl_->rateLimiter.remove(session->remoteAddress);
    }
    impl_->sessions.removeSession(sessionId);
}

GameResult<GatewayAction> GatewayServer::handleMessage(SessionId sessionId,
                                                       uint16_t opcode,
                                                       std::vector<uint8_t> payload) {
    if (!impl_->running.load()) {
        return GameResult<GatewayAction>::err(
            GameError(ErrorCode::GatewayNotStarted, "gateway server is not running"));
    }

    auto session = impl_->sessions.getSession(sessionId);
    if (!session.has_value()) {
        return GameResult<GatewayAction>::err(
            GameError(ErrorCode::SessionNotFound, "session not found"));
    }

    // Rate limit check using the client's IP address.
    if (!impl_->rateLimiter.consume(session->remoteAddress)) {
        impl_->rateLimitHits.fetch_add(1, std::memory_order_relaxed);
        impl_->messagesDropped.fetch_add(1, std::memory_order_relaxed);
        return GameResult<GatewayAction>::err(
            GameError(ErrorCode::GatewayRateLimited, "rate limit exceeded"));
    }

    // Update activity timestamp.
    impl_->sessions.touchSession(sessionId);

    // Handle gateway-level opcodes (0x0000-0x00FF).
    if (RouteTable::isGatewayOpcode(opcode)) {
        return impl_->handleGatewayOpcode(sessionId, opcode, std::move(payload), *session);
    }

    // Resolve the route for non-gateway opcodes.
    auto match = impl_->routes.resolve(opcode);
    if (!match.has_value()) {
        impl_->messagesDropped.fetch_add(1, std::memory_order_relaxed);
        return GameResult<GatewayAction>::ok(makeDrop("no route for opcode"));
    }

    // Check authentication requirement.
    if (match->requiresAuth && session->state != ClientState::Authenticated) {
        impl_->messagesDropped.fetch_add(1, std::memory_order_relaxed);
        return GameResult<GatewayAction>::err(
            GameError(ErrorCode::ClientNotAuthenticated, "authentication required for this route"));
    }

    impl_->messagesRouted.fetch_add(1, std::memory_order_relaxed);
    return GameResult<GatewayAction>::ok(makeForward(match->service));
}

// -- Migration ----------------------------------------------------------------

GameResult<void> GatewayServer::initiateServerTransfer(SessionId sessionId,
                                                       std::string targetService) {
    if (!impl_->running.load()) {
        return GameResult<void>::err(
            GameError(ErrorCode::GatewayNotStarted, "gateway server is not running"));
    }

    if (!impl_->sessions.beginMigration(sessionId, std::move(targetService))) {
        return GameResult<void>::err(GameError(ErrorCode::MigrationFailed,
                                               "cannot begin migration (session not authenticated "
                                               "or not found)"));
    }

    return GameResult<void>::ok();
}

// -- Maintenance --------------------------------------------------------------

std::vector<SessionId> GatewayServer::cleanupIdleSessions() {
    auto idle = impl_->sessions.findIdleSessions(impl_->config.idleTimeout);
    for (auto sid : idle) {
        handleDisconnect(sid);
    }
    return idle;
}

std::vector<SessionId> GatewayServer::cleanupExpiredAuth() {
    auto expired = impl_->sessions.findExpiredAuthSessions(impl_->config.authTimeout);
    for (auto sid : expired) {
        handleDisconnect(sid);
    }
    return expired;
}

// -- Statistics ---------------------------------------------------------------

GatewayStats GatewayServer::stats() const {
    GatewayStats s;
    s.totalConnections = impl_->sessions.sessionCount();
    s.authenticatedConnections = impl_->sessions.sessionCount(ClientState::Authenticated);
    s.unauthenticatedConnections = impl_->sessions.sessionCount(ClientState::Unauthenticated);
    s.migratingConnections = impl_->sessions.sessionCount(ClientState::Migrating);
    s.messagesRouted = impl_->messagesRouted.load(std::memory_order_relaxed);
    s.messagesDropped = impl_->messagesDropped.load(std::memory_order_relaxed);
    s.authSuccessCount = impl_->authSuccessCount.load(std::memory_order_relaxed);
    s.authFailureCount = impl_->authFailureCount.load(std::memory_order_relaxed);
    s.rateLimitHits = impl_->rateLimitHits.load(std::memory_order_relaxed);
    return s;
}

const GatewayConfig& GatewayServer::config() const noexcept {
    return impl_->config;
}

}  // namespace cgs::service
