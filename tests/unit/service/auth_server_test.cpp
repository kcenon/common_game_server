#include <gtest/gtest.h>

#include "cgs/foundation/error_code.hpp"
#include "cgs/service/auth_server.hpp"
#include "cgs/service/auth_types.hpp"
#include "cgs/service/rate_limiter.hpp"
#include "cgs/service/token_store.hpp"
#include "cgs/service/user_repository.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <thread>

using namespace cgs::service;
using cgs::foundation::ErrorCode;

// =============================================================================
// Test fixture
// =============================================================================

class AuthServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        userRepo_ = std::make_shared<InMemoryUserRepository>();
        tokenStore_ = std::make_shared<InMemoryTokenStore>();

        AuthConfig config;
        config.signingKey = "test-key-must-be-at-least-32-bytes-long!!";
        config.accessTokenExpiry = std::chrono::seconds{300};
        config.refreshTokenExpiry = std::chrono::seconds{3600};
        config.minPasswordLength = 8;
        config.rateLimitMaxAttempts = 3;
        config.rateLimitWindow = std::chrono::seconds{60};

        server_ = std::make_unique<AuthServer>(
            std::move(config), userRepo_, tokenStore_);
    }

    static UserCredentials validCredentials() {
        return {"testuser", "test@example.com", "StrongPass1!"};
    }

    /// Register and assert success (avoids nodiscard warnings in setup).
    void registerTestUser() {
        auto result = server_->registerUser(validCredentials());
        ASSERT_TRUE(result.hasValue());
    }

    std::shared_ptr<InMemoryUserRepository> userRepo_;
    std::shared_ptr<InMemoryTokenStore> tokenStore_;
    std::unique_ptr<AuthServer> server_;
};

// =============================================================================
// Registration tests (SRS-SVC-001.1)
// =============================================================================

TEST_F(AuthServerTest, RegisterUserSuccess) {
    auto result = server_->registerUser(validCredentials());
    ASSERT_TRUE(result.hasValue());

    auto& user = result.value();
    EXPECT_EQ(user.username, "testuser");
    EXPECT_EQ(user.email, "test@example.com");
    EXPECT_EQ(user.status, UserStatus::Active);
    EXPECT_GT(user.id, 0u);
    EXPECT_FALSE(user.passwordHash.empty());
    EXPECT_FALSE(user.salt.empty());
}

TEST_F(AuthServerTest, RegisterAssignsDefaultRole) {
    auto result = server_->registerUser(validCredentials());
    ASSERT_TRUE(result.hasValue());
    ASSERT_EQ(result.value().roles.size(), 1u);
    EXPECT_EQ(result.value().roles[0], "player");
}

TEST_F(AuthServerTest, RegisterDuplicateUsername) {
    registerTestUser();

    UserCredentials dup{"testuser", "other@example.com", "AnotherPass1!"};
    auto result = server_->registerUser(dup);
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::UserAlreadyExists);
}

TEST_F(AuthServerTest, RegisterDuplicateEmail) {
    registerTestUser();

    UserCredentials dup{"otheruser", "test@example.com", "AnotherPass1!"};
    auto result = server_->registerUser(dup);
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::UserAlreadyExists);
}

TEST_F(AuthServerTest, RegisterInvalidEmail) {
    UserCredentials cred{"user1", "not-an-email", "StrongPass1!"};
    auto result = server_->registerUser(cred);
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidEmail);
}

TEST_F(AuthServerTest, RegisterEmailMissingAt) {
    UserCredentials cred{"user1", "noemail.com", "StrongPass1!"};
    auto result = server_->registerUser(cred);
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidEmail);
}

TEST_F(AuthServerTest, RegisterEmailMissingDomain) {
    UserCredentials cred{"user1", "user@", "StrongPass1!"};
    auto result = server_->registerUser(cred);
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidEmail);
}

TEST_F(AuthServerTest, RegisterWeakPassword) {
    UserCredentials cred{"user1", "user@example.com", "short"};
    auto result = server_->registerUser(cred);
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::WeakPassword);
}

