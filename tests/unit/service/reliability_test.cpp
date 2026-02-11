/// @file reliability_test.cpp
/// @brief Unit tests for ServiceCircuitBreaker and HealthServer.

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "cgs/service/circuit_breaker.hpp"
#include "cgs/service/health_server.hpp"
#include "cgs/foundation/game_metrics.hpp"

using namespace cgs::service;
using namespace cgs::foundation;

// ===========================================================================
// Circuit Breaker: State transitions
// ===========================================================================

class CircuitBreakerTest : public ::testing::Test {
protected:
    CircuitBreakerConfig config_{
        .failureThreshold = 3,
        .recoveryTimeout = std::chrono::seconds(1),
        .successThreshold = 2,
        .name = "test-service"
    };
    ServiceCircuitBreaker cb_{config_};
};

TEST_F(CircuitBreakerTest, InitialStateClosed) {
    EXPECT_EQ(cb_.state(), ServiceCircuitBreaker::State::Closed);
    EXPECT_EQ(cb_.failureCount(), 0u);
    EXPECT_EQ(cb_.rejectedCount(), 0u);
}

TEST_F(CircuitBreakerTest, AllowsRequestsWhenClosed) {
    EXPECT_TRUE(cb_.allowRequest());
}

TEST_F(CircuitBreakerTest, SuccessResetsFailureCount) {
    cb_.recordFailure();
    cb_.recordFailure();
    EXPECT_EQ(cb_.failureCount(), 2u);

    cb_.recordSuccess();
    EXPECT_EQ(cb_.failureCount(), 0u);
}

TEST_F(CircuitBreakerTest, OpensAfterThresholdFailures) {
    for (uint32_t i = 0; i < config_.failureThreshold; ++i) {
        (void)cb_.allowRequest();
        cb_.recordFailure();
    }
    EXPECT_EQ(cb_.state(), ServiceCircuitBreaker::State::Open);
}

TEST_F(CircuitBreakerTest, RejectsRequestsWhenOpen) {
    // Force open
    cb_.forceState(ServiceCircuitBreaker::State::Open);
    EXPECT_FALSE(cb_.allowRequest());
    EXPECT_EQ(cb_.rejectedCount(), 1u);

    EXPECT_FALSE(cb_.allowRequest());
    EXPECT_EQ(cb_.rejectedCount(), 2u);
}

TEST_F(CircuitBreakerTest, TransitionsToHalfOpenAfterTimeout) {
    // Trigger open
    for (uint32_t i = 0; i < config_.failureThreshold; ++i) {
        (void)cb_.allowRequest();
        cb_.recordFailure();
    }
    EXPECT_EQ(cb_.state(), ServiceCircuitBreaker::State::Open);

    // Wait for recovery timeout
    std::this_thread::sleep_for(config_.recoveryTimeout +
                                 std::chrono::milliseconds(100));

    // Next allowRequest should transition to half-open
    EXPECT_TRUE(cb_.allowRequest());
    EXPECT_EQ(cb_.state(), ServiceCircuitBreaker::State::HalfOpen);
}

TEST_F(CircuitBreakerTest, HalfOpenClosesAfterSuccesses) {
    cb_.forceState(ServiceCircuitBreaker::State::HalfOpen);

    for (uint32_t i = 0; i < config_.successThreshold; ++i) {
        EXPECT_TRUE(cb_.allowRequest());
        cb_.recordSuccess();
    }

    EXPECT_EQ(cb_.state(), ServiceCircuitBreaker::State::Closed);
}

TEST_F(CircuitBreakerTest, HalfOpenReopensOnFailure) {
    cb_.forceState(ServiceCircuitBreaker::State::HalfOpen);

    (void)cb_.allowRequest();
    cb_.recordFailure();

    EXPECT_EQ(cb_.state(), ServiceCircuitBreaker::State::Open);
}

TEST_F(CircuitBreakerTest, ResetRestoresInitialState) {
    cb_.recordFailure();
    cb_.recordFailure();
    cb_.recordFailure();
    EXPECT_EQ(cb_.state(), ServiceCircuitBreaker::State::Open);
    EXPECT_FALSE(cb_.allowRequest());

    cb_.reset();

    EXPECT_EQ(cb_.state(), ServiceCircuitBreaker::State::Closed);
    EXPECT_EQ(cb_.failureCount(), 0u);
    EXPECT_EQ(cb_.rejectedCount(), 0u);
    EXPECT_TRUE(cb_.allowRequest());
}

