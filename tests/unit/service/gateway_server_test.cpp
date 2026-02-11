#include <gtest/gtest.h>

#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/types.hpp"
#include "cgs/service/auth_server.hpp"
#include "cgs/service/gateway_server.hpp"
#include "cgs/service/gateway_types.hpp"
#include "cgs/service/token_store.hpp"
#include "cgs/service/user_repository.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace cgs::service;
using cgs::foundation::ErrorCode;
using cgs::foundation::SessionId;

// =============================================================================
// Test fixture
// =============================================================================

class GatewayServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a real AuthServer with in-memory backends.
        auto userRepo = std::make_shared<InMemoryUserRepository>();
        auto tokenStore = std::make_shared<InMemoryTokenStore>();

        AuthConfig authConfig;
        authConfig.signingKey = "test-key-must-be-at-least-32-bytes-long!!";
        authConfig.accessTokenExpiry = std::chrono::seconds{300};
        authConfig.refreshTokenExpiry = std::chrono::seconds{3600};
        authConfig.minPasswordLength = 8;
        authConfig.rateLimitMaxAttempts = 100;
        authConfig.rateLimitWindow = std::chrono::seconds{60};

        authServer_ = std::make_shared<AuthServer>(
            std::move(authConfig), userRepo, tokenStore);

        // Register a test user and obtain access token.
        auto regResult = authServer_->registerUser(
            {"player1", "player1@test.com", "StrongPass1!"});
        ASSERT_TRUE(regResult.hasValue());

        auto loginResult = authServer_->login(
            "player1", "StrongPass1!", "127.0.0.1");
        ASSERT_TRUE(loginResult.hasValue());
        validToken_ = loginResult.value().accessToken;

        // Create the GatewayServer.
        GatewayConfig gwConfig;
        gwConfig.maxConnections = 100;
        gwConfig.rateLimitCapacity = 50;
        gwConfig.rateLimitRefillRate = 50;
        gwConfig.authTimeout = std::chrono::seconds{2};
        gwConfig.idleTimeout = std::chrono::seconds{3};

        gateway_ = std::make_unique<GatewayServer>(
            std::move(gwConfig), authServer_);

        // Configure routes.
        gateway_->addRoute(0x0100, 0x01FF, "game");
        gateway_->addRoute(0x0200, 0x02FF, "lobby");
        gateway_->addRoute(0x0300, 0x03FF, "chat", false);

        auto startResult = gateway_->start();
        ASSERT_TRUE(startResult.hasValue());
    }

    static SessionId sid(uint64_t id) { return SessionId(id); }

    std::vector<uint8_t> tokenPayload() const {
        return {validToken_.begin(), validToken_.end()};
    }

    std::shared_ptr<AuthServer> authServer_;
    std::unique_ptr<GatewayServer> gateway_;
    std::string validToken_;
};

// =============================================================================
// Lifecycle tests
// =============================================================================

TEST_F(GatewayServerTest, StartAndStop) {
    EXPECT_TRUE(gateway_->isRunning());
    gateway_->stop();
    EXPECT_FALSE(gateway_->isRunning());
}

TEST_F(GatewayServerTest, DoubleStartFails) {
    auto result = gateway_->start();
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::GatewayAlreadyStarted);
}

TEST_F(GatewayServerTest, OperationsFailWhenStopped) {
    gateway_->stop();

    auto connectResult = gateway_->handleConnect(sid(1), "10.0.0.1");
    EXPECT_TRUE(connectResult.hasError());
    EXPECT_EQ(connectResult.error().code(), ErrorCode::GatewayNotStarted);

    auto msgResult = gateway_->handleMessage(sid(1), 0x0100, {});
    EXPECT_TRUE(msgResult.hasError());
    EXPECT_EQ(msgResult.error().code(), ErrorCode::GatewayNotStarted);
}

// =============================================================================
// Connection handling tests (SRS-SVC-002.1)
// =============================================================================

TEST_F(GatewayServerTest, ConnectClient) {
    auto result = gateway_->handleConnect(sid(1), "10.0.0.1");
    ASSERT_TRUE(result.hasValue());

    auto s = gateway_->stats();
    EXPECT_EQ(s.totalConnections, 1u);
    EXPECT_EQ(s.unauthenticatedConnections, 1u);
}

TEST_F(GatewayServerTest, ConnectDuplicateSessionFails) {
    ASSERT_TRUE(gateway_->handleConnect(sid(1), "10.0.0.1").hasValue());

    auto result = gateway_->handleConnect(sid(1), "10.0.0.2");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::ConnectionLimitReached);
}

