#pragma once

/// @file auth_server.hpp
/// @brief Authentication server for registration, login, and token management.
///
/// Orchestrates UserRepository, TokenStore, TokenProvider, PasswordHasher,
/// RateLimiter, and TokenBlacklist into a complete authentication service.
///
/// @see SRS-SVC-001
/// @see SRS-NFR-014

#include "cgs/foundation/game_result.hpp"
#include "cgs/service/auth_types.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace cgs::service {

class IUserRepository;
class ITokenStore;
class TokenProvider;
class PasswordHasher;
class RateLimiter;
class TokenBlacklist;

/// Authentication server implementing user registration, login/logout,
/// JWT token issuance, refresh token management, access token revocation,
/// and token blacklisting.
///
/// Example:
/// @code
///   auto userRepo = std::make_shared<InMemoryUserRepository>();
///   auto tokenStore = std::make_shared<InMemoryTokenStore>();
///   AuthServer server(AuthConfig{}, userRepo, tokenStore);
///
///   auto reg = server.registerUser({"alice", "alice@example.com", "Pa$$w0rd!"});
///   auto tokens = server.login("alice", "Pa$$w0rd!", "127.0.0.1");
/// @endcode
class AuthServer {
public:
    /// Construct with configuration and storage backends.
    AuthServer(AuthConfig config,
               std::shared_ptr<IUserRepository> userRepo,
               std::shared_ptr<ITokenStore> tokenStore);

    ~AuthServer();

    AuthServer(const AuthServer&) = delete;
    AuthServer& operator=(const AuthServer&) = delete;
    AuthServer(AuthServer&&) noexcept;
    AuthServer& operator=(AuthServer&&) noexcept;

    /// Register a new user (SRS-SVC-001.1).
    [[nodiscard]] cgs::foundation::GameResult<UserRecord> registerUser(
        const UserCredentials& credentials);

    /// Authenticate and issue tokens (SRS-SVC-001.2, SRS-SVC-001.3).
    [[nodiscard]] cgs::foundation::GameResult<TokenPair> login(std::string_view username,
                                                               std::string_view password,
                                                               const std::string& clientIp);

    /// Refresh an access token using a refresh token (SRS-SVC-001.4).
    [[nodiscard]] cgs::foundation::GameResult<TokenPair> refreshToken(
        std::string_view refreshToken);

    /// Logout by revoking all refresh tokens for the user (SRS-SVC-001.5).
    ///
    /// Looks up the given refresh token to identify the user, then revokes
    /// every refresh token belonging to that user (all-device logout).
    [[nodiscard]] cgs::foundation::GameResult<void> logout(std::string_view refreshToken);

    /// Validate an access token and return decoded claims.
    /// Checks the token blacklist after signature verification.
    [[nodiscard]] cgs::foundation::GameResult<TokenClaims> validateToken(
        std::string_view accessToken) const;

    /// Revoke an access token by adding it to the blacklist (SRS-NFR-014).
    ///
    /// The token is decoded to extract the jti (JWT ID) and expiry,
    /// then added to the in-memory blacklist until its natural expiry.
    [[nodiscard]] cgs::foundation::GameResult<void> revokeAccessToken(std::string_view accessToken);

    /// Run blacklist cleanup to remove expired entries.
    /// Returns the number of entries removed.
    std::size_t cleanupBlacklist();

private:
    [[nodiscard]] bool isValidEmail(std::string_view email) const;
    [[nodiscard]] bool isValidUsername(std::string_view username) const;
    [[nodiscard]] bool isStrongPassword(std::string_view password) const;

    AuthConfig config_;
    std::shared_ptr<IUserRepository> userRepo_;
    std::shared_ptr<ITokenStore> tokenStore_;
    std::unique_ptr<TokenProvider> tokenProvider_;
    std::unique_ptr<PasswordHasher> passwordHasher_;
    std::unique_ptr<RateLimiter> rateLimiter_;
    std::unique_ptr<TokenBlacklist> blacklist_;
};

}  // namespace cgs::service
