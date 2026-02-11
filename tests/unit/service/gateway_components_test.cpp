#include <gtest/gtest.h>

#include "cgs/foundation/types.hpp"
#include "cgs/service/gateway_session_manager.hpp"
#include "cgs/service/gateway_types.hpp"
#include "cgs/service/route_table.hpp"
#include "cgs/service/token_bucket.hpp"

#include <chrono>
#include <string>
#include <thread>

using namespace cgs::service;
using cgs::foundation::SessionId;

// =============================================================================
// Gateway types tests
// =============================================================================

TEST(GatewayTypesTest, DefaultConfig) {
    GatewayConfig config;
    EXPECT_EQ(config.tcpPort, 8080);
    EXPECT_EQ(config.webSocketPort, 8081);
    EXPECT_EQ(config.authTimeout, std::chrono::seconds{10});
    EXPECT_EQ(config.rateLimitCapacity, 100u);
    EXPECT_EQ(config.rateLimitRefillRate, 50u);
    EXPECT_EQ(config.maxConnections, 10000u);
    EXPECT_EQ(config.idleTimeout, std::chrono::seconds{300});
}

TEST(GatewayTypesTest, ClientStateNames) {
    EXPECT_EQ(clientStateName(ClientState::Unauthenticated), "Unauthenticated");
    EXPECT_EQ(clientStateName(ClientState::Authenticated), "Authenticated");
    EXPECT_EQ(clientStateName(ClientState::Migrating), "Migrating");
    EXPECT_EQ(clientStateName(ClientState::Disconnecting), "Disconnecting");
}

TEST(GatewayTypesTest, GatewayOpcodes) {
    EXPECT_EQ(GatewayOpcode::Authenticate, 0x0001);
    EXPECT_EQ(GatewayOpcode::AuthResult, 0x0002);
    EXPECT_EQ(GatewayOpcode::ServerTransfer, 0x0010);
    EXPECT_EQ(GatewayOpcode::MigrationAck, 0x0011);
    EXPECT_EQ(GatewayOpcode::Ping, 0x00FE);
    EXPECT_EQ(GatewayOpcode::Pong, 0x00FF);
}

TEST(GatewayTypesTest, DefaultClientSession) {
    ClientSession session;
    EXPECT_EQ(session.state, ClientState::Unauthenticated);
    EXPECT_EQ(session.userId, 0u);
    EXPECT_TRUE(session.remoteAddress.empty());
    EXPECT_TRUE(session.currentService.empty());
}

// =============================================================================
// Token bucket tests
// =============================================================================

class TokenBucketTest : public ::testing::Test {
protected:
    // 10 burst, 5 tokens/sec refill
    TokenBucket bucket{10, 5};
};

TEST_F(TokenBucketTest, InitialCapacityAvailable) {
    EXPECT_EQ(bucket.available("key1"), 10u);
}

TEST_F(TokenBucketTest, ConsumeReducesTokens) {
    EXPECT_TRUE(bucket.consume("key1"));
    EXPECT_EQ(bucket.available("key1"), 9u);
}

TEST_F(TokenBucketTest, ConsumeMultipleTokens) {
    EXPECT_TRUE(bucket.consume("key1", 5));
    // Available will be ~5 (minus tiny refill offset)
    EXPECT_LE(bucket.available("key1"), 5u);
}

TEST_F(TokenBucketTest, ExhaustBucket) {
    EXPECT_TRUE(bucket.consume("key1", 10));
    EXPECT_FALSE(bucket.consume("key1"));
}

TEST_F(TokenBucketTest, RefillOverTime) {
    EXPECT_TRUE(bucket.consume("key1", 10));
    EXPECT_FALSE(bucket.consume("key1"));

    // Wait 200ms for ~1 token refill (5 tokens/sec * 0.2s = 1 token)
    std::this_thread::sleep_for(std::chrono::milliseconds(220));

    EXPECT_TRUE(bucket.consume("key1"));
}

TEST_F(TokenBucketTest, CapacityCap) {
    // Wait to ensure refill doesn't exceed capacity
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(bucket.available("key1"), 10u);
}

TEST_F(TokenBucketTest, IndependentKeys) {
    EXPECT_TRUE(bucket.consume("key1", 10));
    EXPECT_FALSE(bucket.consume("key1"));

    // Different key should have full capacity.
    EXPECT_TRUE(bucket.consume("key2", 10));
}

TEST_F(TokenBucketTest, RemoveKey) {
    EXPECT_TRUE(bucket.consume("key1", 10));
    bucket.remove("key1");

    // After removal, key gets fresh bucket with full capacity.
    EXPECT_EQ(bucket.available("key1"), 10u);
}