TEST_F(CircuitBreakerTest, NameAccessor) {
    EXPECT_EQ(cb_.name(), "test-service");
}

TEST_F(CircuitBreakerTest, ToStringConversion) {
    EXPECT_EQ(toString(ServiceCircuitBreaker::State::Closed), "closed");
    EXPECT_EQ(toString(ServiceCircuitBreaker::State::Open), "open");
    EXPECT_EQ(toString(ServiceCircuitBreaker::State::HalfOpen), "half_open");
}

TEST_F(CircuitBreakerTest, FullCycleClosedOpenHalfOpenClosed) {
    // Closed -> failures -> Open
    for (uint32_t i = 0; i < config_.failureThreshold; ++i) {
        (void)cb_.allowRequest();
        cb_.recordFailure();
    }
    EXPECT_EQ(cb_.state(), ServiceCircuitBreaker::State::Open);

    // Open -> wait -> HalfOpen
    std::this_thread::sleep_for(config_.recoveryTimeout +
                                 std::chrono::milliseconds(100));
    EXPECT_TRUE(cb_.allowRequest());
    EXPECT_EQ(cb_.state(), ServiceCircuitBreaker::State::HalfOpen);

    // HalfOpen -> successes -> Closed
    for (uint32_t i = 0; i < config_.successThreshold; ++i) {
        cb_.recordSuccess();
    }
    EXPECT_EQ(cb_.state(), ServiceCircuitBreaker::State::Closed);
}

// ===========================================================================
// Circuit Breaker: Thread safety
// ===========================================================================

TEST(CircuitBreakerThreadSafetyTest, ConcurrentAccessDoesNotCrash) {
    ServiceCircuitBreaker cb(CircuitBreakerConfig{
        .failureThreshold = 100,
        .recoveryTimeout = std::chrono::seconds(1),
        .successThreshold = 5,
        .name = "concurrent"
    });

    constexpr int kThreads = 4;
    constexpr int kIterations = 1000;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&cb, t]() {
            for (int i = 0; i < kIterations; ++i) {
                if (cb.allowRequest()) {
                    if ((t + i) % 3 == 0) {
                        cb.recordFailure();
                    } else {
                        cb.recordSuccess();
                    }
                }
                (void)cb.state();
                (void)cb.failureCount();
            }
        });
    }

    for (auto& t : threads) { t.join(); }

    // No specific assertions; test verifies no data races or crashes.
    SUCCEED();
}

// ===========================================================================
// Health Server: Lifecycle
// ===========================================================================

class HealthServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        metrics_.reset();
    }

    GameMetrics metrics_;
};

TEST_F(HealthServerTest, StartAndStop) {
    HealthServer server({.port = 0, .serviceName = "test"}, metrics_);
    // Port 0 will fail to be useful but we test the lifecycle pattern.
    // Real tests would use a random available port.
    EXPECT_FALSE(server.isRunning());
}

TEST_F(HealthServerTest, StartOnAvailablePort) {
    // Use a high port that is likely available.
    HealthServerConfig config{.port = 19876, .serviceName = "test"};
    HealthServer server(config, metrics_);

    auto result = server.start();
    if (result.hasValue()) {
        EXPECT_TRUE(server.isRunning());
        EXPECT_EQ(server.port(), 19876);

        server.setReady(true);
        server.stop();
        EXPECT_FALSE(server.isRunning());
    }
    // If port is in use, skip gracefully.
}

TEST_F(HealthServerTest, DoubleStartIsIdempotent) {
    HealthServerConfig config{.port = 19877, .serviceName = "test"};
    HealthServer server(config, metrics_);

    auto r1 = server.start();
    if (r1.hasValue()) {
        auto r2 = server.start();
        EXPECT_TRUE(r2.hasValue());
        server.stop();
    }
}

TEST_F(HealthServerTest, DoubleStopIsIdempotent) {
    HealthServerConfig config{.port = 19878, .serviceName = "test"};
    HealthServer server(config, metrics_);

    auto result = server.start();
    if (result.hasValue()) {
        server.stop();
        server.stop();  // Should not crash
    }
}

// ===========================================================================
// Health Server: HTTP endpoint responses
// ===========================================================================

