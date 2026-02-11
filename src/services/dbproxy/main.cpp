/// @file main.cpp
/// @brief DBProxy service entry point.
///
/// Standalone executable for the database proxy server.
/// Provides connection pooling, query caching, and read replica routing.

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "cgs/foundation/config_manager.hpp"
#include "cgs/foundation/game_metrics.hpp"
#include "cgs/service/dbproxy_server.hpp"
#include "cgs/service/dbproxy_types.hpp"
#include "cgs/service/health_server.hpp"
#include "cgs/service/service_runner.hpp"

namespace {

cgs::service::DBEndpointConfig buildEndpointConfig(
    const cgs::foundation::ConfigManager& config,
    const std::string& prefix) {
    cgs::service::DBEndpointConfig ep;

    auto connStr = config.get<std::string>(prefix + ".connection_string");
    if (connStr) {
        ep.connectionString = std::move(connStr).value();
    }

    auto minConn = config.get<unsigned int>(prefix + ".min_connections");
    if (minConn) {
        ep.minConnections = minConn.value();
    }

    auto maxConn = config.get<unsigned int>(prefix + ".max_connections");
    if (maxConn) {
        ep.maxConnections = maxConn.value();
    }

    auto timeout = config.get<int>(prefix + ".connection_timeout_seconds");
    if (timeout) {
        ep.connectionTimeout = std::chrono::seconds(timeout.value());
    }

    return ep;
}

cgs::service::DBProxyConfig buildDBProxyConfig(
    const cgs::foundation::ConfigManager& config) {
    cgs::service::DBProxyConfig cfg;

    cfg.primary = buildEndpointConfig(config, "dbproxy.primary");

    // Check for numbered replicas: dbproxy.replicas.0, dbproxy.replicas.1, ...
    for (unsigned int i = 0; i < 8; ++i) {
        auto prefix = "dbproxy.replicas." + std::to_string(i);
        auto connStr = config.get<std::string>(prefix + ".connection_string");
        if (connStr) {
            cfg.replicas.push_back(buildEndpointConfig(config, prefix));
        } else {
            break;
        }
    }

    auto cacheEnabled = config.get<bool>("dbproxy.cache.enabled");
    if (cacheEnabled) {
        cfg.cache.enabled = cacheEnabled.value();
    }

    auto maxEntries = config.get<unsigned int>("dbproxy.cache.max_entries");
    if (maxEntries) {
        cfg.cache.maxEntries = static_cast<std::size_t>(maxEntries.value());
    }

    auto ttl = config.get<int>("dbproxy.cache.default_ttl_seconds");
    if (ttl) {
        cfg.cache.defaultTtl = std::chrono::seconds(ttl.value());
    }

    auto healthCheck = config.get<int>("dbproxy.health_check_interval_seconds");
    if (healthCheck) {
        cfg.healthCheckInterval = std::chrono::seconds(healthCheck.value());
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

    // Health server + metrics.
    auto& metrics = cgs::foundation::GameMetrics::instance();
    cgs::service::HealthServer health(
        {.port = 9103, .serviceName = "dbproxy"}, metrics);

    auto healthResult = health.start();
    if (!healthResult) {
        std::cerr << "Failed to start health server: "
                  << healthResult.error().message() << "\n";
        return EXIT_FAILURE;
    }

    auto dbCfg = buildDBProxyConfig(config);
    cgs::service::DBProxyServer server(dbCfg);

    auto startResult = server.start();
    if (!startResult) {
        std::cerr << "Failed to start dbproxy server: "
                  << startResult.error().message() << "\n";
        return EXIT_FAILURE;
    }

    health.setReady(true);

    std::cout << "DBProxy server started (primary: "
              << dbCfg.primary.connectionString
              << ", replicas: " << dbCfg.replicas.size()
              << ", health_port: 9103)\n";

    signals.waitForShutdown();

    std::cout << "Shutting down dbproxy server...\n";
    health.setReady(false);
    server.stop();
    health.stop();
    std::cout << "DBProxy server stopped\n";
    return EXIT_SUCCESS;
}