TEST_F(TokenBucketTest, ResetKey) {
    EXPECT_TRUE(bucket.consume("key1", 8));
    bucket.reset("key1");
    EXPECT_EQ(bucket.available("key1"), 10u);
}

TEST_F(TokenBucketTest, ConsumeMoreThanAvailable) {
    EXPECT_FALSE(bucket.consume("key1", 11));
    // Tokens should not be consumed on failure.
    EXPECT_EQ(bucket.available("key1"), 10u);
}

// =============================================================================
// Route table tests
// =============================================================================

class RouteTableTest : public ::testing::Test {
protected:
    RouteTable routes;

    void SetUp() override {
        routes.addRoute(0x0100, 0x01FF, "game");
        routes.addRoute(0x0200, 0x02FF, "lobby");
        routes.addRoute(0x0300, 0x03FF, "chat", false);
    }
};

TEST_F(RouteTableTest, ResolveGameOpcode) {
    auto match = routes.resolve(0x0150);
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->service, "game");
    EXPECT_TRUE(match->requiresAuth);
}

TEST_F(RouteTableTest, ResolveLobbyOpcode) {
    auto match = routes.resolve(0x0200);
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->service, "lobby");
}

TEST_F(RouteTableTest, ResolveChatNoAuth) {
    auto match = routes.resolve(0x0350);
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->service, "chat");
    EXPECT_FALSE(match->requiresAuth);
}

TEST_F(RouteTableTest, ResolveUnknownOpcode) {
    auto match = routes.resolve(0x0500);
    EXPECT_FALSE(match.has_value());
}

TEST_F(RouteTableTest, ResolveBoundaryOpcodes) {
    // Min boundary
    auto matchMin = routes.resolve(0x0100);
    ASSERT_TRUE(matchMin.has_value());
    EXPECT_EQ(matchMin->service, "game");

    // Max boundary
    auto matchMax = routes.resolve(0x01FF);
    ASSERT_TRUE(matchMax.has_value());
    EXPECT_EQ(matchMax->service, "game");
}

TEST_F(RouteTableTest, IsGatewayOpcode) {
    EXPECT_TRUE(RouteTable::isGatewayOpcode(0x0000));
    EXPECT_TRUE(RouteTable::isGatewayOpcode(0x0001));
    EXPECT_TRUE(RouteTable::isGatewayOpcode(0x00FF));
    EXPECT_FALSE(RouteTable::isGatewayOpcode(0x0100));
    EXPECT_FALSE(RouteTable::isGatewayOpcode(0xFFFF));
}

TEST_F(RouteTableTest, GetRoutes) {
    EXPECT_EQ(routes.routes().size(), 3u);
}

TEST_F(RouteTableTest, RemoveRoutesForService) {
    routes.removeRoutesForService("game");
    EXPECT_EQ(routes.routes().size(), 2u);
    EXPECT_FALSE(routes.resolve(0x0150).has_value());
    EXPECT_TRUE(routes.resolve(0x0250).has_value());
}

TEST_F(RouteTableTest, Clear) {
    routes.clear();
    EXPECT_EQ(routes.routes().size(), 0u);
    EXPECT_FALSE(routes.resolve(0x0150).has_value());
}

TEST_F(RouteTableTest, AddRouteEntry) {
    RouteEntry entry;
    entry.opcodeMin = 0x0400;
    entry.opcodeMax = 0x04FF;
    entry.service = "inventory";
    entry.requiresAuth = true;
    routes.addRoute(std::move(entry));

    auto match = routes.resolve(0x0450);
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->service, "inventory");
}

// =============================================================================
// Gateway session manager tests
// =============================================================================

class GatewaySessionManagerTest : public ::testing::Test {
protected:
    GatewaySessionManager mgr{100};

    static SessionId sid(uint64_t id) {
        return SessionId(id);
    }

    static TokenClaims testClaims(const std::string& username) {
        TokenClaims claims;
        claims.subject = "user-1";
        claims.username = username;
        claims.roles = {"player"};
        return claims;
    }
};

TEST_F(GatewaySessionManagerTest, CreateSession) {
    EXPECT_TRUE(mgr.createSession(sid(1), "10.0.0.1"));
    EXPECT_EQ(mgr.sessionCount(), 1u);

    auto session = mgr.getSession(sid(1));
    ASSERT_TRUE(session.has_value());
    EXPECT_EQ(session->state, ClientState::Unauthenticated);
    EXPECT_EQ(session->remoteAddress, "10.0.0.1");
}

