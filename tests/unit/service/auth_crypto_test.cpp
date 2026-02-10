#include <gtest/gtest.h>

#include "cgs/service/auth_types.hpp"
#include "cgs/service/password_hasher.hpp"
#include "cgs/service/token_provider.hpp"

#include <chrono>
#include <set>
#include <string>
#include <thread>

using namespace cgs::service;

// =============================================================================
// AuthTypes tests
// =============================================================================

TEST(AuthTypesTest, DefaultAuthConfig) {
    AuthConfig config;
    EXPECT_EQ(config.accessTokenExpiry, std::chrono::seconds{900});
    EXPECT_EQ(config.refreshTokenExpiry, std::chrono::seconds{604800});
    EXPECT_EQ(config.minPasswordLength, 8u);
    EXPECT_EQ(config.rateLimitMaxAttempts, 5u);
    EXPECT_EQ(config.rateLimitWindow, std::chrono::seconds{60});
}

TEST(AuthTypesTest, DefaultUserRecord) {
    UserRecord record;
    EXPECT_EQ(record.id, 0u);
    EXPECT_TRUE(record.username.empty());
    EXPECT_TRUE(record.email.empty());
    EXPECT_TRUE(record.passwordHash.empty());
    EXPECT_TRUE(record.salt.empty());
    EXPECT_EQ(record.status, UserStatus::Active);
    EXPECT_TRUE(record.roles.empty());
}

TEST(AuthTypesTest, DefaultTokenPair) {
    TokenPair pair;
    EXPECT_TRUE(pair.accessToken.empty());
    EXPECT_TRUE(pair.refreshToken.empty());
    EXPECT_EQ(pair.accessExpiresIn, std::chrono::seconds{0});
    EXPECT_EQ(pair.refreshExpiresIn, std::chrono::seconds{0});
}

TEST(AuthTypesTest, DefaultRefreshTokenRecord) {
    RefreshTokenRecord record;
    EXPECT_TRUE(record.token.empty());
    EXPECT_EQ(record.userId, 0u);
    EXPECT_FALSE(record.revoked);
}

// =============================================================================
// PasswordHasher tests (SRS-SVC-001.6)
// =============================================================================

class PasswordHasherTest : public ::testing::Test {
protected:
    PasswordHasher hasher;
};

TEST_F(PasswordHasherTest, HashProducesSaltAndHash) {
    auto result = hasher.hash("password123");
    EXPECT_FALSE(result.hash.empty());
    EXPECT_FALSE(result.salt.empty());
    // SHA-256 hex output is 64 characters.
    EXPECT_EQ(result.hash.size(), 64u);
    // Salt is 16 bytes → 32 hex chars.
    EXPECT_EQ(result.salt.size(), 32u);
}

TEST_F(PasswordHasherTest, SamePasswordDifferentSalts) {
    auto r1 = hasher.hash("password123");
    auto r2 = hasher.hash("password123");

    // Salts should differ (random).
    EXPECT_NE(r1.salt, r2.salt);
    // Hashes should differ (different salts).
    EXPECT_NE(r1.hash, r2.hash);
}

TEST_F(PasswordHasherTest, VerifyCorrectPassword) {
    auto result = hasher.hash("my_secret_pass");
    EXPECT_TRUE(hasher.verify("my_secret_pass", result.hash, result.salt));
}

TEST_F(PasswordHasherTest, VerifyWrongPassword) {
    auto result = hasher.hash("correct_password");
    EXPECT_FALSE(hasher.verify("wrong_password", result.hash, result.salt));
}

TEST_F(PasswordHasherTest, VerifyWrongSalt) {
    auto result = hasher.hash("password123");
    auto otherSalt = PasswordHasher::generateSalt();
    EXPECT_FALSE(hasher.verify("password123", result.hash, otherSalt));
}

TEST_F(PasswordHasherTest, VerifyEmptyPassword) {
    auto result = hasher.hash("");
    EXPECT_TRUE(hasher.verify("", result.hash, result.salt));
    EXPECT_FALSE(hasher.verify("not_empty", result.hash, result.salt));
}

TEST_F(PasswordHasherTest, DeterministicWithSameSalt) {
    auto salt = PasswordHasher::generateSalt();
    auto hash1 = hasher.hashWithSalt("test_password", salt);
    auto hash2 = hasher.hashWithSalt("test_password", salt);
    EXPECT_EQ(hash1, hash2);
}

TEST_F(PasswordHasherTest, PasswordNotStoredInPlaintext) {
    auto result = hasher.hash("plaintext_password");
    // Hash should not contain the original password.
    EXPECT_EQ(result.hash.find("plaintext_password"), std::string::npos);
    EXPECT_EQ(result.salt.find("plaintext_password"), std::string::npos);
}

TEST_F(PasswordHasherTest, GenerateSaltUniqueness) {
    std::set<std::string> salts;
    for (int i = 0; i < 100; ++i) {
        salts.insert(PasswordHasher::generateSalt());
    }
    // All 100 salts should be unique.
    EXPECT_EQ(salts.size(), 100u);
}

// =============================================================================
// TokenProvider tests (SRS-SVC-001.3, SRS-SVC-001.4)
// =============================================================================

class TokenProviderTest : public ::testing::Test {
protected:
    static constexpr auto kSigningKey =
        "test-signing-key-must-be-at-least-32-bytes!!";
    TokenProvider provider{kSigningKey};

    TokenClaims makeTestClaims() {
        TokenClaims claims;
        claims.subject = "user-123";
        claims.username = "testuser";
        claims.roles = {"player", "admin"};
        return claims;
    }
};

