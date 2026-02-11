/// @file main.cpp
/// @brief Game service entry point.
///
/// Standalone executable for the game server.
/// Runs the ECS-based world simulation with a fixed-rate game loop.

#include <cstdlib>
#include <iostream>

#include "cgs/foundation/config_manager.hpp"
#include "cgs/service/game_server.hpp"
#include "cgs/service/service_runner.hpp"

namespace {

cgs::service::GameServerConfig buildGameConfig(
    const cgs::foundation::ConfigManager& config) {
    cgs::service::GameServerConfig cfg;

    auto tickRate = config.get<unsigned int>("game.tick_rate");
    if (tickRate) {
        cfg.tickRate = tickRate.value();
    }

    auto maxInstances = config.get<unsigned int>("game.max_instances");
    if (maxInstances) {
        cfg.maxInstances = maxInstances.value();
    }

    auto cellSize = config.get<float>("game.spatial_cell_size");
    if (cellSize) {
        cfg.spatialCellSize = cellSize.value();
    }

    auto aiTick = config.get<float>("game.ai_tick_interval");
    if (aiTick) {
        cfg.aiTickInterval = aiTick.value();
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

    auto gameCfg = buildGameConfig(config);
    cgs::service::GameServer server(gameCfg);

    auto startResult = server.start();
    if (!startResult) {
        std::cerr << "Failed to start game server: "
                  << startResult.error().message() << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "Game server started (tick_rate: " << gameCfg.tickRate
              << " Hz, max_instances: " << gameCfg.maxInstances << ")\n";

    signals.waitForShutdown();

    std::cout << "Shutting down game server...\n";
    server.stop();
    std::cout << "Game server stopped\n";
    return EXIT_SUCCESS;
}
