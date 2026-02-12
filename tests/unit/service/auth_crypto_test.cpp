#include <gtest/gtest.h>

#include "cgs/service/auth_types.hpp"
#include "cgs/service/password_hasher.hpp"
#include "cgs/service/token_blacklist.hpp"
#include "cgs/service/token_provider.hpp"

#include <chrono>
#include <future>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

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
    EXPECT_EQ(config.jwtAlgorithm, JwtAlgorithm::HS256);
    EXPECT_TRUE(config.rsaPrivateKeyPem.empty());
    EXPECT_TRUE(config.rsaPublicKeyPem.empty());
    EXPECT_EQ(config.blacklistCleanupInterval, std::chrono::seconds{300});
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

TEST(AuthTypesTest, TokenClaimsHasJti) {
    TokenClaims claims;
    EXPECT_TRUE(claims.jti.empty());
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
// TokenProvider tests — HS256 backward compatibility (SRS-SVC-001.3, SRS-SVC-001.4)
// =============================================================================

namespace {
AuthConfig makeHs256Config() {
    AuthConfig config;
    config.signingKey = "test-signing-key-must-be-at-least-32-bytes!!";
    config.jwtAlgorithm = JwtAlgorithm::HS256;
    return config;
}
} // namespace

class TokenProviderTest : public ::testing::Test {
protected:
    AuthConfig config_ = makeHs256Config();
    TokenProvider provider{config_};

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

TEST_F(TokenProviderTest, GeneratedTokenContainsJti) {
    auto claims = makeTestClaims();
    auto token = provider.generateAccessToken(claims, std::chrono::seconds{900});

    auto result = provider.validateAccessToken(token);
    ASSERT_TRUE(result.hasValue());
    EXPECT_FALSE(result.value().jti.empty());
    // jti is 16 random bytes = 32 hex chars.
    EXPECT_EQ(result.value().jti.size(), 32u);
}

TEST_F(TokenProviderTest, JtiIsUniquePerToken) {
    auto claims = makeTestClaims();
    std::set<std::string> jtis;
    for (int i = 0; i < 50; ++i) {
        auto token =
            provider.generateAccessToken(claims, std::chrono::seconds{900});
        auto result = provider.validateAccessToken(token);
        ASSERT_TRUE(result.hasValue());
        jtis.insert(result.value().jti);
    }
    EXPECT_EQ(jtis.size(), 50u);
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
    AuthConfig otherConfig;
    otherConfig.signingKey = "different-signing-key-also-32-bytes!!";
    TokenProvider otherProvider(otherConfig);
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

// =============================================================================
// TokenProvider tests — RS256 (SRS-NFR-014)
// =============================================================================

namespace {

/// Helper to extract a PEM string from an EVP_PKEY using a BIO.
std::string evpPkeyToPem(EVP_PKEY* pkey, bool isPrivate) {
    BIO* bio = BIO_new(BIO_s_mem());
    if (isPrivate) {
        PEM_write_bio_PrivateKey(bio, pkey, nullptr, nullptr, 0, nullptr,
                                 nullptr);
    } else {
        PEM_write_bio_PUBKEY(bio, pkey);
    }
    char* data = nullptr;
    auto len = BIO_get_mem_data(bio, &data);
    std::string pem(data, static_cast<std::size_t>(len));
    BIO_free(bio);
    return pem;
}

/// Generate a fresh 2048-bit RSA keypair at test runtime.
/// Returns {privateKeyPem, publicKeyPem}.
std::pair<std::string, std::string> generateTestRsaKeypair() {
    EVP_PKEY* pkey = EVP_RSA_gen(2048);
    auto privateKeyPem = evpPkeyToPem(pkey, true);
    auto publicKeyPem = evpPkeyToPem(pkey, false);
    EVP_PKEY_free(pkey);
    return {privateKeyPem, publicKeyPem};
}

/// Lazily-initialized test keypair (generated once per test process).
const std::pair<std::string, std::string>& testRsaKeypair() {
    static const auto kp = generateTestRsaKeypair();
    return kp;
}

AuthConfig makeRs256Config() {
    auto& [priv, pub] = testRsaKeypair();
    AuthConfig config;
    config.jwtAlgorithm = JwtAlgorithm::RS256;
    config.rsaPrivateKeyPem = priv;
    config.rsaPublicKeyPem = pub;
    return config;
}

} // namespace

class Rs256TokenProviderTest : public ::testing::Test {
protected:
    AuthConfig config_ = makeRs256Config();
    TokenProvider provider{config_};

    TokenClaims makeTestClaims() {
        TokenClaims claims;
        claims.subject = "user-rs256";
        claims.username = "rsauser";
        claims.roles = {"player"};
        return claims;
    }
};

TEST_F(Rs256TokenProviderTest, RS256SignAndVerify) {
    auto claims = makeTestClaims();
    auto token = provider.generateAccessToken(claims, std::chrono::seconds{900});

    EXPECT_FALSE(token.empty());

    auto result = provider.validateAccessToken(token);
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().subject, "user-rs256");
    EXPECT_EQ(result.value().username, "rsauser");
    ASSERT_EQ(result.value().roles.size(), 1u);
    EXPECT_EQ(result.value().roles[0], "player");
    EXPECT_FALSE(result.value().jti.empty());
}

TEST_F(Rs256TokenProviderTest, RS256RejectsTamperedToken) {
    auto claims = makeTestClaims();
    auto token = provider.generateAccessToken(claims, std::chrono::seconds{900});

    // Tamper with the payload.
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

TEST_F(Rs256TokenProviderTest, RS256RejectsHS256Token) {
    // Sign a token with HS256.
    auto hs256Config = makeHs256Config();
    TokenProvider hs256Provider(hs256Config);

    auto claims = makeTestClaims();
    auto hs256Token =
        hs256Provider.generateAccessToken(claims, std::chrono::seconds{900});

    // Create RS256-only verifier (no signing key set).
    auto& [priv, pub] = testRsaKeypair();
    AuthConfig rs256OnlyConfig;
    rs256OnlyConfig.jwtAlgorithm = JwtAlgorithm::RS256;
    rs256OnlyConfig.rsaPublicKeyPem = pub;
    // Clear signing key so HS256 verification would use empty key.
    rs256OnlyConfig.signingKey = "";
    TokenProvider rs256Verifier(rs256OnlyConfig);

    // The HS256 token should fail RS256 verification because the header says
    // "HS256" and HMAC with empty key won't match.
    auto result = rs256Verifier.validateAccessToken(hs256Token);
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(),
              cgs::foundation::ErrorCode::InvalidToken);
}

TEST_F(Rs256TokenProviderTest, RS256ExpiredToken) {
    auto claims = makeTestClaims();
    auto token = provider.generateAccessToken(claims, std::chrono::seconds{0});

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto result = provider.validateAccessToken(token);
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(),
              cgs::foundation::ErrorCode::TokenExpired);
}

// =============================================================================
// TokenBlacklist tests (SRS-NFR-014)
// =============================================================================

class TokenBlacklistTest : public ::testing::Test {
protected:
    TokenBlacklist blacklist{std::chrono::seconds{60}};
};

TEST_F(TokenBlacklistTest, RevokeAndCheck) {
    auto expiresAt = std::chrono::system_clock::now() + std::chrono::seconds{300};
    blacklist.revoke("jti-001", expiresAt);

    EXPECT_TRUE(blacklist.isRevoked("jti-001"));
    EXPECT_FALSE(blacklist.isRevoked("jti-002"));
    EXPECT_EQ(blacklist.size(), 1u);
}

TEST_F(TokenBlacklistTest, MultipleRevocations) {
    auto expiresAt = std::chrono::system_clock::now() + std::chrono::seconds{300};
    blacklist.revoke("jti-a", expiresAt);
    blacklist.revoke("jti-b", expiresAt);
    blacklist.revoke("jti-c", expiresAt);

    EXPECT_TRUE(blacklist.isRevoked("jti-a"));
    EXPECT_TRUE(blacklist.isRevoked("jti-b"));
    EXPECT_TRUE(blacklist.isRevoked("jti-c"));
    EXPECT_EQ(blacklist.size(), 3u);
}

TEST_F(TokenBlacklistTest, AutoCleanup) {
    // Revoke a token that's already expired.
    auto alreadyExpired =
        std::chrono::system_clock::now() - std::chrono::seconds{1};
    blacklist.revoke("expired-jti", alreadyExpired);

    // Revoke a token that's still valid.
    auto futureExpiry =
        std::chrono::system_clock::now() + std::chrono::seconds{300};
    blacklist.revoke("valid-jti", futureExpiry);

    EXPECT_EQ(blacklist.size(), 2u);

    // Cleanup should remove the expired entry.
    auto removed = blacklist.cleanup();
    EXPECT_EQ(removed, 1u);
    EXPECT_EQ(blacklist.size(), 1u);

    EXPECT_FALSE(blacklist.isRevoked("expired-jti"));
    EXPECT_TRUE(blacklist.isRevoked("valid-jti"));
}

TEST_F(TokenBlacklistTest, ThreadSafety) {
    constexpr int kWriterCount = 4;
    constexpr int kOpsPerWriter = 100;

    std::vector<std::future<void>> futures;
    auto expiresAt = std::chrono::system_clock::now() + std::chrono::seconds{300};

    // Writers: add entries concurrently.
    for (int w = 0; w < kWriterCount; ++w) {
        futures.push_back(std::async(std::launch::async, [&, w]() {
            for (int i = 0; i < kOpsPerWriter; ++i) {
                std::string jti = "w" + std::to_string(w) + "-" + std::to_string(i);
                blacklist.revoke(jti, expiresAt);
            }
        }));
    }

    // Readers: check entries concurrently with writers.
    for (int r = 0; r < kWriterCount; ++r) {
        futures.push_back(std::async(std::launch::async, [&]() {
            for (int i = 0; i < kOpsPerWriter; ++i) {
                // Just ensure no crash or data race.
                (void)blacklist.isRevoked("w0-" + std::to_string(i));
                (void)blacklist.size();
            }
        }));
    }

    for (auto& f : futures) {
        f.get();
    }

    EXPECT_EQ(blacklist.size(),
              static_cast<std::size_t>(kWriterCount * kOpsPerWriter));
}

// =============================================================================
// TokenProvider + Blacklist integration (SRS-NFR-014)
// =============================================================================

TEST(TokenProviderBlacklistTest, BlacklistedTokenIsRejected) {
    auto config = makeHs256Config();
    TokenProvider provider(config);
    TokenBlacklist blacklist(std::chrono::seconds{60});
    provider.setBlacklist(&blacklist);

    TokenClaims claims;
    claims.subject = "user-bl";
    claims.username = "blacklistuser";
    claims.roles = {"player"};

    auto token = provider.generateAccessToken(claims, std::chrono::seconds{900});

    // Decode to get jti.
    auto decoded = provider.validateAccessToken(token);
    ASSERT_TRUE(decoded.hasValue());
    auto jti = decoded.value().jti;
    EXPECT_FALSE(jti.empty());

    // Revoke.
    blacklist.revoke(jti, decoded.value().expiresAt);

    // Validation should now fail.
    auto result = provider.validateAccessToken(token);
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(),
              cgs::foundation::ErrorCode::TokenRevoked);
}

TEST(TokenProviderBlacklistTest, NonBlacklistedTokenPasses) {
    auto config = makeHs256Config();
    TokenProvider provider(config);
    TokenBlacklist blacklist(std::chrono::seconds{60});
    provider.setBlacklist(&blacklist);

    TokenClaims claims;
    claims.subject = "user-ok";
    claims.username = "okuser";

    auto token = provider.generateAccessToken(claims, std::chrono::seconds{900});

    // Don't revoke — should pass.
    auto result = provider.validateAccessToken(token);
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().subject, "user-ok");
}

TEST(TokenProviderBlacklistTest, BackwardCompatibleHS256) {
    // Default config uses HS256 — ensure everything still works.
    AuthConfig config;
    config.signingKey = "backward-compat-key-at-least-32-bytes!!!";
    TokenProvider provider(config);

    TokenClaims claims;
    claims.subject = "user-compat";
    claims.username = "compatuser";
    claims.roles = {"player"};

    auto token = provider.generateAccessToken(claims, std::chrono::seconds{60});
    auto result = provider.validateAccessToken(token);
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().subject, "user-compat");
    EXPECT_EQ(result.value().username, "compatuser");
    EXPECT_FALSE(result.value().jti.empty());
}
