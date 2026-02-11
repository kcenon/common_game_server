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
// Integration fixture: Gateway ↔ Auth round-trip
// =============================================================================

class GatewayIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto userRepo = std::make_shared<InMemoryUserRepository>();
        auto tokenStore = std::make_shared<InMemoryTokenStore>();

        AuthConfig authConfig;
        authConfig.signingKey = "integration-test-key-at-least-32-bytes-long!";
        authConfig.accessTokenExpiry = std::chrono::seconds{600};
        authConfig.refreshTokenExpiry = std::chrono::seconds{3600};
        authConfig.minPasswordLength = 8;
        authConfig.rateLimitMaxAttempts = 1000;
        authConfig.rateLimitWindow = std::chrono::seconds{60};

        authServer_ = std::make_shared<AuthServer>(
            std::move(authConfig), userRepo, tokenStore);

        GatewayConfig gwConfig;
        gwConfig.maxConnections = 1000;
        gwConfig.rateLimitCapacity = 200;
        gwConfig.rateLimitRefillRate = 200;
        gwConfig.authTimeout = std::chrono::seconds{1};
        gwConfig.idleTimeout = std::chrono::seconds{2};

        gateway_ = std::make_unique<GatewayServer>(
            std::move(gwConfig), authServer_);

        gateway_->addRoute(0x0100, 0x01FF, "game");
        gateway_->addRoute(0x0200, 0x02FF, "lobby");

        auto startResult = gateway_->start();
        ASSERT_TRUE(startResult.hasValue());
    }

    std::string registerAndLogin(const std::string& username) {
        auto reg = authServer_->registerUser(
            {username, username + "@test.com", "StrongPass1!"});
        EXPECT_TRUE(reg.hasValue());

        auto login = authServer_->login(username, "StrongPass1!", "127.0.0.1");
        EXPECT_TRUE(login.hasValue());
        return login.value().accessToken;
    }

    static SessionId sid(uint64_t id) { return SessionId(id); }

    std::shared_ptr<AuthServer> authServer_;
    std::unique_ptr<GatewayServer> gateway_;
};

// =============================================================================
// Multi-client round-trip: register → connect → auth → route → disconnect
// =============================================================================

TEST_F(GatewayIntegrationTest, MultiClientRoundTrip) {
    constexpr int kClients = 10;
    std::vector<std::string> tokens;

    // Register all clients.
    for (std::size_t i = 0; i < static_cast<std::size_t>(kClients); ++i) {
        tokens.push_back(registerAndLogin("user" + std::to_string(i)));
    }

    // All clients connect and authenticate.
    for (std::size_t i = 0; i < static_cast<std::size_t>(kClients); ++i) {
        auto sessionId = sid(i + 1);
        ASSERT_TRUE(gateway_->handleConnect(
            sessionId, "10.0.0." + std::to_string(i + 1)).hasValue());

        std::vector<uint8_t> payload(tokens[i].begin(), tokens[i].end());
        auto authResult = gateway_->handleMessage(
            sessionId, GatewayOpcode::Authenticate, payload);
        ASSERT_TRUE(authResult.hasValue());
        EXPECT_EQ(authResult.value().replyPayload[0], 0x00);
    }

    auto s = gateway_->stats();
    EXPECT_EQ(s.totalConnections, static_cast<std::size_t>(kClients));
    EXPECT_EQ(s.authenticatedConnections, static_cast<std::size_t>(kClients));
    EXPECT_EQ(s.authSuccessCount, static_cast<uint64_t>(kClients));

    // All clients send game messages.
    for (std::size_t i = 0; i < static_cast<std::size_t>(kClients); ++i) {
        auto sessionId = sid(i + 1);
        auto result = gateway_->handleMessage(sessionId, 0x0150, {0xAA});
        ASSERT_TRUE(result.hasValue());
        EXPECT_EQ(result.value().type, GatewayActionType::Forward);
        EXPECT_EQ(result.value().targetService, "game");
    }

    EXPECT_EQ(gateway_->stats().messagesRouted,
              static_cast<uint64_t>(kClients));

    // All clients disconnect.
    for (std::size_t i = 0; i < static_cast<std::size_t>(kClients); ++i) {
        gateway_->handleDisconnect(sid(i + 1));
    }

    EXPECT_EQ(gateway_->stats().totalConnections, 0u);
}

// =============================================================================
// Expired auth cleanup
// =============================================================================

