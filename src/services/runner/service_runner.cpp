/// @file service_runner.cpp
/// @brief Implementation of shared service entry-point utilities.

#include "cgs/service/service_runner.hpp"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <string_view>
#include <thread>

namespace cgs::service {

// -- SignalHandler -----------------------------------------------------------

std::atomic<bool> SignalHandler::shutdownFlag_{false};

void SignalHandler::handler(int /*signal*/) {
    // async-signal-safe: relaxed store on a lock-free atomic.
    shutdownFlag_.store(true, std::memory_order_relaxed);
}

SignalHandler::SignalHandler() {
    shutdownFlag_.store(false, std::memory_order_relaxed);
    std::signal(SIGINT, &SignalHandler::handler);
    std::signal(SIGTERM, &SignalHandler::handler);
}

SignalHandler::~SignalHandler() {
    // Restore default handlers so that a second signal terminates immediately.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

bool SignalHandler::shutdownRequested() const noexcept {
    return shutdownFlag_.load(std::memory_order_relaxed);
}

void SignalHandler::waitForShutdown() const {
    using namespace std::chrono_literals;
    while (!shutdownFlag_.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(100ms);
    }
}

// -- Config loading ----------------------------------------------------------

cgs::foundation::GameResult<void>
loadConfig(cgs::foundation::ConfigManager& config,
           const std::filesystem::path& defaultPath) {
    std::filesystem::path configPath = defaultPath;

    // Environment variable override for 12-factor compliance.
    const char* envPath = std::getenv("CGS_CONFIG_PATH");
    if (envPath != nullptr) {
        configPath = envPath;
    }

    return config.load(configPath);
}

// -- CLI argument parsing ----------------------------------------------------

std::filesystem::path parseConfigArg(int argc, char* argv[]) {
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string_view(argv[i]) == "--config") {  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            return argv[i + 1];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }
    }
    return {};
}

} // namespace cgs::service