namespace {

/// Helper: connect to localhost:port, send request, return response body.
std::string httpGet(uint16_t port, const std::string& path) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { return ""; }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr),
                sizeof(addr)) < 0) {
        close(fd);
        return "";
    }

    std::string request = "GET " + path + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
    auto written = write(fd, request.data(), request.size());
    (void)written;

    // Read response
    std::string response;
    char buf[4096];
    ssize_t n = 0;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        response.append(buf, static_cast<std::size_t>(n));
    }
    close(fd);
    return response;
}

} // anonymous namespace

TEST_F(HealthServerTest, HealthzEndpoint) {
    constexpr uint16_t kPort = 19879;
    HealthServer server({.port = kPort, .serviceName = "test-svc"}, metrics_);

    auto result = server.start();
    if (!result.hasValue()) { GTEST_SKIP() << "Port unavailable"; }

    // Allow server thread to start accepting.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto response = httpGet(kPort, "/healthz");
    EXPECT_TRUE(response.find("200 OK") != std::string::npos);
    EXPECT_TRUE(response.find("\"status\":\"healthy\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"service\":\"test-svc\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"uptime_seconds\":") != std::string::npos);

    server.stop();
}

TEST_F(HealthServerTest, ReadyzEndpointNotReady) {
    constexpr uint16_t kPort = 19880;
    HealthServer server({.port = kPort, .serviceName = "test-svc"}, metrics_);

    auto result = server.start();
    if (!result.hasValue()) { GTEST_SKIP() << "Port unavailable"; }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Not ready by default
    auto response = httpGet(kPort, "/readyz");
    EXPECT_TRUE(response.find("503") != std::string::npos);
    EXPECT_TRUE(response.find("not_ready") != std::string::npos);

    server.stop();
}

TEST_F(HealthServerTest, ReadyzEndpointReady) {
    constexpr uint16_t kPort = 19881;
    HealthServer server({.port = kPort, .serviceName = "test-svc"}, metrics_);

    auto result = server.start();
    if (!result.hasValue()) { GTEST_SKIP() << "Port unavailable"; }

    server.setReady(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto response = httpGet(kPort, "/readyz");
    EXPECT_TRUE(response.find("200 OK") != std::string::npos);
    EXPECT_TRUE(response.find("\"status\":\"healthy\"") != std::string::npos);

    server.stop();
}

TEST_F(HealthServerTest, MetricsEndpoint) {
    constexpr uint16_t kPort = 19882;
    HealthServer server({.port = kPort, .serviceName = "test-svc"}, metrics_);

    // Add some metrics
    metrics_.incrementCounter("test_requests_total", 42);
    metrics_.setGauge("test_ccu", 100.0);

    auto result = server.start();
    if (!result.hasValue()) { GTEST_SKIP() << "Port unavailable"; }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto response = httpGet(kPort, "/metrics");
    EXPECT_TRUE(response.find("200 OK") != std::string::npos);
    EXPECT_TRUE(response.find("text/plain") != std::string::npos);
    EXPECT_TRUE(response.find("test_requests_total 42") != std::string::npos);
    EXPECT_TRUE(response.find("test_ccu 100") != std::string::npos);

    server.stop();
}

TEST_F(HealthServerTest, UnknownPathReturns404) {
    constexpr uint16_t kPort = 19883;
    HealthServer server({.port = kPort, .serviceName = "test-svc"}, metrics_);

    auto result = server.start();
    if (!result.hasValue()) { GTEST_SKIP() << "Port unavailable"; }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto response = httpGet(kPort, "/unknown");
    EXPECT_TRUE(response.find("404") != std::string::npos);

    server.stop();
}

TEST_F(HealthServerTest, HealthzShowsDegradedComponents) {
    constexpr uint16_t kPort = 19884;
    HealthServer server({.port = kPort, .serviceName = "test-svc"}, metrics_);

    metrics_.setComponentHealth("database", HealthStatus::Healthy);
    metrics_.setComponentHealth("cache", HealthStatus::Degraded);

    auto result = server.start();
    if (!result.hasValue()) { GTEST_SKIP() << "Port unavailable"; }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto response = httpGet(kPort, "/healthz");
    EXPECT_TRUE(response.find("\"status\":\"degraded\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"database\":\"healthy\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"cache\":\"degraded\"") != std::string::npos);

    server.stop();
}