TEST_F(GatewaySessionManagerTest, CreateDuplicateSession) {
    EXPECT_TRUE(mgr.createSession(sid(1), "10.0.0.1"));
    EXPECT_FALSE(mgr.createSession(sid(1), "10.0.0.2"));
    EXPECT_EQ(mgr.sessionCount(), 1u);
}

TEST_F(GatewaySessionManagerTest, CreateSessionCapacityLimit) {
    GatewaySessionManager small(2);
    EXPECT_TRUE(small.createSession(sid(1), "10.0.0.1"));
    EXPECT_TRUE(small.createSession(sid(2), "10.0.0.2"));
    EXPECT_FALSE(small.createSession(sid(3), "10.0.0.3"));
}

TEST_F(GatewaySessionManagerTest, AuthenticateSession) {
    EXPECT_TRUE(mgr.createSession(sid(1), "10.0.0.1"));

    auto claims = testClaims("alice");
    EXPECT_TRUE(mgr.authenticateSession(sid(1), claims, 42));

    auto session = mgr.getSession(sid(1));
    ASSERT_TRUE(session.has_value());
    EXPECT_EQ(session->state, ClientState::Authenticated);
    EXPECT_EQ(session->userId, 42u);
    EXPECT_EQ(session->claims.username, "alice");
}

TEST_F(GatewaySessionManagerTest, AuthenticateNonexistentSession) {
    EXPECT_FALSE(mgr.authenticateSession(sid(99), testClaims("bob"), 1));
}

TEST_F(GatewaySessionManagerTest, AuthenticateAlreadyAuthenticated) {
    EXPECT_TRUE(mgr.createSession(sid(1), "10.0.0.1"));
    EXPECT_TRUE(mgr.authenticateSession(sid(1), testClaims("alice"), 1));

    // Cannot authenticate again.
    EXPECT_FALSE(mgr.authenticateSession(sid(1), testClaims("bob"), 2));
}

TEST_F(GatewaySessionManagerTest, MigrationLifecycle) {
    EXPECT_TRUE(mgr.createSession(sid(1), "10.0.0.1"));
    EXPECT_TRUE(mgr.authenticateSession(sid(1), testClaims("alice"), 1));

    // Begin migration.
    EXPECT_TRUE(mgr.beginMigration(sid(1), "game-server-2"));
    auto session = mgr.getSession(sid(1));
    ASSERT_TRUE(session.has_value());
    EXPECT_EQ(session->state, ClientState::Migrating);
    EXPECT_EQ(session->currentService, "game-server-2");

    // Complete migration.
    EXPECT_TRUE(mgr.completeMigration(sid(1)));
    session = mgr.getSession(sid(1));
    EXPECT_EQ(session->state, ClientState::Authenticated);
}

TEST_F(GatewaySessionManagerTest, MigrationRequiresAuthenticated) {
    EXPECT_TRUE(mgr.createSession(sid(1), "10.0.0.1"));
    // Cannot migrate unauthenticated session.
    EXPECT_FALSE(mgr.beginMigration(sid(1), "game-server-2"));
}

TEST_F(GatewaySessionManagerTest, CompleteMigrationRequiresMigrating) {
    EXPECT_TRUE(mgr.createSession(sid(1), "10.0.0.1"));
    EXPECT_TRUE(mgr.authenticateSession(sid(1), testClaims("alice"), 1));
    // Cannot complete migration when not migrating.
    EXPECT_FALSE(mgr.completeMigration(sid(1)));
}

TEST_F(GatewaySessionManagerTest, RemoveSession) {
    EXPECT_TRUE(mgr.createSession(sid(1), "10.0.0.1"));
    EXPECT_EQ(mgr.sessionCount(), 1u);

    mgr.removeSession(sid(1));
    EXPECT_EQ(mgr.sessionCount(), 0u);
    EXPECT_FALSE(mgr.getSession(sid(1)).has_value());
}

TEST_F(GatewaySessionManagerTest, TouchSession) {
    EXPECT_TRUE(mgr.createSession(sid(1), "10.0.0.1"));

    auto before = mgr.getSession(sid(1))->lastActivity;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    mgr.touchSession(sid(1));
    auto after = mgr.getSession(sid(1))->lastActivity;

    EXPECT_GT(after, before);
}

TEST_F(GatewaySessionManagerTest, SetCurrentService) {
    EXPECT_TRUE(mgr.createSession(sid(1), "10.0.0.1"));
    EXPECT_TRUE(mgr.setCurrentService(sid(1), "game-1"));

    auto session = mgr.getSession(sid(1));
    EXPECT_EQ(session->currentService, "game-1");
}

