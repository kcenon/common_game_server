#pragma once

/// @file health_server.hpp
/// @brief Lightweight HTTP health/metrics server for K8s probes.
///
/// Provides /healthz (liveness), /readyz (readiness), and /metrics
/// (Prometheus text format) endpoints over a minimal HTTP responder.
/// Designed for K8s httpGet probes and Prometheus scraping.
/// Part of SRS-NFR-011 (99.9% uptime monitoring).

#include "cgs/foundation/game_result.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace cgs::foundation {
class GameMetrics;
}

namespace cgs::service {

/// Configuration for the HealthServer.
struct HealthServerConfig {
    /// TCP port to listen on for health check and metrics requests.
    uint16_t port = 9100;

    /// Human-readable service name for JSON responses.
    std::string serviceName = "cgs";
};

/// Minimal HTTP server providing health check and Prometheus metrics endpoints.
///
/// Endpoints:
///   - GET /healthz → 200 OK (liveness probe)
///   - GET /readyz  → 200 OK if ready, 503 if not (readiness probe)
///   - GET /metrics → Prometheus text exposition format
///
/// Example:
/// @code
///   HealthServer health({.port = 9100, .serviceName = "auth"});
///   health.start();
///   health.setReady(true);
///   // ... service runs ...
///   health.stop();
/// @endcode
///
/// Thread-safe: runs an internal background thread for accepting connections.
class HealthServer {
public:
    explicit HealthServer(HealthServerConfig config, cgs::foundation::GameMetrics& metrics);
    ~HealthServer();

    HealthServer(const HealthServer&) = delete;
    HealthServer& operator=(const HealthServer&) = delete;

    /// Start listening on the configured port.
    [[nodiscard]] cgs::foundation::GameResult<void> start();

    /// Stop the server and close the listening socket.
    void stop();

    /// Set the readiness state. When false, /readyz returns 503.
    void setReady(bool ready);

    /// Check if the server is currently running.
    [[nodiscard]] bool isRunning() const;

    /// Get the configured port.
    [[nodiscard]] uint16_t port() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace cgs::service
