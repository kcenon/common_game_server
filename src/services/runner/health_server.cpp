/// @file health_server.cpp
/// @brief Minimal HTTP health/metrics server implementation.
///
/// Uses POSIX sockets for a single-threaded, poll-based HTTP responder.
/// Supports GET /healthz, /readyz, and /metrics with graceful shutdown.

#include "cgs/service/health_server.hpp"

#include "cgs/foundation/game_error.hpp"
#include "cgs/foundation/game_metrics.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

// POSIX socket headers
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace cgs::service {

namespace {

/// Format HealthCheckResult as JSON.
std::string healthToJson(const cgs::foundation::HealthCheckResult& result,
                         std::chrono::seconds uptime) {
    std::ostringstream out;
    out << R"({"status":")";

    switch (result.status) {
        case cgs::foundation::HealthStatus::Healthy:   out << "healthy"; break;
        case cgs::foundation::HealthStatus::Degraded:  out << "degraded"; break;
        case cgs::foundation::HealthStatus::Unhealthy: out << "unhealthy"; break;
    }

    out << R"(","service":")" << result.serviceName
        << R"(","uptime_seconds":)" << uptime.count();

    if (!result.components.empty()) {
        out << R"(,"components":{)";
        bool first = true;
        for (const auto& [name, status] : result.components) {
            if (!first) { out << ","; }
            first = false;
            out << R"(")" << name << R"(":)";
            switch (status) {
                case cgs::foundation::HealthStatus::Healthy:
                    out << R"("healthy")"; break;
                case cgs::foundation::HealthStatus::Degraded:
                    out << R"("degraded")"; break;
                case cgs::foundation::HealthStatus::Unhealthy:
                    out << R"("unhealthy")"; break;
            }
        }
        out << "}";
    }

    out << "}";
    return out.str();
}

/// Build a minimal HTTP response.
std::string httpResponse(int statusCode, std::string_view contentType,
                         std::string_view body) {
    std::ostringstream out;
    out << "HTTP/1.1 " << statusCode;
    switch (statusCode) {
        case 200: out << " OK"; break;
        case 404: out << " Not Found"; break;
        case 503: out << " Service Unavailable"; break;
        default:  out << " Error"; break;
    }
    out << "\r\nContent-Type: " << contentType
        << "\r\nContent-Length: " << body.size()
        << "\r\nConnection: close"
        << "\r\n\r\n"
        << body;
    return out.str();
}

/// Extract the request path from an HTTP request line.
/// e.g., "GET /healthz HTTP/1.1\r\n..." -> "/healthz"
std::string_view extractPath(std::string_view request) {
    // Find "GET " or "HEAD "
    auto methodEnd = request.find(' ');
    if (methodEnd == std::string_view::npos) { return "/"; }
    auto pathStart = methodEnd + 1;
    auto pathEnd = request.find(' ', pathStart);
    if (pathEnd == std::string_view::npos) { return "/"; }
    return request.substr(pathStart, pathEnd - pathStart);
}

} // anonymous namespace

// ── Impl ────────────────────────────────────────────────────────────────────

struct HealthServer::Impl {
    HealthServerConfig config;
    cgs::foundation::GameMetrics& metrics;
    std::atomic<bool> running{false};
    std::atomic<bool> ready{false};
    std::thread serverThread;
    int listenFd{-1};
    std::chrono::steady_clock::time_point startTime{};

    Impl(HealthServerConfig cfg, cgs::foundation::GameMetrics& m)
        : config(std::move(cfg)), metrics(m) {}

    void run() {
        while (running.load(std::memory_order_relaxed)) {
            struct pollfd pfd{};
            pfd.fd = listenFd;
            pfd.events = POLLIN;

            // Poll with 500ms timeout for shutdown responsiveness.
            int ret = poll(&pfd, 1, 500);
            if (ret <= 0) { continue; }

            if ((pfd.revents & POLLIN) == 0) { continue; }

            int clientFd = accept(listenFd, nullptr, nullptr);
            if (clientFd < 0) { continue; }

            handleClient(clientFd);
            close(clientFd);
        }
    }

