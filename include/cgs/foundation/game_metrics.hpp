#pragma once

/// @file game_metrics.hpp
/// @brief GameMetrics for game-specific metric collection, distributed tracing,
///        health checking, and Prometheus-compatible export.
///
/// Provides counters, gauges, histograms, trace spans, and component health
/// status with thread-safe in-memory storage behind PIMPL. When the kcenon
/// monitoring_system becomes available, only the implementation file and
/// CMakeLists.txt need to change.
/// Part of the Monitoring System Adapter (SDS-MOD-006).

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace cgs::foundation {

// ── Metric types ────────────────────────────────────────────────────────────

/// Classification of a metric for Prometheus TYPE annotation.
enum class MetricType : uint8_t {
    Counter,
    Gauge,
    Histogram
};

/// Bucket boundaries for histogram metrics.
///
/// Histograms partition observed values into buckets. Each boundary defines
/// the upper bound of a bucket (le = "less than or equal").
struct HistogramBuckets {
    /// Default latency buckets in milliseconds: {1,5,10,25,50,100,250,500,1000}.
    static HistogramBuckets defaultLatency();

    /// Default duration buckets in seconds: {0.1,0.5,1,2.5,5,10,25,50}.
    static HistogramBuckets defaultDuration();

    std::vector<double> boundaries;
};

// ── Tracing ─────────────────────────────────────────────────────────────────

/// A lightweight trace span for distributed tracing.
///
/// Call GameMetrics::startSpan() to begin a span and endSpan() to complete it.
/// The span records wall-clock start time and a monotonic duration.
struct TraceSpan {
    uint64_t id{0};
    std::string name;
    std::chrono::steady_clock::time_point startTime{};
    bool ended{false};
};

// ── Health checking ─────────────────────────────────────────────────────────

/// Overall health status of a component or the service itself.
enum class HealthStatus : uint8_t {
    Healthy,
    Degraded,
    Unhealthy
};

/// Aggregated health check result for the service.
struct HealthCheckResult {
    HealthStatus status{HealthStatus::Healthy};
    std::string serviceName;
    std::unordered_map<std::string, HealthStatus> components;
    std::chrono::system_clock::time_point timestamp{};
};

// ── GameMetrics ─────────────────────────────────────────────────────────────

/// Central metrics facade providing counters, gauges, histograms, trace spans,
/// health status, and Prometheus text-format export.
///
/// Thread-safe: counters and gauges use atomic operations; histograms and
/// health maps are mutex-protected. The class is non-copyable but movable
/// (PIMPL pattern).
///
/// Example:
/// @code
///   auto& metrics = GameMetrics::instance();
///   metrics.incrementCounter("cgs_messages_total");
///   metrics.setGauge("cgs_ccu", 1500.0);
///   metrics.registerHistogram("cgs_tick_ms", HistogramBuckets::defaultLatency());
///   metrics.recordHistogram("cgs_tick_ms", 12.5);
///   std::string prom = metrics.scrape();  // Prometheus text format
/// @endcode
class GameMetrics {
public:
    GameMetrics();
    ~GameMetrics();

    GameMetrics(const GameMetrics&) = delete;
    GameMetrics& operator=(const GameMetrics&) = delete;
    GameMetrics(GameMetrics&&) noexcept;
    GameMetrics& operator=(GameMetrics&&) noexcept;

    // ── Counters ────────────────────────────────────────────────────────

    /// Increment a counter by the given value (default 1).
    /// Creates the counter on first use.
    void incrementCounter(std::string_view name, uint64_t value = 1);

    /// Read the current counter value. Returns 0 if the counter does not exist.
    [[nodiscard]] uint64_t counterValue(std::string_view name) const;

    // ── Gauges ──────────────────────────────────────────────────────────

    /// Set a gauge to an absolute value. Creates the gauge on first use.
    void setGauge(std::string_view name, double value);

    /// Increment a gauge by delta (default 1.0).
    void incrementGauge(std::string_view name, double delta = 1.0);

    /// Decrement a gauge by delta (default 1.0).
    void decrementGauge(std::string_view name, double delta = 1.0);

    /// Read the current gauge value. Returns 0.0 if the gauge does not exist.
    [[nodiscard]] double gaugeValue(std::string_view name) const;

    // ── Histograms ──────────────────────────────────────────────────────

    /// Register a histogram with the given bucket boundaries.
    /// Must be called before recordHistogram() for the same name.
    void registerHistogram(std::string_view name, HistogramBuckets buckets);

    /// Record an observation in a previously registered histogram.
    /// No-op if the histogram has not been registered.
    void recordHistogram(std::string_view name, double value);

    // ── Tracing ─────────────────────────────────────────────────────────

    /// Begin a new trace span with a unique ID and the current timestamp.
    [[nodiscard]] TraceSpan startSpan(std::string_view name);

    /// End a span, recording its completion.
    void endSpan(TraceSpan& span);

    // ── Health ──────────────────────────────────────────────────────────

    /// Set the health status of a named component.
    void setComponentHealth(std::string_view component, HealthStatus status);

    /// Aggregate component statuses into a HealthCheckResult.
    /// Overall status is the worst status among all components.
    [[nodiscard]] HealthCheckResult healthCheck() const;

    // ── Export ───────────────────────────────────────────────────────────

    /// Serialize all metrics in Prometheus text exposition format.
    [[nodiscard]] std::string scrape() const;

    // ── Utility ─────────────────────────────────────────────────────────

    /// Clear all metrics, histograms, and health state. Intended for tests.
    void reset();

    // ── Singleton ───────────────────────────────────────────────────────

    /// Access the global GameMetrics instance.
    static GameMetrics& instance();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cgs::foundation