TEST_F(GatewayServerTest, DisconnectClient) {
    ASSERT_TRUE(gateway_->handleConnect(sid(1), "10.0.0.1").hasValue());
    EXPECT_EQ(gateway_->stats().totalConnections, 1u);

    gateway_->handleDisconnect(sid(1));
    EXPECT_EQ(gateway_->stats().totalConnections, 0u);
}

TEST_F(GatewayServerTest, DisconnectNonexistentSession) {
    // Should not crash.
    gateway_->handleDisconnect(sid(999));
}

// =============================================================================
// Authentication tests (SRS-SVC-002.2)
// =============================================================================

TEST_F(GatewayServerTest, AuthenticateWithValidToken) {
    ASSERT_TRUE(gateway_->handleConnect(sid(1), "10.0.0.1").hasValue());

    auto result = gateway_->handleMessage(
        sid(1), GatewayOpcode::Authenticate, tokenPayload());
    ASSERT_TRUE(result.hasValue());

    auto& action = result.value();
    EXPECT_EQ(action.type, GatewayActionType::Reply);
    EXPECT_EQ(action.replyOpcode, GatewayOpcode::AuthResult);
    ASSERT_FALSE(action.replyPayload.empty());
    EXPECT_EQ(action.replyPayload[0], 0x00); // success

    auto s = gateway_->stats();
    EXPECT_EQ(s.authenticatedConnections, 1u);
    EXPECT_EQ(s.unauthenticatedConnections, 0u);
    EXPECT_EQ(s.authSuccessCount, 1u);
}

TEST_F(GatewayServerTest, AuthenticateWithInvalidToken) {
    ASSERT_TRUE(gateway_->handleConnect(sid(1), "10.0.0.1").hasValue());

    std::string badToken = "this-is-not-a-valid-jwt";
    std::vector<uint8_t> payload(badToken.begin(), badToken.end());
    auto result = gateway_->handleMessage(
        sid(1), GatewayOpcode::Authenticate, payload);
    ASSERT_TRUE(result.hasValue());

    auto& action = result.value();
    EXPECT_EQ(action.type, GatewayActionType::Reply);
    EXPECT_EQ(action.replyOpcode, GatewayOpcode::AuthResult);
    ASSERT_FALSE(action.replyPayload.empty());
    EXPECT_EQ(action.replyPayload[0], 0x01); // failure

    EXPECT_EQ(gateway_->stats().authFailureCount, 1u);
}

TEST_F(GatewayServerTest, AuthenticateAlreadyAuthenticatedDrops) {
    ASSERT_TRUE(gateway_->handleConnect(sid(1), "10.0.0.1").hasValue());

    // First authentication succeeds.
    auto r1 = gateway_->handleMessage(
        sid(1), GatewayOpcode::Authenticate, tokenPayload());
    ASSERT_TRUE(r1.hasValue());
    EXPECT_EQ(r1.value().replyPayload[0], 0x00);

    // Second authentication is dropped.
    auto r2 = gateway_->handleMessage(
        sid(1), GatewayOpcode::Authenticate, tokenPayload());
    ASSERT_TRUE(r2.hasValue());
    EXPECT_EQ(r2.value().type, GatewayActionType::Drop);
}

// =============================================================================
// Message routing tests (SRS-SVC-002.3)
// =============================================================================

TEST_F(GatewayServerTest, RouteMessageToGameService) {
    ASSERT_TRUE(gateway_->handleConnect(sid(1), "10.0.0.1").hasValue());
    ASSERT_TRUE(gateway_->handleMessage(
        sid(1), GatewayOpcode::Authenticate, tokenPayload()).hasValue());

    auto result = gateway_->handleMessage(sid(1), 0x0150, {0x01, 0x02});
    ASSERT_TRUE(result.hasValue());

    auto& action = result.value();
    EXPECT_EQ(action.type, GatewayActionType::Forward);
    EXPECT_EQ(action.targetService, "game");
}

TEST_F(GatewayServerTest, RouteMessageToLobbyService) {
    ASSERT_TRUE(gateway_->handleConnect(sid(1), "10.0.0.1").hasValue());
    ASSERT_TRUE(gateway_->handleMessage(
        sid(1), GatewayOpcode::Authenticate, tokenPayload()).hasValue());

    auto result = gateway_->handleMessage(sid(1), 0x0250, {});
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().targetService, "lobby");
}