TEST_F(GatewaySessionManagerTest, GetSessionsByState) {
    EXPECT_TRUE(mgr.createSession(sid(1), "10.0.0.1"));
    EXPECT_TRUE(mgr.createSession(sid(2), "10.0.0.2"));
    EXPECT_TRUE(mgr.authenticateSession(sid(2), testClaims("bob"), 2));

    auto unauth = mgr.getSessionsByState(ClientState::Unauthenticated);
    EXPECT_EQ(unauth.size(), 1u);
    EXPECT_EQ(unauth[0].sessionId, sid(1));

    auto auth = mgr.getSessionsByState(ClientState::Authenticated);
    EXPECT_EQ(auth.size(), 1u);
    EXPECT_EQ(auth[0].sessionId, sid(2));
}

TEST_F(GatewaySessionManagerTest, SessionCountByState) {
    EXPECT_TRUE(mgr.createSession(sid(1), "10.0.0.1"));
    EXPECT_TRUE(mgr.createSession(sid(2), "10.0.0.2"));
    EXPECT_TRUE(mgr.authenticateSession(sid(2), testClaims("bob"), 2));

    EXPECT_EQ(mgr.sessionCount(ClientState::Unauthenticated), 1u);
    EXPECT_EQ(mgr.sessionCount(ClientState::Authenticated), 1u);
    EXPECT_EQ(mgr.sessionCount(ClientState::Migrating), 0u);
}

TEST_F(GatewaySessionManagerTest, FindExpiredAuthSessions) {
    EXPECT_TRUE(mgr.createSession(sid(1), "10.0.0.1"));

    // With a very short timeout, the session should be expired.
    auto expired = mgr.findExpiredAuthSessions(std::chrono::seconds{0});
    EXPECT_EQ(expired.size(), 1u);
    EXPECT_EQ(expired[0], sid(1));

    // With a long timeout, nothing should be expired.
    auto notExpired = mgr.findExpiredAuthSessions(std::chrono::seconds{3600});
    EXPECT_TRUE(notExpired.empty());
}

TEST_F(GatewaySessionManagerTest, FindIdleSessions) {
    EXPECT_TRUE(mgr.createSession(sid(1), "10.0.0.1"));
    EXPECT_TRUE(mgr.authenticateSession(sid(1), testClaims("alice"), 1));

    // With 0s timeout, authenticated session should be idle.
    auto idle = mgr.findIdleSessions(std::chrono::seconds{0});
    EXPECT_EQ(idle.size(), 1u);

    // Touch it and check again with longer timeout.
    mgr.touchSession(sid(1));
    auto notIdle = mgr.findIdleSessions(std::chrono::seconds{3600});
    EXPECT_TRUE(notIdle.empty());
}

TEST_F(GatewaySessionManagerTest, FindIdleExcludesUnauthenticated) {
    EXPECT_TRUE(mgr.createSession(sid(1), "10.0.0.1"));
    // Unauthenticated sessions should not appear in idle list.
    auto idle = mgr.findIdleSessions(std::chrono::seconds{0});
    EXPECT_TRUE(idle.empty());
}

// =============================================================================
// Integration: session lifecycle with state transitions
// =============================================================================

TEST_F(GatewaySessionManagerTest, FullSessionLifecycle) {
    // 1. Client connects.
    EXPECT_TRUE(mgr.createSession(sid(1), "192.168.1.10"));
    EXPECT_EQ(mgr.sessionCount(), 1u);
    EXPECT_EQ(mgr.sessionCount(ClientState::Unauthenticated), 1u);

    // 2. Client authenticates.
    auto claims = testClaims("player1");
    EXPECT_TRUE(mgr.authenticateSession(sid(1), claims, 100));
    EXPECT_EQ(mgr.sessionCount(ClientState::Authenticated), 1u);
    EXPECT_EQ(mgr.sessionCount(ClientState::Unauthenticated), 0u);

    // 3. Client gets routed to game service.
    EXPECT_TRUE(mgr.setCurrentService(sid(1), "game-server-1"));

    // 4. Server transfer (migration).
    EXPECT_TRUE(mgr.beginMigration(sid(1), "game-server-2"));
    EXPECT_EQ(mgr.sessionCount(ClientState::Migrating), 1u);

    // 5. Migration completes.
    EXPECT_TRUE(mgr.completeMigration(sid(1)));
    auto session = mgr.getSession(sid(1));
    EXPECT_EQ(session->state, ClientState::Authenticated);
    EXPECT_EQ(session->currentService, "game-server-2");

    // 6. Client disconnects.
    mgr.removeSession(sid(1));
    EXPECT_EQ(mgr.sessionCount(), 0u);
}
