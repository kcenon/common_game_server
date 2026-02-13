/// @file auth_server_integration_test.cpp
/// @brief Integration tests for AuthServer: multi-device sessions, token
///        lifecycle, and cross-user isolation.
///
/// @see SRS-SVC-001

#include <gtest/gtest.h>

#include "cgs/foundation/error_code.hpp"
#include "cgs/service/auth_server.hpp"
#include "cgs/service/auth_types.hpp"
#include "cgs/service/token_store.hpp"
#include "cgs/service/user_repository.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

using namespace cgs::service;
using cgs::foundation::ErrorCode;

// =============================================================================
// Integration fixture: AuthServer full lifecycle
// =============================================================================

class AuthServerIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        userRepo_ = std::make_shared<InMemoryUserRepository>();
        tokenStore_ = std::make_shared<InMemoryTokenStore>();

        AuthConfig config;
        config.signingKey = "integration-test-key-at-least-32-bytes-long!";
        config.accessTokenExpiry = std::chrono::seconds{600};
        config.refreshTokenExpiry = std::chrono::seconds{3600};
        config.minPasswordLength = 8;
        config.rateLimitMaxAttempts = 1000;
        config.rateLimitWindow = std::chrono::seconds{60};

        server_ = std::make_unique<AuthServer>(
            std::move(config), userRepo_, tokenStore_);
    }

    /// Register a user and return the record id.
    uint64_t registerUser(const std::string& username,
                          const std::string& email,
                          const std::string& password) {
        auto result = server_->registerUser({username, email, password});
        EXPECT_TRUE(result.hasValue());
        return result.hasValue() ? result.value().id : 0;
    }

    /// Login and return the full TokenPair.
    TokenPair loginUser(const std::string& username,
                        const std::string& password,
                        const std::string& ip = "127.0.0.1") {
        auto result = server_->login(username, password, ip);
        EXPECT_TRUE(result.hasValue());
        return result.hasValue() ? std::move(result.value()) : TokenPair{};
    }

    std::shared_ptr<InMemoryUserRepository> userRepo_;
    std::shared_ptr<InMemoryTokenStore> tokenStore_;
    std::unique_ptr<AuthServer> server_;
};

// =============================================================================
// Multi-device session management (SRS-SVC-001.5)
// =============================================================================

TEST_F(AuthServerIntegrationTest, MultiDeviceLogoutRevokesAllSessions) {
    registerUser("alice", "alice@example.com", "AlicePass1!");

    // Simulate 5 concurrent device sessions.
    constexpr std::size_t kDevices = 5;
    std::vector<TokenPair> sessions;
    sessions.reserve(kDevices);
    for (std::size_t i = 0; i < kDevices; ++i) {
        sessions.push_back(
            loginUser("alice", "AlicePass1!", "10.0.0." + std::to_string(i)));
    }

    // All access tokens should be valid.
    for (const auto& s : sessions) {
        auto v = server_->validateToken(s.accessToken);
        EXPECT_TRUE(v.hasValue()) << "session access token should be valid";
    }

    // Logout from the first device.
    auto logoutResult = server_->logout(sessions[0].refreshToken);
    ASSERT_TRUE(logoutResult.hasValue());

    // ALL refresh tokens should be revoked (all-device logout).
    for (std::size_t i = 0; i < kDevices; ++i) {
        auto r = server_->refreshToken(sessions[i].refreshToken);
        EXPECT_TRUE(r.hasError())
            << "device " << i << " refresh token should be revoked";
        EXPECT_EQ(r.error().code(), ErrorCode::TokenRevoked);
    }
}

// =============================================================================
// Cross-user isolation during logout
// =============================================================================

TEST_F(AuthServerIntegrationTest, LogoutIsolationAcrossUsers) {
    registerUser("alice", "alice@example.com", "AlicePass1!");
    registerUser("bob", "bob@example.com", "BobPass123!");
    registerUser("carol", "carol@example.com", "CarolPw1!");

    auto aliceSession = loginUser("alice", "AlicePass1!", "10.0.0.1");
    auto bobSession = loginUser("bob", "BobPass123!", "10.0.0.2");
    auto carolSession = loginUser("carol", "CarolPw1!", "10.0.0.3");

    // Alice logs out.
    auto logoutResult = server_->logout(aliceSession.refreshToken);
    ASSERT_TRUE(logoutResult.hasValue());

    // Alice's token is revoked.
    auto aliceRefresh = server_->refreshToken(aliceSession.refreshToken);
    EXPECT_TRUE(aliceRefresh.hasError());
    EXPECT_EQ(aliceRefresh.error().code(), ErrorCode::TokenRevoked);

    // Bob and Carol remain unaffected.
    auto bobRefresh = server_->refreshToken(bobSession.refreshToken);
    EXPECT_TRUE(bobRefresh.hasValue()) << "bob should not be affected";

    auto carolRefresh = server_->refreshToken(carolSession.refreshToken);
    EXPECT_TRUE(carolRefresh.hasValue()) << "carol should not be affected";
}

