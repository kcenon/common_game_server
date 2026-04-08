// examples/01_result_errors.cpp
//
// Tutorial: Result<T> and error handling
// See: docs/tutorial_result_errors.dox
//
// This example walks through the Result<T, E> pattern used throughout
// common_game_server to replace exceptions with explicit, type-safe error
// propagation. It implements a tiny configuration loader that:
//
//   1. Reads a "raw" string buffer (simulating a file read)
//   2. Parses it into a ServerConfig struct
//   3. Validates the parsed values
//
// Each stage returns GameResult<T>. A failure in any stage short-circuits
// the pipeline, and the caller prints a categorized error message with the
// subsystem name derived from the ErrorCode.

#include "cgs/core/result.hpp"
#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_error.hpp"
#include "cgs/foundation/game_result.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

using cgs::foundation::ErrorCode;
using cgs::foundation::errorSubsystem;
using cgs::foundation::GameError;
using cgs::foundation::GameResult;

// ─── Domain types ──────────────────────────────────────────────────────────

struct ServerConfig {
    std::string bindAddress;
    std::uint16_t port = 0;
    std::uint32_t maxConnections = 0;
};

// ─── Stage 1: "read" a configuration buffer ────────────────────────────────
//
// In a real server this would touch the filesystem or a config service.
// Here we simulate it with a string table so the example has zero runtime
// dependencies.

GameResult<std::string> ReadConfig(std::string_view name) {
    if (name == "server") {
        return GameResult<std::string>::ok(
            "bind=0.0.0.0\nport=8080\nmax_connections=10000\n");
    }
    if (name == "missing-port") {
        return GameResult<std::string>::ok("bind=127.0.0.1\nmax_connections=100\n");
    }
    return GameResult<std::string>::err(
        GameError(ErrorCode::ConfigLoadFailed,
                  "config source not found: " + std::string(name)));
}

// ─── Stage 2: parse key=value lines into ServerConfig ──────────────────────

GameResult<ServerConfig> ParseConfig(const std::string& raw) {
    ServerConfig cfg;
    std::size_t pos = 0;
    while (pos < raw.size()) {
        const auto lineEnd = raw.find('\n', pos);
        const auto line = raw.substr(pos, lineEnd - pos);
        pos = (lineEnd == std::string::npos) ? raw.size() : lineEnd + 1;
        if (line.empty()) {
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            return GameResult<ServerConfig>::err(
                GameError(ErrorCode::ConfigTypeMismatch,
                          "malformed config line: " + line));
        }
        const auto key = line.substr(0, eq);
        const auto value = line.substr(eq + 1);
        if (key == "bind") {
            cfg.bindAddress = value;
        } else if (key == "port") {
            cfg.port = static_cast<std::uint16_t>(std::stoi(value));
        } else if (key == "max_connections") {
            cfg.maxConnections = static_cast<std::uint32_t>(std::stoul(value));
        } else {
            return GameResult<ServerConfig>::err(
                GameError(ErrorCode::ConfigKeyNotFound, "unknown key: " + key));
        }
    }
    return GameResult<ServerConfig>::ok(cfg);
}

// ─── Stage 3: validate parsed values ───────────────────────────────────────

GameResult<ServerConfig> ValidateConfig(ServerConfig cfg) {
    if (cfg.bindAddress.empty()) {
        return GameResult<ServerConfig>::err(
            GameError(ErrorCode::InvalidArgument, "bind address is required"));
    }
    if (cfg.port == 0) {
        return GameResult<ServerConfig>::err(
            GameError(ErrorCode::InvalidArgument, "port must be non-zero"));
    }
    if (cfg.maxConnections == 0) {
        return GameResult<ServerConfig>::err(
            GameError(ErrorCode::InvalidArgument,
                      "max_connections must be non-zero"));
    }
    return GameResult<ServerConfig>::ok(std::move(cfg));
}

// ─── Orchestration: chain the 3 stages with manual propagation ─────────────
//
// Result<T> does NOT provide monadic combinators like and_then/map. Chaining
// is done by hand with early returns. The pattern is repetitive but
// completely explicit — every failure point is visible in the source.

GameResult<ServerConfig> LoadAndValidate(std::string_view name) {
    auto raw = ReadConfig(name);
    if (!raw) {
        return GameResult<ServerConfig>::err(raw.error());
    }

    auto parsed = ParseConfig(raw.value());
    if (!parsed) {
        return GameResult<ServerConfig>::err(parsed.error());
    }

    return ValidateConfig(std::move(parsed).value());
}

// ─── Reporting helper ──────────────────────────────────────────────────────

void Report(std::string_view label, const GameResult<ServerConfig>& result) {
    std::cout << "[" << label << "] ";
    if (result.hasValue()) {
        const auto& cfg = result.value();
        std::cout << "ok: " << cfg.bindAddress << ":" << cfg.port
                  << " (max " << cfg.maxConnections << " connections)\n";
    } else {
        const auto& err = result.error();
        std::cout << "error [" << errorSubsystem(err.code()) << "/"
                  << static_cast<std::uint32_t>(err.code())
                  << "]: " << err.message() << "\n";
    }
}

// ─── main ──────────────────────────────────────────────────────────────────

int main() {
    // Happy path: valid config loads, parses, validates.
    Report("server", LoadAndValidate("server"));

    // Missing-port path: ReadConfig succeeds, ParseConfig succeeds, but
    // ValidateConfig catches the zero port and short-circuits.
    Report("missing-port", LoadAndValidate("missing-port"));

    // Not-found path: ReadConfig fails first and ParseConfig is never called.
    Report("absent", LoadAndValidate("absent"));

    // Demonstrate valueOr — extract with a fallback. Use this when you want
    // a safe default instead of propagating the error.
    const auto fallback = GameResult<std::string>::err(
                              GameError(ErrorCode::NotFound, "no key"))
                              .valueOr("(default)");
    std::cout << "[fallback] " << fallback << "\n";

    return EXIT_SUCCESS;
}