TEST_F(AuthServerTest, RegisterPasswordExactlyMinLength) {
    UserCredentials cred{"user1", "user@example.com", "12345678"};
    auto result = server_->registerUser(cred);
    ASSERT_TRUE(result.hasValue());
}

TEST_F(AuthServerTest, PasswordNotStoredInPlaintext) {
    auto result = server_->registerUser(validCredentials());
    ASSERT_TRUE(result.hasValue());

    auto& user = result.value();
    EXPECT_EQ(user.passwordHash.find("StrongPass1!"), std::string::npos);
    EXPECT_EQ(user.salt.find("StrongPass1!"), std::string::npos);
}

// =============================================================================
// Login tests (SRS-SVC-001.2, SRS-SVC-001.3)
// =============================================================================

TEST_F(AuthServerTest, LoginSuccess) {
    registerTestUser();

    auto result = server_->login("testuser", "StrongPass1!", "127.0.0.1");
    ASSERT_TRUE(result.hasValue());

    auto& tokens = result.value();
    EXPECT_FALSE(tokens.accessToken.empty());
    EXPECT_FALSE(tokens.refreshToken.empty());
    EXPECT_EQ(tokens.accessExpiresIn, std::chrono::seconds{300});
    EXPECT_EQ(tokens.refreshExpiresIn, std::chrono::seconds{3600});
}

TEST_F(AuthServerTest, LoginWrongPassword) {
    registerTestUser();

    auto result = server_->login("testuser", "WrongPass!!", "127.0.0.1");
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidCredentials);
}

TEST_F(AuthServerTest, LoginNonexistentUser) {
    auto result = server_->login("nobody", "Password1!", "127.0.0.1");
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidCredentials);
}

TEST_F(AuthServerTest, LoginAccessTokenIsValidJwt) {
    registerTestUser();
    auto loginResult = server_->login("testuser", "StrongPass1!", "127.0.0.1");
    ASSERT_TRUE(loginResult.hasValue());

    auto validateResult =
        server_->validateToken(loginResult.value().accessToken);
    ASSERT_TRUE(validateResult.hasValue());

    auto& claims = validateResult.value();
    EXPECT_EQ(claims.username, "testuser");
    EXPECT_FALSE(claims.subject.empty());
}

TEST_F(AuthServerTest, LoginReturnsUniqueTokens) {
    registerTestUser();

    auto r1 = server_->login("testuser", "StrongPass1!", "10.0.0.1");
    auto r2 = server_->login("testuser", "StrongPass1!", "10.0.0.2");
    ASSERT_TRUE(r1.hasValue());
    ASSERT_TRUE(r2.hasValue());

    EXPECT_NE(r1.value().refreshToken, r2.value().refreshToken);
}

// =============================================================================
// Rate limiting tests
// =============================================================================

TEST_F(AuthServerTest, RateLimitAfterMaxAttempts) {
    registerTestUser();
    const std::string ip = "192.168.1.100";

    // Exhaust rate limit (3 failed attempts).
    for (int i = 0; i < 3; ++i) {
        (void)server_->login("testuser", "WrongPass!!", ip);
    }

    // Fourth attempt should be rate-limited even with correct password.
    auto result = server_->login("testuser", "StrongPass1!", ip);
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::RateLimitExceeded);
}

TEST_F(AuthServerTest, RateLimitPerIp) {
    registerTestUser();

    // Exhaust rate limit for one IP.
    for (int i = 0; i < 3; ++i) {
        (void)server_->login("testuser", "WrongPass!!", "192.168.1.1");
    }

    // Different IP should still work.
    auto result = server_->login("testuser", "StrongPass1!", "192.168.1.2");
    ASSERT_TRUE(result.hasValue());
}

