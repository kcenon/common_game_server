/// @file main.cpp
/// @brief Lobby service entry point.
///
/// Standalone executable for the lobby server.
/// Manages matchmaking queues, party groups, and ELO rating.

#include <cstdlib>
#include <iostream>

#include "cgs/foundation/config_manager.hpp"
#include "cgs/foundation/game_metrics.hpp"
#include "cgs/service/health_server.hpp"
#include "cgs/service/lobby_server.hpp"
#include "cgs/service/lobby_types.hpp"
#include "cgs/service/service_runner.hpp"

namespace {

cgs::service::LobbyConfig buildLobbyConfig(
    const cgs::foundation::ConfigManager& config) {
    cgs::service::LobbyConfig cfg;

    auto minPlayers =
        config.get<unsigned int>("lobby.queue.min_players_per_match");
    if (minPlayers) {
        cfg.queueConfig.minPlayersPerMatch = minPlayers.value();
    }

    auto maxPlayers =
        config.get<unsigned int>("lobby.queue.max_players_per_match");
    if (maxPlayers) {
        cfg.queueConfig.maxPlayersPerMatch = maxPlayers.value();
    }

    auto initTol = config.get<int>("lobby.queue.initial_rating_tolerance");
    if (initTol) {
        cfg.queueConfig.initialRatingTolerance = initTol.value();
    }

    auto maxTol = config.get<int>("lobby.queue.max_rating_tolerance");
    if (maxTol) {
        cfg.queueConfig.maxRatingTolerance = maxTol.value();
    }

    auto expStep = config.get<int>("lobby.queue.expansion_step");
    if (expStep) {
        cfg.queueConfig.expansionStep = expStep.value();
    }

    auto expInterval = config.get<int>("lobby.queue.expansion_interval_seconds");
    if (expInterval) {
        cfg.queueConfig.expansionInterval =
            std::chrono::seconds(expInterval.value());
    }

    auto maxQueue = config.get<unsigned int>("lobby.queue.max_queue_size");
    if (maxQueue) {
        cfg.queueConfig.maxQueueSize = maxQueue.value();
    }

    auto maxParty = config.get<unsigned int>("lobby.max_party_size");
    if (maxParty) {
        cfg.maxPartySize = maxParty.value();
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
        {.port = 9102, .serviceName = "lobby"}, metrics);

    auto healthResult = health.start();
    if (!healthResult) {
        std::cerr << "Failed to start health server: "
                  << healthResult.error().message() << "\n";
        return EXIT_FAILURE;
    }

    auto lobbyCfg = buildLobbyConfig(config);
    cgs::service::LobbyServer server(lobbyCfg);

    auto startResult = server.start();
    if (!startResult) {
        std::cerr << "Failed to start lobby server: "
                  << startResult.error().message() << "\n";
        return EXIT_FAILURE;
    }

    health.setReady(true);

    std::cout << "Lobby server started (max_party: " << lobbyCfg.maxPartySize
              << ", max_queue: " << lobbyCfg.queueConfig.maxQueueSize
              << ", health_port: 9102)\n";

    signals.waitForShutdown();

    std::cout << "Shutting down lobby server...\n";
    health.setReady(false);
    server.stop();
    health.stop();
    std::cout << "Lobby server stopped\n";
    return EXIT_SUCCESS;
}
