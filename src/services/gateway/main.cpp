/// @file main.cpp
/// @brief Gateway service entry point.
///
/// Standalone executable for the gateway server.
/// Creates an embedded AuthServer (in-memory backends) and starts
/// the GatewayServer for client connection management.

#include <cstdlib>
#include <iostream>
#include <memory>

#include "cgs/foundation/config_manager.hpp"
#include "cgs/service/auth_server.hpp"
#include "cgs/service/auth_types.hpp"
#include "cgs/service/gateway_server.hpp"
#include "cgs/service/gateway_types.hpp"
#include "cgs/service/service_runner.hpp"
#include "cgs/service/token_store.hpp"
#include "cgs/service/user_repository.hpp"

namespace {

cgs::service::AuthConfig buildAuthConfig(
    const cgs::foundation::ConfigManager& config) {
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

    return cfg;
}

cgs::service::GatewayConfig buildGatewayConfig(
    const cgs::foundation::ConfigManager& config) {
    cgs::service::GatewayConfig cfg;

    auto tcpPort = config.get<int>("gateway.tcp_port");
    if (tcpPort) {
        cfg.tcpPort = static_cast<uint16_t>(tcpPort.value());
    }

    auto wsPort = config.get<int>("gateway.websocket_port");
    if (wsPort) {
        cfg.webSocketPort = static_cast<uint16_t>(wsPort.value());
    }

    auto authTimeout = config.get<int>("gateway.auth_timeout_seconds");
    if (authTimeout) {
        cfg.authTimeout = std::chrono::seconds(authTimeout.value());
    }

    auto rlCapacity = config.get<unsigned int>("gateway.rate_limit_capacity");
    if (rlCapacity) {
        cfg.rateLimitCapacity = rlCapacity.value();
    }

    auto rlRefill = config.get<unsigned int>("gateway.rate_limit_refill_rate");
    if (rlRefill) {
        cfg.rateLimitRefillRate = rlRefill.value();
    }

    auto maxConn = config.get<unsigned int>("gateway.max_connections");
    if (maxConn) {
        cfg.maxConnections = maxConn.value();
    }

    auto idleTimeout = config.get<int>("gateway.idle_timeout_seconds");
    if (idleTimeout) {
        cfg.idleTimeout = std::chrono::seconds(idleTimeout.value());
    }

    return cfg;
}

} // namespace

int main(int argc, char* argv[]) {
    cgs::service::SignalHandler signals;

    auto configPath = cgs::service::parseConfigArg(argc, argv);
    if (configPath.empty()) {
        configPath = "/etc/cgs/config.yaml";
    }

    cgs::foundation::ConfigManager config;
    auto loadResult = cgs::service::loadConfig(config, configPath);
    if (!loadResult) {
        std::cerr << "Failed to load config: "
                  << loadResult.error().message() << "\n";
        return EXIT_FAILURE;
    }

    // Embedded auth server with in-memory storage.
    auto authConfig = buildAuthConfig(config);
    auto userRepo = std::make_shared<cgs::service::InMemoryUserRepository>();
    auto tokenStore = std::make_shared<cgs::service::InMemoryTokenStore>();
    auto authServer = std::make_shared<cgs::service::AuthServer>(
        authConfig, userRepo, tokenStore);

    auto gwConfig = buildGatewayConfig(config);
    cgs::service::GatewayServer gateway(gwConfig, authServer);

    auto startResult = gateway.start();
    if (!startResult) {
        std::cerr << "Failed to start gateway: "
                  << startResult.error().message() << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "Gateway server started (tcp:" << gwConfig.tcpPort
              << " ws:" << gwConfig.webSocketPort << ")\n";

    signals.waitForShutdown();

    std::cout << "Shutting down gateway...\n";
    gateway.stop();
    std::cout << "Gateway server stopped\n";
    return EXIT_SUCCESS;
}
