/// @file auth_server.cpp
/// @brief AuthServer implementation orchestrating the full auth workflow.
///
/// @see SRS-SVC-001
/// @see SRS-NFR-014

#include "cgs/service/auth_server.hpp"

#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_error.hpp"
#include "cgs/service/input_validator.hpp"
#include "cgs/service/password_hasher.hpp"
#include "cgs/service/rate_limiter.hpp"
#include "cgs/service/token_blacklist.hpp"
#include "cgs/service/token_provider.hpp"
#include "cgs/service/token_store.hpp"
#include "cgs/service/user_repository.hpp"

#include <algorithm>
#include <chrono>
#include <string>

namespace cgs::service {

using cgs::foundation::ErrorCode;
using cgs::foundation::GameError;

// -- Construction / destruction -----------------------------------------------

AuthServer::AuthServer(AuthConfig config,
                       std::shared_ptr<IUserRepository> userRepo,
                       std::shared_ptr<ITokenStore> tokenStore)
    : config_(std::move(config)),
      userRepo_(std::move(userRepo)),
      tokenStore_(std::move(tokenStore)),
      tokenProvider_(std::make_unique<TokenProvider>(config_)),
      passwordHasher_(std::make_unique<PasswordHasher>()),
      rateLimiter_(
          std::make_unique<RateLimiter>(config_.rateLimitMaxAttempts, config_.rateLimitWindow)),
      blacklist_(std::make_unique<TokenBlacklist>(config_.blacklistCleanupInterval)) {
    // Wire the blacklist into the token provider for validation checks.
    tokenProvider_->setBlacklist(blacklist_.get());
}

AuthServer::~AuthServer() = default;
AuthServer::AuthServer(AuthServer&&) noexcept = default;
AuthServer& AuthServer::operator=(AuthServer&&) noexcept = default;

// -- Registration (SRS-SVC-001.1) ---------------------------------------------

cgs::foundation::GameResult<UserRecord> AuthServer::registerUser(
    const UserCredentials& credentials) {
    // Validate username format.
    if (!isValidUsername(credentials.username)) {
        auto result = InputValidator::validateUsername(credentials.username);
        return cgs::foundation::GameResult<UserRecord>::err(
            GameError(ErrorCode::InvalidUsername, result.message));
    }

    // Validate email format.
    if (!isValidEmail(credentials.email)) {
        auto result = InputValidator::validateEmail(credentials.email);
        return cgs::foundation::GameResult<UserRecord>::err(
            GameError(ErrorCode::InvalidEmail, result.message));
    }

    // Validate password strength.
    if (!isStrongPassword(credentials.password)) {
        auto result =
            InputValidator::validatePassword(credentials.password, config_.minPasswordLength);
        return cgs::foundation::GameResult<UserRecord>::err(
            GameError(ErrorCode::WeakPassword, result.message));
    }

    // Check for duplicate username.
    if (userRepo_->findByUsername(credentials.username).has_value()) {
        return cgs::foundation::GameResult<UserRecord>::err(
            GameError(ErrorCode::UserAlreadyExists, "username already taken"));
    }

    // Check for duplicate email.
    if (userRepo_->findByEmail(credentials.email).has_value()) {
        return cgs::foundation::GameResult<UserRecord>::err(
            GameError(ErrorCode::UserAlreadyExists, "email already registered"));
    }

    // Hash password.
    auto hashed = passwordHasher_->hash(credentials.password);

    // Build user record.
    UserRecord record;
    record.username = credentials.username;
    record.email = credentials.email;
    record.passwordHash = std::move(hashed.hash);
    record.salt = std::move(hashed.salt);
    record.status = UserStatus::Active;
    record.roles = {"player"};

    // Persist and return.
    auto id = userRepo_->create(std::move(record));
    auto stored = userRepo_->findById(id);
    return cgs::foundation::GameResult<UserRecord>::ok(std::move(*stored));
}

// -- Login (SRS-SVC-001.2, SRS-SVC-001.3) ------------------------------------

cgs::foundation::GameResult<TokenPair> AuthServer::login(std::string_view username,
                                                         std::string_view password,
                                                         const std::string& clientIp) {
    // Rate limit check.
    if (!rateLimiter_->allow(clientIp)) {
        return cgs::foundation::GameResult<TokenPair>::err(
            GameError(ErrorCode::RateLimitExceeded, "too many login attempts, try again later"));
    }

    // Find user.
    auto userOpt = userRepo_->findByUsername(username);
    if (!userOpt.has_value()) {
        return cgs::foundation::GameResult<TokenPair>::err(
            GameError(ErrorCode::InvalidCredentials, "invalid username or password"));
    }

    auto& user = *userOpt;

    // Check account status.
    if (user.status != UserStatus::Active) {
        return cgs::foundation::GameResult<TokenPair>::err(
            GameError(ErrorCode::AuthenticationFailed, "account is not active"));
    }

    // Verify password.
    if (!passwordHasher_->verify(password, user.passwordHash, user.salt)) {
        return cgs::foundation::GameResult<TokenPair>::err(
            GameError(ErrorCode::InvalidCredentials, "invalid username or password"));
    }

    // Build claims.
    TokenClaims claims;
    claims.subject = std::to_string(user.id);
    claims.username = user.username;
    claims.roles = user.roles;

    // Generate access token.
    auto accessToken = tokenProvider_->generateAccessToken(claims, config_.accessTokenExpiry);

    // Generate and store refresh token.
    auto refreshTokenStr = TokenProvider::generateRefreshToken();
    RefreshTokenRecord refreshRecord;
    refreshRecord.token = refreshTokenStr;
    refreshRecord.userId = user.id;
    refreshRecord.expiresAt = std::chrono::system_clock::now() + config_.refreshTokenExpiry;
    refreshRecord.revoked = false;
    tokenStore_->store(std::move(refreshRecord));

    // Reset rate limiter on successful login.
    rateLimiter_->reset(clientIp);

    TokenPair pair;
    pair.accessToken = std::move(accessToken);
    pair.refreshToken = std::move(refreshTokenStr);
    pair.accessExpiresIn = config_.accessTokenExpiry;
    pair.refreshExpiresIn = config_.refreshTokenExpiry;
    return cgs::foundation::GameResult<TokenPair>::ok(std::move(pair));
}

// -- Token refresh (SRS-SVC-001.4) --------------------------------------------

cgs::foundation::GameResult<TokenPair> AuthServer::refreshToken(std::string_view refreshToken) {
    // Look up the refresh token.
    auto recordOpt = tokenStore_->find(refreshToken);
    if (!recordOpt.has_value()) {
        return cgs::foundation::GameResult<TokenPair>::err(
            GameError(ErrorCode::InvalidToken, "refresh token not found"));
    }

    auto& record = *recordOpt;

    // Check revocation.
    if (record.revoked) {
        return cgs::foundation::GameResult<TokenPair>::err(
            GameError(ErrorCode::TokenRevoked, "refresh token has been revoked"));
    }

    // Check expiry.
    if (std::chrono::system_clock::now() > record.expiresAt) {
        return cgs::foundation::GameResult<TokenPair>::err(
            GameError(ErrorCode::RefreshTokenExpired, "refresh token has expired"));
    }

    // Find the user.
    auto userOpt = userRepo_->findById(record.userId);
    if (!userOpt.has_value()) {
        return cgs::foundation::GameResult<TokenPair>::err(
            GameError(ErrorCode::AuthenticationFailed, "user not found for refresh token"));
    }

    auto& user = *userOpt;

    // Revoke the old refresh token (rotation).
    tokenStore_->revoke(refreshToken);

    // Build claims.
    TokenClaims claims;
    claims.subject = std::to_string(user.id);
    claims.username = user.username;
    claims.roles = user.roles;

    // Generate new token pair.
    auto newAccessToken = tokenProvider_->generateAccessToken(claims, config_.accessTokenExpiry);
    auto newRefreshTokenStr = TokenProvider::generateRefreshToken();

    RefreshTokenRecord newRefreshRecord;
    newRefreshRecord.token = newRefreshTokenStr;
    newRefreshRecord.userId = user.id;
    newRefreshRecord.expiresAt = std::chrono::system_clock::now() + config_.refreshTokenExpiry;
    newRefreshRecord.revoked = false;
    tokenStore_->store(std::move(newRefreshRecord));

    TokenPair pair;
    pair.accessToken = std::move(newAccessToken);
    pair.refreshToken = std::move(newRefreshTokenStr);
    pair.accessExpiresIn = config_.accessTokenExpiry;
    pair.refreshExpiresIn = config_.refreshTokenExpiry;
    return cgs::foundation::GameResult<TokenPair>::ok(std::move(pair));
}

// -- Logout (SRS-SVC-001.5) ---------------------------------------------------

cgs::foundation::GameResult<void> AuthServer::logout(std::string_view refreshToken) {
    // Look up the refresh token to identify the user.
    auto recordOpt = tokenStore_->find(refreshToken);
    if (!recordOpt.has_value()) {
        return cgs::foundation::GameResult<void>::err(
            GameError(ErrorCode::InvalidToken, "refresh token not found"));
    }

    // Revoke ALL refresh tokens belonging to this user (all-device logout).
    tokenStore_->revokeAllForUser(recordOpt->userId);
    return cgs::foundation::GameResult<void>::ok();
}

// -- Token validation ---------------------------------------------------------

cgs::foundation::GameResult<TokenClaims> AuthServer::validateToken(
    std::string_view accessToken) const {
    // TokenProvider::validateAccessToken already checks the blacklist.
    return tokenProvider_->validateAccessToken(accessToken);
}

// -- Access token revocation (SRS-NFR-014) ------------------------------------

cgs::foundation::GameResult<void> AuthServer::revokeAccessToken(std::string_view accessToken) {
    // Decode the token to extract jti and expiry (skip blacklist check
    // during decode since we're about to blacklist it anyway).
    auto parts = std::vector<std::string>{};
    {
        std::size_t start = 0;
        for (int i = 0; i < 2; ++i) {
            auto pos = accessToken.find('.', start);
            if (pos == std::string_view::npos) {
                return cgs::foundation::GameResult<void>::err(
                    GameError(ErrorCode::InvalidToken, "malformed JWT: expected 3 parts"));
            }
            parts.emplace_back(accessToken.substr(start, pos - start));
            start = pos + 1;
        }
        parts.emplace_back(accessToken.substr(start));
    }

    // Validate the token first (this also checks signature and expiry).
    auto result = tokenProvider_->validateAccessToken(accessToken);
    if (result.hasError()) {
        // If the token is already expired, that's fine — no need to blacklist.
        if (result.error().code() == ErrorCode::TokenExpired) {
            return cgs::foundation::GameResult<void>::ok();
        }
        // If already revoked, also fine.
        if (result.error().code() == ErrorCode::TokenRevoked) {
            return cgs::foundation::GameResult<void>::ok();
        }
        return cgs::foundation::GameResult<void>::err(result.error());
    }

    auto& claims = result.value();
    if (claims.jti.empty()) {
        return cgs::foundation::GameResult<void>::err(
            GameError(ErrorCode::InvalidToken, "token has no jti claim for revocation"));
    }

    blacklist_->revoke(claims.jti, claims.expiresAt);
    return cgs::foundation::GameResult<void>::ok();
}

// -- Blacklist maintenance ----------------------------------------------------

std::size_t AuthServer::cleanupBlacklist() {
    return blacklist_->cleanup();
}

// -- Validation helpers -------------------------------------------------------

bool AuthServer::isValidEmail(std::string_view email) const {
    return InputValidator::validateEmail(email).valid;
}

bool AuthServer::isValidUsername(std::string_view username) const {
    return InputValidator::validateUsername(username).valid;
}

bool AuthServer::isStrongPassword(std::string_view password) const {
    return InputValidator::validatePassword(password, config_.minPasswordLength).valid;
}

}  // namespace cgs::service
