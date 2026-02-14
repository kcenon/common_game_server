/// @file main.cpp
/// @brief Auth service entry point.
///
/// Standalone executable for the authentication service.
/// Creates an AuthServer with in-memory storage backends
/// suitable for development and testing.

#include "cgs/foundation/config_manager.hpp"
#include "cgs/foundation/game_metrics.hpp"
#include "cgs/service/auth_server.hpp"
#include "cgs/service/auth_types.hpp"
#include "cgs/service/health_server.hpp"
#include "cgs/service/service_runner.hpp"
#include "cgs/service/token_store.hpp"
#include "cgs/service/user_repository.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>

namespace {

cgs::service::AuthConfig buildAuthConfig(const cgs::foundation::ConfigManager& config) {
    cgs::service::AuthConfig cfg;

    auto signingKey = config.get<std::string>("auth.signing_key");
    if (signingKey) {
        cfg.signingKey = std::move(signingKey).value();
    }

    auto accessExpiry = config.get<int>("auth.access_token_expiry_seconds");
    if (accessExpiry) {
        cfg.accessTokenExpiry = std::chrono::seconds(accessExpiry.value());
    }

    auto refreshExpiry = config.get<int>("auth.refresh_token_expiry_seconds");
    if (refreshExpiry) {
        cfg.refreshTokenExpiry = std::chrono::seconds(refreshExpiry.value());
    }

    auto minPwLen = config.get<unsigned int>("auth.min_password_length");
    if (minPwLen) {
        cfg.minPasswordLength = minPwLen.value();
    }

    auto rateMax = config.get<unsigned int>("auth.rate_limit_max_attempts");
    if (rateMax) {
        cfg.rateLimitMaxAttempts = rateMax.value();
    }

    auto rateWindow = config.get<int>("auth.rate_limit_window_seconds");
    if (rateWindow) {
        cfg.rateLimitWindow = std::chrono::seconds(rateWindow.value());
    }

    // RS256 configuration (optional; falls back to HS256 if not set).
    auto jwtAlg = config.get<std::string>("auth.jwt_algorithm");
    if (jwtAlg && jwtAlg.value() == "RS256") {
        cfg.jwtAlgorithm = cgs::service::JwtAlgorithm::RS256;
    }

    auto rsaPrivateKey = config.get<std::string>("auth.rsa_private_key_pem");
    if (rsaPrivateKey) {
        cfg.rsaPrivateKeyPem = std::move(rsaPrivateKey).value();
    }

    auto rsaPublicKey = config.get<std::string>("auth.rsa_public_key_pem");
    if (rsaPublicKey) {
        cfg.rsaPublicKeyPem = std::move(rsaPublicKey).value();
    }

    auto blacklistInterval = config.get<int>("auth.blacklist_cleanup_interval_seconds");
    if (blacklistInterval) {
        cfg.blacklistCleanupInterval = std::chrono::seconds(blacklistInterval.value());
    }

    return cfg;
}

}  // namespace

int main(int argc, char* argv[]) {
    cgs::service::SignalHandler signals;

    // Resolve config path: --config flag > CGS_CONFIG_PATH env > default.
    auto configPath = cgs::service::parseConfigArg(argc, argv);
    if (configPath.empty()) {
        configPath = "/etc/cgs/config.yaml";
    }

    cgs::foundation::ConfigManager config;
    auto loadResult = cgs::service::loadConfig(config, configPath);
    if (!loadResult) {
        std::cerr << "Failed to load config: " << loadResult.error().message() << "\n";
        return EXIT_FAILURE;
    }

    // Health server + metrics.
    auto& metrics = cgs::foundation::GameMetrics::instance();
    cgs::service::HealthServer health({.port = 9101, .serviceName = "auth"}, metrics);

    auto healthResult = health.start();
    if (!healthResult) {
        std::cerr << "Failed to start health server: " << healthResult.error().message() << "\n";
        return EXIT_FAILURE;
    }

    auto authConfig = buildAuthConfig(config);

    // In-memory backends for standalone development mode.
    auto userRepo = std::make_shared<cgs::service::InMemoryUserRepository>();
    auto tokenStore = std::make_shared<cgs::service::InMemoryTokenStore>();

    cgs::service::AuthServer server(authConfig, userRepo, tokenStore);

    health.setReady(true);

    std::cout << "Auth service started (config: " << configPath.string()
              << ", health_port: 9101)\n";

    signals.waitForShutdown();

    health.setReady(false);
    health.stop();
    std::cout << "Auth service stopped\n";
    return EXIT_SUCCESS;
}
