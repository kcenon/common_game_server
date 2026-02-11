#pragma once

/// @file service_runner.hpp
/// @brief Shared utilities for service entry points.
///
/// Provides signal handling, configuration loading, and CLI argument
/// parsing for all CGS service executables.

#include <atomic>
#include <filesystem>
#include <string_view>

#include "cgs/foundation/config_manager.hpp"
#include "cgs/foundation/game_result.hpp"

namespace cgs::service {

/// Installs SIGINT and SIGTERM handlers and exposes a shutdown flag.
///
/// Only one SignalHandler instance should exist per process.
/// The handler writes to a static atomic flag in an async-signal-safe
/// manner (relaxed store on a lock-free atomic).
///
/// After shutdown is requested, the original default handlers are
/// restored so that a second signal terminates the process immediately.
class SignalHandler {
public:
    SignalHandler();
    ~SignalHandler();

    SignalHandler(const SignalHandler&) = delete;
    SignalHandler& operator=(const SignalHandler&) = delete;

    /// Returns true after SIGINT or SIGTERM is received.
    [[nodiscard]] bool shutdownRequested() const noexcept;

    /// Block the calling thread until a shutdown signal arrives.
    void waitForShutdown() const;

private:
    static std::atomic<bool> shutdownFlag_;
    static void handler(int signal);
};

/// Load a YAML configuration file into the provided ConfigManager.
///
/// The config file path is resolved in order:
///   1. CGS_CONFIG_PATH environment variable (if set)
///   2. @p defaultPath parameter
///
/// @param config      ConfigManager to populate.
/// @param defaultPath Fallback config file path.
/// @return Success or ConfigLoadFailed error.
[[nodiscard]] cgs::foundation::GameResult<void>
loadConfig(cgs::foundation::ConfigManager& config,
           const std::filesystem::path& defaultPath);

/// Parse --config <path> from command-line arguments.
///
/// @return Config file path, or empty path if not specified.
[[nodiscard]] std::filesystem::path
parseConfigArg(int argc, char* argv[]);

} // namespace cgs::service