TEST_F(TokenProviderTest, GenerateAccessToken) {
    auto claims = makeTestClaims();
    auto token = provider.generateAccessToken(claims, std::chrono::seconds{900});

    EXPECT_FALSE(token.empty());
    // JWT has 3 parts separated by dots.
    int dotCount = 0;
    for (char c : token) {
        if (c == '.') { ++dotCount; }
    }
    EXPECT_EQ(dotCount, 2);
}

TEST_F(TokenProviderTest, ValidateAccessToken) {
    auto claims = makeTestClaims();
    auto token = provider.generateAccessToken(claims, std::chrono::seconds{900});

    auto result = provider.validateAccessToken(token);
    ASSERT_TRUE(result.hasValue());

    auto& decoded = result.value();
    EXPECT_EQ(decoded.subject, "user-123");
    EXPECT_EQ(decoded.username, "testuser");
    ASSERT_EQ(decoded.roles.size(), 2u);
    EXPECT_EQ(decoded.roles[0], "player");
    EXPECT_EQ(decoded.roles[1], "admin");
}

TEST_F(TokenProviderTest, ValidateExpiredToken) {
    auto claims = makeTestClaims();
    // Token that expires in 0 seconds (immediately expired).
    auto token = provider.generateAccessToken(claims, std::chrono::seconds{0});

    // Wait a moment to ensure expiry.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto result = provider.validateAccessToken(token);
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(),
              cgs::foundation::ErrorCode::TokenExpired);
}

TEST_F(TokenProviderTest, ValidateTamperedToken) {
    auto claims = makeTestClaims();
    auto token = provider.generateAccessToken(claims, std::chrono::seconds{900});

    // Tamper with the payload (change a character).
    auto tampered = token;
    auto firstDot = tampered.find('.');
    auto secondDot = tampered.find('.', firstDot + 1);
    if (secondDot > firstDot + 2) {
        tampered[firstDot + 1] =
            (tampered[firstDot + 1] == 'A') ? 'B' : 'A';
    }

    auto result = provider.validateAccessToken(tampered);
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(),
              cgs::foundation::ErrorCode::InvalidToken);
}

TEST_F(TokenProviderTest, ValidateMalformedToken) {
    auto result = provider.validateAccessToken("not.a.valid.jwt.token");
    ASSERT_TRUE(result.hasError());

    auto result2 = provider.validateAccessToken("no-dots-at-all");
    ASSERT_TRUE(result2.hasError());

    auto result3 = provider.validateAccessToken("only.one");
    ASSERT_TRUE(result3.hasError());
}

TEST_F(TokenProviderTest, ValidateWrongSigningKey) {
    auto claims = makeTestClaims();
    auto token = provider.generateAccessToken(claims, std::chrono::seconds{900});

    // Validate with a different key.
    TokenProvider otherProvider("different-signing-key-also-32-bytes!!");
    auto result = otherProvider.validateAccessToken(token);
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(),
              cgs::foundation::ErrorCode::InvalidToken);
}

TEST_F(TokenProviderTest, GenerateRefreshTokenIsRandom) {
    std::set<std::string> tokens;
    for (int i = 0; i < 100; ++i) {
        tokens.insert(TokenProvider::generateRefreshToken());
    }
    EXPECT_EQ(tokens.size(), 100u);
}

TEST_F(TokenProviderTest, RefreshTokenLength) {
    auto token = TokenProvider::generateRefreshToken();
    // 32 bytes → 64 hex chars.
    EXPECT_EQ(token.size(), 64u);
}

TEST_F(TokenProviderTest, TokenClaimsTimestamps) {
    auto claims = makeTestClaims();
    auto beforeGen = std::chrono::system_clock::now();
    auto token = provider.generateAccessToken(claims, std::chrono::seconds{300});
    auto afterGen = std::chrono::system_clock::now();

    auto result = provider.validateAccessToken(token);
    ASSERT_TRUE(result.hasValue());

    auto& decoded = result.value();

    // iat should be approximately now.
    auto iatDiff = std::chrono::duration_cast<std::chrono::seconds>(
                       decoded.issuedAt - beforeGen)
                       .count();
    EXPECT_GE(iatDiff, -1);
    EXPECT_LE(iatDiff, 2);

    // exp should be approximately now + 300s.
    auto expDiff = std::chrono::duration_cast<std::chrono::seconds>(
                       decoded.expiresAt - afterGen)
                       .count();
    EXPECT_GE(expDiff, 298);
    EXPECT_LE(expDiff, 302);
}

TEST_F(TokenProviderTest, EmptyRoles) {
    TokenClaims claims;
    claims.subject = "user-456";
    claims.username = "noroles";

    auto token = provider.generateAccessToken(claims, std::chrono::seconds{60});
    auto result = provider.validateAccessToken(token);
    ASSERT_TRUE(result.hasValue());
    EXPECT_TRUE(result.value().roles.empty());
}

TEST_F(TokenProviderTest, SpecialCharactersInClaims) {
    TokenClaims claims;
    claims.subject = "user with spaces";
    claims.username = "user@domain.com";
    claims.roles = {"role\"with\"quotes"};

    auto token = provider.generateAccessToken(claims, std::chrono::seconds{60});
    auto result = provider.validateAccessToken(token);
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().subject, "user with spaces");
    EXPECT_EQ(result.value().username, "user@domain.com");
}

TEST_F(TokenProviderTest, EmptyTokenString) {
    auto result = provider.validateAccessToken("");
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(),
              cgs::foundation::ErrorCode::InvalidToken);
}