TEST_F(GatewayServerTest, RouteToNoAuthService) {
    ASSERT_TRUE(gateway_->handleConnect(sid(1), "10.0.0.1").hasValue());

    // Chat route does not require auth.
    auto result = gateway_->handleMessage(sid(1), 0x0350, {});
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().type, GatewayActionType::Forward);
    EXPECT_EQ(result.value().targetService, "chat");
}

TEST_F(GatewayServerTest, RouteRequiresAuthWhenUnauthenticated) {
    ASSERT_TRUE(gateway_->handleConnect(sid(1), "10.0.0.1").hasValue());

    auto result = gateway_->handleMessage(sid(1), 0x0150, {});
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::ClientNotAuthenticated);
}

TEST_F(GatewayServerTest, RouteUnknownOpcode) {
    ASSERT_TRUE(gateway_->handleConnect(sid(1), "10.0.0.1").hasValue());
    ASSERT_TRUE(gateway_->handleMessage(
        sid(1), GatewayOpcode::Authenticate, tokenPayload()).hasValue());

    auto result = gateway_->handleMessage(sid(1), 0x0500, {});
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().type, GatewayActionType::Drop);
}

TEST_F(GatewayServerTest, MessageToNonexistentSession) {
    auto result = gateway_->handleMessage(sid(999), 0x0100, {});
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::SessionNotFound);
}

// =============================================================================
// Migration tests (SRS-SVC-002.4)
// =============================================================================

TEST_F(GatewayServerTest, ServerTransferLifecycle) {
    ASSERT_TRUE(gateway_->handleConnect(sid(1), "10.0.0.1").hasValue());
    ASSERT_TRUE(gateway_->handleMessage(
        sid(1), GatewayOpcode::Authenticate, tokenPayload()).hasValue());

    // Initiate server transfer.
    auto transferResult = gateway_->initiateServerTransfer(
        sid(1), "game-server-2");
    ASSERT_TRUE(transferResult.hasValue());
    EXPECT_EQ(gateway_->stats().migratingConnections, 1u);

    // Client acknowledges migration.
    auto ackResult = gateway_->handleMessage(
        sid(1), GatewayOpcode::MigrationAck, {});
    ASSERT_TRUE(ackResult.hasValue());

    // Back to authenticated state.
    EXPECT_EQ(gateway_->stats().authenticatedConnections, 1u);
    EXPECT_EQ(gateway_->stats().migratingConnections, 0u);
}

TEST_F(GatewayServerTest, ServerTransferUnauthenticatedFails) {
    ASSERT_TRUE(gateway_->handleConnect(sid(1), "10.0.0.1").hasValue());

    auto result = gateway_->initiateServerTransfer(sid(1), "game-server-2");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::MigrationFailed);
}

TEST_F(GatewayServerTest, MigrationAckWithoutMigrationDrops) {
    ASSERT_TRUE(gateway_->handleConnect(sid(1), "10.0.0.1").hasValue());
    ASSERT_TRUE(gateway_->handleMessage(
        sid(1), GatewayOpcode::Authenticate, tokenPayload()).hasValue());

    auto result = gateway_->handleMessage(
        sid(1), GatewayOpcode::MigrationAck, {});
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().type, GatewayActionType::Drop);
}

// =============================================================================
// Rate limiting tests (SRS-SVC-002.5)
// =============================================================================

TEST_F(GatewayServerTest, RateLimitExceeded) {
    // Create a gateway with a very small rate limit for deterministic testing.
    GatewayConfig limitedConfig;
    limitedConfig.maxConnections = 100;
    limitedConfig.rateLimitCapacity = 5;
    limitedConfig.rateLimitRefillRate = 1; // 1 token/sec, negligible refill
    limitedConfig.authTimeout = std::chrono::seconds{10};
    limitedConfig.idleTimeout = std::chrono::seconds{300};

    auto limitedGw = std::make_unique<GatewayServer>(
        std::move(limitedConfig), authServer_);
    limitedGw->addRoute(0x0300, 0x03FF, "chat", false);
    ASSERT_TRUE(limitedGw->start().hasValue());

    ASSERT_TRUE(limitedGw->handleConnect(sid(1), "10.0.0.1").hasValue());

    // Consume all 5 tokens (chat route, no auth required).
    for (int i = 0; i < 5; ++i) {
        auto r = limitedGw->handleMessage(sid(1), 0x0350, {});
        ASSERT_TRUE(r.hasValue()) << "Failed at message " << i;
    }

    // The next message should be rate-limited.
    auto result = limitedGw->handleMessage(sid(1), 0x0350, {});
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::GatewayRateLimited);
    EXPECT_GT(limitedGw->stats().rateLimitHits, 0u);
}

// =============================================================================
// Heartbeat tests
// =============================================================================

