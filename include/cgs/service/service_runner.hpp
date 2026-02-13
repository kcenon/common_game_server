#pragma once

/// @file service_runner.hpp
/// @brief Shared utilities for service entry points.
///
/// Provides signal handling, configuration loading, graceful shutdown
/// coordination, and CLI argument parsing for all CGS service executables.

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string_view>
#include <vector>

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

/// Shutdown hook callback type.
///
/// Each hook receives a name for logging and a callable.
/// Hooks are executed in registration order during graceful shutdown.
using ShutdownHook = std::function<void()>;

/// Coordinates graceful shutdown: drains connections, saves state, exits.
///
/// Services register shutdown hooks that are executed in order when
/// a shutdown signal is received. This ensures:
///   1. New connections are refused (readiness → false)
///   2. In-flight requests drain within a timeout
///   3. Persistent state is saved (snapshots, WAL flush)
///   4. Resources are released cleanly
///
/// Usage:
/// @code
///   GracefulShutdown shutdown;
///   shutdown.addHook("health", [&]() { health.setReady(false); });
///   shutdown.addHook("drain",  [&]() { server.drainConnections(); });
///   shutdown.addHook("persist",[&]() { persistence.stop(); });
///   shutdown.addHook("stop",   [&]() { server.stop(); });
///
///   // On signal:
///   shutdown.execute();
/// @endcode
class GracefulShutdown {
public:
    /// Add a named shutdown hook.
    ///
    /// Hooks execute in registration order during shutdown.
    void addHook(std::string name, ShutdownHook hook);

    /// Execute all registered hooks in order.
    ///
    /// Each hook is given up to the configured timeout.
    /// Errors in one hook do not prevent subsequent hooks from running.
    void execute();

    /// Get the number of registered hooks.
    [[nodiscard]] std::size_t hookCount() const;

    /// Set the maximum time to wait for all hooks to complete.
    void setDrainTimeout(std::chrono::seconds timeout);

private:
    struct Hook {
        std::string name;
        ShutdownHook callback;
    };
    std::vector<Hook> hooks_;
    std::chrono::seconds drainTimeout_{30};
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

/// Parse `--config <path>` from command-line arguments.
///
/// @return Config file path, or empty path if not specified.
[[nodiscard]] std::filesystem::path
parseConfigArg(int argc, char* argv[]);

} // namespace cgs::service