    void handleClient(int clientFd) {
        // Read the request (small buffer, we only need the first line).
        std::array<char, 1024> buf{};
        auto bytesRead = read(clientFd, buf.data(), buf.size() - 1);
        if (bytesRead <= 0) { return; }

        std::string_view request(buf.data(),
                                 static_cast<std::size_t>(bytesRead));
        auto path = extractPath(request);

        std::string response;

        if (path == "/healthz") {
            auto health = metrics.healthCheck();
            health.serviceName = config.serviceName;
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - startTime);
            auto body = healthToJson(health, uptime);
            response = httpResponse(200, "application/json", body);
        } else if (path == "/readyz") {
            if (ready.load(std::memory_order_relaxed)) {
                auto health = metrics.healthCheck();
                health.serviceName = config.serviceName;
                auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - startTime);
                auto body = healthToJson(health, uptime);
                response = httpResponse(200, "application/json", body);
            } else {
                response = httpResponse(
                    503, "application/json",
                    R"({"status":"not_ready","service":")" + config.serviceName +
                        R"("})");
            }
        } else if (path == "/metrics") {
            auto body = metrics.scrape();
            response = httpResponse(
                200, "text/plain; version=0.0.4; charset=utf-8", body);
        } else {
            response = httpResponse(404, "text/plain", "Not Found");
        }

        // Write response, ignoring partial writes for simplicity
        // (health probes are small payloads over localhost).
        auto written = write(clientFd, response.data(), response.size());
        (void)written;
    }
};

// ── Public API ──────────────────────────────────────────────────────────────

HealthServer::HealthServer(HealthServerConfig config,
                           cgs::foundation::GameMetrics& metrics)
    : impl_(std::make_unique<Impl>(std::move(config), metrics)) {}

HealthServer::~HealthServer() {
    stop();
}

cgs::foundation::GameResult<void> HealthServer::start() {
    if (impl_->running.load()) {
        return cgs::foundation::GameResult<void>::ok();
    }

    // Create socket.
    impl_->listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (impl_->listenFd < 0) {
        return cgs::foundation::GameResult<void>::err(
            cgs::foundation::GameError(
                cgs::foundation::ErrorCode::NetworkError,
                "Failed to create health server socket"));
    }

    // Allow address reuse.
    int optval = 1;
    setsockopt(impl_->listenFd, SOL_SOCKET, SO_REUSEADDR,
               &optval, sizeof(optval));

    // Bind.
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(impl_->config.port);

    if (bind(impl_->listenFd,
             reinterpret_cast<struct sockaddr*>(&addr),  // NOLINT
             sizeof(addr)) < 0) {
        close(impl_->listenFd);
        impl_->listenFd = -1;
        return cgs::foundation::GameResult<void>::err(
            cgs::foundation::GameError(
                cgs::foundation::ErrorCode::ListenFailed,
                "Failed to bind health server on port " +
                    std::to_string(impl_->config.port)));
    }

    // Listen with small backlog (only K8s probes + Prometheus).
    if (listen(impl_->listenFd, 8) < 0) {
        close(impl_->listenFd);
        impl_->listenFd = -1;
        return cgs::foundation::GameResult<void>::err(
            cgs::foundation::GameError(
                cgs::foundation::ErrorCode::ListenFailed,
                "Failed to listen on health server socket"));
    }

    impl_->startTime = std::chrono::steady_clock::now();
    impl_->running.store(true, std::memory_order_relaxed);
    impl_->serverThread = std::thread([this]() { impl_->run(); });

    return cgs::foundation::GameResult<void>::ok();
}

void HealthServer::stop() {
    if (!impl_->running.load(std::memory_order_relaxed)) {
        return;
    }

    impl_->running.store(false, std::memory_order_relaxed);

    // Close the listening socket to unblock poll().
    if (impl_->listenFd >= 0) {
        close(impl_->listenFd);
        impl_->listenFd = -1;
    }

    if (impl_->serverThread.joinable()) {
        impl_->serverThread.join();
    }
}

void HealthServer::setReady(bool ready) {
    impl_->ready.store(ready, std::memory_order_relaxed);
    impl_->metrics.setGauge("cgs_health_ready", ready ? 1.0 : 0.0);
}

bool HealthServer::isRunning() const {
    return impl_->running.load(std::memory_order_relaxed);
}

uint16_t HealthServer::port() const {
    return impl_->config.port;
}

} // namespace cgs::service