TEST_F(GatewayServerTest, PongHandled) {
    ASSERT_TRUE(gateway_->handleConnect(sid(1), "10.0.0.1").hasValue());

    auto result = gateway_->handleMessage(
        sid(1), GatewayOpcode::Pong, {});
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().type, GatewayActionType::Drop);
    EXPECT_EQ(result.value().reason, "pong received");
}

// =============================================================================
// Statistics tests
// =============================================================================

TEST_F(GatewayServerTest, StatsReflectState) {
    auto s0 = gateway_->stats();
    EXPECT_EQ(s0.totalConnections, 0u);

    ASSERT_TRUE(gateway_->handleConnect(sid(1), "10.0.0.1").hasValue());
    ASSERT_TRUE(gateway_->handleConnect(sid(2), "10.0.0.2").hasValue());

    auto s1 = gateway_->stats();
    EXPECT_EQ(s1.totalConnections, 2u);
    EXPECT_EQ(s1.unauthenticatedConnections, 2u);

    // Authenticate one.
    ASSERT_TRUE(gateway_->handleMessage(
        sid(1), GatewayOpcode::Authenticate, tokenPayload()).hasValue());

    auto s2 = gateway_->stats();
    EXPECT_EQ(s2.authenticatedConnections, 1u);
    EXPECT_EQ(s2.unauthenticatedConnections, 1u);
    EXPECT_EQ(s2.authSuccessCount, 1u);

    // Disconnect one.
    gateway_->handleDisconnect(sid(2));

    auto s3 = gateway_->stats();
    EXPECT_EQ(s3.totalConnections, 1u);
}

TEST_F(GatewayServerTest, StatsCountRouted) {
    ASSERT_TRUE(gateway_->handleConnect(sid(1), "10.0.0.1").hasValue());
    ASSERT_TRUE(gateway_->handleMessage(
        sid(1), GatewayOpcode::Authenticate, tokenPayload()).hasValue());

    (void)gateway_->handleMessage(sid(1), 0x0150, {});
    (void)gateway_->handleMessage(sid(1), 0x0250, {});

    auto s = gateway_->stats();
    EXPECT_EQ(s.messagesRouted, 2u);
}

// =============================================================================
// Config access
// =============================================================================

TEST_F(GatewayServerTest, ConfigAccess) {
    EXPECT_EQ(gateway_->config().tcpPort, 8080);
    EXPECT_EQ(gateway_->config().webSocketPort, 8081);
    EXPECT_EQ(gateway_->config().maxConnections, 100u);
}

// =============================================================================
// Full lifecycle integration
// =============================================================================

TEST_F(GatewayServerTest, FullClientLifecycle) {
    // 1. Client connects.
    ASSERT_TRUE(gateway_->handleConnect(sid(1), "192.168.1.10").hasValue());
    EXPECT_EQ(gateway_->stats().totalConnections, 1u);

    // 2. Client authenticates.
    auto authResult = gateway_->handleMessage(
        sid(1), GatewayOpcode::Authenticate, tokenPayload());
    ASSERT_TRUE(authResult.hasValue());
    EXPECT_EQ(authResult.value().replyPayload[0], 0x00);

    // 3. Client sends game messages.
    auto gameMsg = gateway_->handleMessage(sid(1), 0x0150, {0xAA, 0xBB});
    ASSERT_TRUE(gameMsg.hasValue());
    EXPECT_EQ(gameMsg.value().type, GatewayActionType::Forward);
    EXPECT_EQ(gameMsg.value().targetService, "game");

    // 4. Server transfer initiated.
    ASSERT_TRUE(
        gateway_->initiateServerTransfer(sid(1), "game-server-2").hasValue());
    EXPECT_EQ(gateway_->stats().migratingConnections, 1u);

    // 5. Client acknowledges migration.
    auto ack = gateway_->handleMessage(
        sid(1), GatewayOpcode::MigrationAck, {});
    ASSERT_TRUE(ack.hasValue());

    // 6. Client continues sending after migration.
    auto lobbyMsg = gateway_->handleMessage(sid(1), 0x0250, {});
    ASSERT_TRUE(lobbyMsg.hasValue());
    EXPECT_EQ(lobbyMsg.value().targetService, "lobby");

    // 7. Client disconnects.
    gateway_->handleDisconnect(sid(1));
    EXPECT_EQ(gateway_->stats().totalConnections, 0u);

    auto finalStats = gateway_->stats();
    EXPECT_EQ(finalStats.authSuccessCount, 1u);
    EXPECT_GE(finalStats.messagesRouted, 2u);
}