TEST_F(AuthServerTest, RateLimitResetsOnSuccess) {
    registerTestUser();
    const std::string ip = "10.0.0.1";

    // Two failed attempts.
    (void)server_->login("testuser", "WrongPass!!", ip);
    (void)server_->login("testuser", "WrongPass!!", ip);

    // Successful login resets counter.
    auto success = server_->login("testuser", "StrongPass1!", ip);
    ASSERT_TRUE(success.hasValue());

    // Two more failed attempts should be allowed (counter was reset).
    (void)server_->login("testuser", "WrongPass!!", ip);
    (void)server_->login("testuser", "WrongPass!!", ip);

    // Third failed attempt after reset should still be allowed (limit is 3).
    auto result = server_->login("testuser", "WrongPass!!", ip);
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidCredentials);
}

// =============================================================================
// Token refresh tests (SRS-SVC-001.4)
// =============================================================================

TEST_F(AuthServerTest, RefreshTokenSuccess) {
    registerTestUser();
    auto loginResult = server_->login("testuser", "StrongPass1!", "127.0.0.1");
    ASSERT_TRUE(loginResult.hasValue());

    auto refreshResult =
        server_->refreshToken(loginResult.value().refreshToken);
    ASSERT_TRUE(refreshResult.hasValue());

    auto& newTokens = refreshResult.value();
    EXPECT_FALSE(newTokens.accessToken.empty());
    EXPECT_FALSE(newTokens.refreshToken.empty());

    // New tokens should be different from original.
    EXPECT_NE(newTokens.refreshToken, loginResult.value().refreshToken);
}

TEST_F(AuthServerTest, RefreshTokenRotation) {
    registerTestUser();
    auto loginResult = server_->login("testuser", "StrongPass1!", "127.0.0.1");
    ASSERT_TRUE(loginResult.hasValue());

    auto oldRefresh = loginResult.value().refreshToken;

    // Refresh once.
    auto refreshResult = server_->refreshToken(oldRefresh);
    ASSERT_TRUE(refreshResult.hasValue());

    // Old refresh token should now be revoked.
    auto reuse = server_->refreshToken(oldRefresh);
    ASSERT_TRUE(reuse.hasError());
    EXPECT_EQ(reuse.error().code(), ErrorCode::TokenRevoked);
}

TEST_F(AuthServerTest, RefreshInvalidToken) {
    auto result = server_->refreshToken("nonexistent-token");
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidToken);
}

TEST_F(AuthServerTest, RefreshNewAccessTokenIsValid) {
    registerTestUser();
    auto loginResult = server_->login("testuser", "StrongPass1!", "127.0.0.1");
    ASSERT_TRUE(loginResult.hasValue());

    auto refreshResult =
        server_->refreshToken(loginResult.value().refreshToken);
    ASSERT_TRUE(refreshResult.hasValue());

    // The new access token should be valid.
    auto validateResult =
        server_->validateToken(refreshResult.value().accessToken);
    ASSERT_TRUE(validateResult.hasValue());
    EXPECT_EQ(validateResult.value().username, "testuser");
}

// =============================================================================
// Logout tests (SRS-SVC-001.5)
// =============================================================================

TEST_F(AuthServerTest, LogoutSuccess) {
    registerTestUser();
    auto loginResult = server_->login("testuser", "StrongPass1!", "127.0.0.1");
    ASSERT_TRUE(loginResult.hasValue());

    auto logoutResult = server_->logout(loginResult.value().refreshToken);
    ASSERT_TRUE(logoutResult.hasValue());
}

TEST_F(AuthServerTest, LogoutRevokesRefreshToken) {
    registerTestUser();
    auto loginResult = server_->login("testuser", "StrongPass1!", "127.0.0.1");
    ASSERT_TRUE(loginResult.hasValue());

    (void)server_->logout(loginResult.value().refreshToken);

    // Cannot refresh with revoked token.
    auto refreshResult =
        server_->refreshToken(loginResult.value().refreshToken);
    ASSERT_TRUE(refreshResult.hasError());
    EXPECT_EQ(refreshResult.error().code(), ErrorCode::TokenRevoked);
}

TEST_F(AuthServerTest, LogoutNonexistentToken) {
    auto result = server_->logout("nonexistent-token");
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidToken);
}