TEST_F(GatewayIntegrationTest, ExpiredAuthCleanup) {
    ASSERT_TRUE(gateway_->handleConnect(sid(1), "10.0.0.1").hasValue());
    ASSERT_TRUE(gateway_->handleConnect(sid(2), "10.0.0.2").hasValue());

    // Authenticate only client 2.
    auto token = registerAndLogin("auth_user");
    std::vector<uint8_t> payload(token.begin(), token.end());
    auto authResult = gateway_->handleMessage(
        sid(2), GatewayOpcode::Authenticate, payload);
    ASSERT_TRUE(authResult.hasValue());

    // Wait for auth timeout (1 second + margin).
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    // Cleanup should remove unauthenticated session (sid 1) only.
    auto expired = gateway_->cleanupExpiredAuth();
    EXPECT_EQ(expired.size(), 1u);
    EXPECT_EQ(expired[0], sid(1));

    // Authenticated session should remain.
    EXPECT_EQ(gateway_->stats().totalConnections, 1u);
    EXPECT_EQ(gateway_->stats().authenticatedConnections, 1u);
}

// =============================================================================
// Idle session cleanup
// =============================================================================

TEST_F(GatewayIntegrationTest, IdleSessionCleanup) {
    auto token = registerAndLogin("idle_user");
    std::vector<uint8_t> payload(token.begin(), token.end());

    ASSERT_TRUE(gateway_->handleConnect(sid(1), "10.0.0.1").hasValue());
    ASSERT_TRUE(gateway_->handleMessage(
        sid(1), GatewayOpcode::Authenticate, payload).hasValue());

    // Wait for idle timeout (2 seconds + margin).
    std::this_thread::sleep_for(std::chrono::milliseconds(2200));

    auto idle = gateway_->cleanupIdleSessions();
    EXPECT_EQ(idle.size(), 1u);
    EXPECT_EQ(idle[0], sid(1));
    EXPECT_EQ(gateway_->stats().totalConnections, 0u);
}

// =============================================================================
// Server migration with message routing
// =============================================================================

TEST_F(GatewayIntegrationTest, MigrationDoesNotInterruptRouting) {
    auto token = registerAndLogin("migrating_user");
    std::vector<uint8_t> payload(token.begin(), token.end());

    ASSERT_TRUE(gateway_->handleConnect(sid(1), "10.0.0.1").hasValue());
    ASSERT_TRUE(gateway_->handleMessage(
        sid(1), GatewayOpcode::Authenticate, payload).hasValue());

    // Route a message before migration.
    auto r1 = gateway_->handleMessage(sid(1), 0x0150, {});
    ASSERT_TRUE(r1.hasValue());
    EXPECT_EQ(r1.value().targetService, "game");

    // Initiate migration.
    ASSERT_TRUE(
        gateway_->initiateServerTransfer(sid(1), "game-server-2").hasValue());

    // During migration, auth-required routes should fail because the
    // session state is Migrating, not Authenticated.
    auto r2 = gateway_->handleMessage(sid(1), 0x0150, {});
    EXPECT_TRUE(r2.hasError());
    EXPECT_EQ(r2.error().code(), ErrorCode::ClientNotAuthenticated);

    // Complete migration.
    auto ack = gateway_->handleMessage(
        sid(1), GatewayOpcode::MigrationAck, {});
    ASSERT_TRUE(ack.hasValue());

    // After migration completes, routing works again.
    auto r3 = gateway_->handleMessage(sid(1), 0x0250, {});
    ASSERT_TRUE(r3.hasValue());
    EXPECT_EQ(r3.value().targetService, "lobby");
}

// =============================================================================
// Benchmark: message throughput
// =============================================================================

TEST_F(GatewayIntegrationTest, MessageRoutingThroughput) {
    // Use a dedicated gateway with high rate limit for benchmarking.
    auto benchAuth = authServer_;
    GatewayConfig benchConfig;
    benchConfig.maxConnections = 1000;
    benchConfig.rateLimitCapacity = 10000;
    benchConfig.rateLimitRefillRate = 10000;
    benchConfig.authTimeout = std::chrono::seconds{60};
    benchConfig.idleTimeout = std::chrono::seconds{300};

    GatewayServer benchGw(std::move(benchConfig), benchAuth);
    benchGw.addRoute(0x0100, 0x01FF, "game");
    ASSERT_TRUE(benchGw.start().hasValue());

    auto token = registerAndLogin("bench_user");
    std::vector<uint8_t> payload(token.begin(), token.end());

    ASSERT_TRUE(benchGw.handleConnect(sid(1), "10.0.0.1").hasValue());
    ASSERT_TRUE(benchGw.handleMessage(
        sid(1), GatewayOpcode::Authenticate, payload).hasValue());

    constexpr int kMessages = 1000;
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < kMessages; ++i) {
        auto result = benchGw.handleMessage(sid(1), 0x0150, {0x01});
        ASSERT_TRUE(result.hasValue());
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);

    EXPECT_EQ(benchGw.stats().messagesRouted,
              static_cast<uint64_t>(kMessages));

    if (ms.count() > 0) {
        auto throughput = (kMessages * 1000) / ms.count();
        std::cout << "[BENCHMARK] Gateway routed " << kMessages
                  << " messages in " << ms.count() << "ms ("
                  << throughput << " msg/sec)" << std::endl;
    }
}