// =============================================================================
// Full lifecycle: register → multi-login → refresh → revoke → logout
// =============================================================================

TEST_F(AuthServerIntegrationTest, FullTokenLifecycle) {
    // 1. Register.
    registerUser("dave", "dave@example.com", "DavePass1!");

    // 2. Login from two devices.
    auto device1 = loginUser("dave", "DavePass1!", "10.0.0.1");
    auto device2 = loginUser("dave", "DavePass1!", "10.0.0.2");

    // 3. Refresh device1's token.
    auto refreshed = server_->refreshToken(device1.refreshToken);
    ASSERT_TRUE(refreshed.hasValue());

    // 4. Old refresh token (device1) is rotated — can't reuse.
    auto reuse = server_->refreshToken(device1.refreshToken);
    EXPECT_TRUE(reuse.hasError());

    // 5. New refreshed token and device2 are still valid.
    auto v1 = server_->validateToken(refreshed.value().accessToken);
    auto v2 = server_->validateToken(device2.accessToken);
    EXPECT_TRUE(v1.hasValue());
    EXPECT_TRUE(v2.hasValue());

    // 6. Revoke the new access token from device1.
    auto revokeResult =
        server_->revokeAccessToken(refreshed.value().accessToken);
    ASSERT_TRUE(revokeResult.hasValue());

    // 7. Revoked access token fails validation.
    auto revokedCheck =
        server_->validateToken(refreshed.value().accessToken);
    EXPECT_TRUE(revokedCheck.hasError());
    EXPECT_EQ(revokedCheck.error().code(), ErrorCode::TokenRevoked);

    // 8. Device2's access token is still valid.
    auto v3 = server_->validateToken(device2.accessToken);
    EXPECT_TRUE(v3.hasValue());

    // 9. Logout from device2 — all remaining sessions revoked.
    auto logoutResult = server_->logout(device2.refreshToken);
    ASSERT_TRUE(logoutResult.hasValue());

    // 10. Neither device can refresh.
    auto r1 = server_->refreshToken(refreshed.value().refreshToken);
    auto r2 = server_->refreshToken(device2.refreshToken);
    EXPECT_TRUE(r1.hasError());
    EXPECT_TRUE(r2.hasError());
}

// =============================================================================
// Concurrent registrations — unique IDs
// =============================================================================

TEST_F(AuthServerIntegrationTest, BulkRegistrationUniqueIds) {
    constexpr std::size_t kUsers = 20;
    std::vector<uint64_t> ids;
    ids.reserve(kUsers);

    for (std::size_t i = 0; i < kUsers; ++i) {
        auto name = "user" + std::to_string(i);
        auto email = name + "@test.com";
        ids.push_back(registerUser(name, email, "BulkPass1!"));
    }

    // All IDs should be unique.
    std::sort(ids.begin(), ids.end());
    auto dup = std::adjacent_find(ids.begin(), ids.end());
    EXPECT_EQ(dup, ids.end()) << "all user IDs should be unique";
}

// =============================================================================
// Re-login after logout
// =============================================================================

TEST_F(AuthServerIntegrationTest, ReLoginAfterLogout) {
    registerUser("eve", "eve@example.com", "EvePass12!");

    // First session.
    auto firstSession = loginUser("eve", "EvePass12!", "10.0.0.1");

    // Logout.
    auto logoutResult = server_->logout(firstSession.refreshToken);
    ASSERT_TRUE(logoutResult.hasValue());

    // Re-login should succeed.
    auto secondSession = loginUser("eve", "EvePass12!", "10.0.0.2");
    EXPECT_FALSE(secondSession.accessToken.empty());
    EXPECT_FALSE(secondSession.refreshToken.empty());

    // New session tokens should work.
    auto v = server_->validateToken(secondSession.accessToken);
    EXPECT_TRUE(v.hasValue());
    EXPECT_EQ(v.value().username, "eve");

    auto r = server_->refreshToken(secondSession.refreshToken);
    EXPECT_TRUE(r.hasValue());
}