// =============================================================================
// Token validation tests
// =============================================================================

TEST_F(AuthServerTest, ValidateValidToken) {
    registerTestUser();
    auto loginResult = server_->login("testuser", "StrongPass1!", "127.0.0.1");
    ASSERT_TRUE(loginResult.hasValue());

    auto result = server_->validateToken(loginResult.value().accessToken);
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().username, "testuser");
    EXPECT_FALSE(result.value().roles.empty());
}

TEST_F(AuthServerTest, ValidateInvalidToken) {
    auto result = server_->validateToken("invalid.jwt.token");
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidToken);
}

TEST_F(AuthServerTest, ValidateExpiredToken) {
    // Create server with very short token expiry.
    auto shortRepo = std::make_shared<InMemoryUserRepository>();
    auto shortStore = std::make_shared<InMemoryTokenStore>();
    AuthConfig shortConfig;
    shortConfig.signingKey = "test-key-must-be-at-least-32-bytes-long!!";
    shortConfig.accessTokenExpiry = std::chrono::seconds{0};
    AuthServer shortServer(std::move(shortConfig), shortRepo, shortStore);

    auto reg = shortServer.registerUser(validCredentials());
    ASSERT_TRUE(reg.hasValue());

    auto loginResult =
        shortServer.login("testuser", "StrongPass1!", "127.0.0.1");
    ASSERT_TRUE(loginResult.hasValue());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto result = shortServer.validateToken(loginResult.value().accessToken);
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::TokenExpired);
}

// =============================================================================
// User repository direct tests
// =============================================================================

TEST_F(AuthServerTest, UserRepositoryFindByIdAfterRegister) {
    auto reg = server_->registerUser(validCredentials());
    ASSERT_TRUE(reg.hasValue());

    auto found = userRepo_->findById(reg.value().id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->username, "testuser");
}

TEST_F(AuthServerTest, UserRepositoryFindByEmail) {
    registerTestUser();

    auto found = userRepo_->findByEmail("test@example.com");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->username, "testuser");
}

// =============================================================================
// Token store direct tests
// =============================================================================

TEST_F(AuthServerTest, TokenStoreRemoveExpired) {
    RefreshTokenRecord expired;
    expired.token = "expired-token";
    expired.userId = 1;
    expired.expiresAt = std::chrono::system_clock::now() - std::chrono::hours{1};
    expired.revoked = false;
    tokenStore_->store(std::move(expired));

    EXPECT_TRUE(tokenStore_->find("expired-token").has_value());

    tokenStore_->removeExpired();

    EXPECT_FALSE(tokenStore_->find("expired-token").has_value());
}

TEST_F(AuthServerTest, TokenStoreRevokeAllForUser) {
    registerTestUser();

    // Login multiple times to create multiple refresh tokens.
    auto r1 = server_->login("testuser", "StrongPass1!", "10.0.0.1");
    auto r2 = server_->login("testuser", "StrongPass1!", "10.0.0.2");
    ASSERT_TRUE(r1.hasValue());
    ASSERT_TRUE(r2.hasValue());

    // Get user ID.
    auto user = userRepo_->findByUsername("testuser");
    ASSERT_TRUE(user.has_value());

    tokenStore_->revokeAllForUser(user->id);

    // Both refresh tokens should now be revoked.
    auto check1 = tokenStore_->find(r1.value().refreshToken);
    auto check2 = tokenStore_->find(r2.value().refreshToken);
    ASSERT_TRUE(check1.has_value());
    ASSERT_TRUE(check2.has_value());
    EXPECT_TRUE(check1->revoked);
    EXPECT_TRUE(check2->revoked);
}

// =============================================================================
// Rate limiter direct tests
// =============================================================================

TEST(RateLimiterTest, AllowWithinLimit) {
    RateLimiter limiter(3, std::chrono::seconds{60});
    EXPECT_TRUE(limiter.allow("key1"));
    EXPECT_TRUE(limiter.allow("key1"));
    EXPECT_TRUE(limiter.allow("key1"));
    EXPECT_FALSE(limiter.allow("key1"));
}

TEST(RateLimiterTest, RemainingCount) {
    RateLimiter limiter(5, std::chrono::seconds{60});
    EXPECT_EQ(limiter.remaining("key1"), 5u);
    (void)limiter.allow("key1");
    EXPECT_EQ(limiter.remaining("key1"), 4u);
}

TEST(RateLimiterTest, ResetClearsAttempts) {
    RateLimiter limiter(2, std::chrono::seconds{60});
    (void)limiter.allow("key1");
    (void)limiter.allow("key1");
    EXPECT_FALSE(limiter.allow("key1"));

    limiter.reset("key1");
    EXPECT_TRUE(limiter.allow("key1"));
}

TEST(RateLimiterTest, IndependentKeys) {
    RateLimiter limiter(1, std::chrono::seconds{60});
    EXPECT_TRUE(limiter.allow("key1"));
    EXPECT_FALSE(limiter.allow("key1"));
    EXPECT_TRUE(limiter.allow("key2"));
}

// =============================================================================
// Full auth flow integration test
// =============================================================================

TEST_F(AuthServerTest, FullAuthFlow) {
    // 1. Register.
    auto reg = server_->registerUser(validCredentials());
    ASSERT_TRUE(reg.hasValue());

    // 2. Login.
    auto login = server_->login("testuser", "StrongPass1!", "127.0.0.1");
    ASSERT_TRUE(login.hasValue());

    // 3. Validate access token.
    auto validate = server_->validateToken(login.value().accessToken);
    ASSERT_TRUE(validate.hasValue());
    EXPECT_EQ(validate.value().username, "testuser");

    // 4. Refresh token.
    auto refresh = server_->refreshToken(login.value().refreshToken);
    ASSERT_TRUE(refresh.hasValue());

    // 5. Validate new access token.
    auto validateNew = server_->validateToken(refresh.value().accessToken);
    ASSERT_TRUE(validateNew.hasValue());
    EXPECT_EQ(validateNew.value().username, "testuser");

    // 6. Old refresh token should be revoked.
    auto reuseOld = server_->refreshToken(login.value().refreshToken);
    ASSERT_TRUE(reuseOld.hasError());

    // 7. Logout.
    auto logoutResult = server_->logout(refresh.value().refreshToken);
    ASSERT_TRUE(logoutResult.hasValue());

    // 8. Cannot refresh after logout.
    auto afterLogout = server_->refreshToken(refresh.value().refreshToken);
    ASSERT_TRUE(afterLogout.hasError());
}

// =============================================================================
// Multiple users integration test
// =============================================================================

TEST_F(AuthServerTest, MultipleUsersIsolation) {
    // Register two users.
    UserCredentials alice{"alice", "alice@example.com", "AlicePass1!"};
    UserCredentials bob{"bob", "bob@example.com", "BobPass123!"};

    auto regAlice = server_->registerUser(alice);
    auto regBob = server_->registerUser(bob);
    ASSERT_TRUE(regAlice.hasValue());
    ASSERT_TRUE(regBob.hasValue());
    EXPECT_NE(regAlice.value().id, regBob.value().id);

    // Login as Alice.
    auto loginAlice = server_->login("alice", "AlicePass1!", "10.0.0.1");
    ASSERT_TRUE(loginAlice.hasValue());

    // Login as Bob.
    auto loginBob = server_->login("bob", "BobPass123!", "10.0.0.2");
    ASSERT_TRUE(loginBob.hasValue());

    // Validate Alice's token contains Alice's claims.
    auto claimsAlice = server_->validateToken(loginAlice.value().accessToken);
    ASSERT_TRUE(claimsAlice.hasValue());
    EXPECT_EQ(claimsAlice.value().username, "alice");

    // Validate Bob's token contains Bob's claims.
    auto claimsBob = server_->validateToken(loginBob.value().accessToken);
    ASSERT_TRUE(claimsBob.hasValue());
    EXPECT_EQ(claimsBob.value().username, "bob");
}
