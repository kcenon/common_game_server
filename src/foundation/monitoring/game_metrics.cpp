/// @file game_metrics.cpp
/// @brief In-memory implementation of GameMetrics (SDS-MOD-006).
///
/// Uses atomic operations for counters/gauges and a mutex for histograms
/// and health state. When monitoring_system becomes available as a kcenon
/// dependency, this file and CMakeLists.txt are the only files that change.

#include "cgs/foundation/game_metrics.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <mutex>
#include <sstream>
#include <unordered_map>

namespace cgs::foundation {

// ── HistogramBuckets factory methods ────────────────────────────────────────

HistogramBuckets HistogramBuckets::defaultLatency() {
    return HistogramBuckets{{1, 5, 10, 25, 50, 100, 250, 500, 1000}};
}

HistogramBuckets HistogramBuckets::defaultDuration() {
    return HistogramBuckets{{0.1, 0.5, 1, 2.5, 5, 10, 25, 50}};
}

// ── Histogram data ──────────────────────────────────────────────────────────

namespace {

struct HistogramData {
    std::vector<double> boundaries;
    std::vector<uint64_t> bucketCounts;  // one per boundary + 1 for +Inf
    uint64_t totalCount{0};
    double totalSum{0.0};

    explicit HistogramData(std::vector<double> bounds)
        : boundaries(std::move(bounds)), bucketCounts(boundaries.size() + 1, 0) {}

    void record(double value) {
        for (std::size_t i = 0; i < boundaries.size(); ++i) {
            if (value <= boundaries[i]) {
                ++bucketCounts[i];
            }
        }
        // +Inf bucket always incremented
        ++bucketCounts.back();
        ++totalCount;
        totalSum += value;
    }
};

// Atomic double helper using CAS loop.
// std::atomic<double> does not support fetch_add in C++20.
void atomicAdd(std::atomic<double>& target, double delta) {
    double current = target.load(std::memory_order_relaxed);
    while (!target.compare_exchange_weak(
        current, current + delta, std::memory_order_release, std::memory_order_relaxed)) {
        // CAS failed, current now holds the latest value; retry
    }
}

void atomicStore(std::atomic<double>& target, double value) {
    target.store(value, std::memory_order_release);
}

double atomicLoad(const std::atomic<double>& target) {
    return target.load(std::memory_order_acquire);
}

// Format a double for Prometheus output, removing unnecessary trailing zeros.
std::string formatDouble(double value) {
    if (std::isinf(value)) {
        return value > 0 ? "+Inf" : "-Inf";
    }
    if (std::isnan(value)) {
        return "NaN";
    }
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

}  // anonymous namespace

// ── Impl ────────────────────────────────────────────────────────────────────

struct GameMetrics::Impl {
    // Counters: lock-free via atomic<uint64_t>.
    // We use a mutex for map insertion but reads/increments on existing
    // counters are lock-free after creation.
    mutable std::mutex counterMutex;
    std::unordered_map<std::string, std::atomic<uint64_t>> counters;

    // Gauges: similar pattern with atomic<double>.
    mutable std::mutex gaugeMutex;
    std::unordered_map<std::string, std::atomic<double>> gauges;

    // Histograms: fully mutex-protected (recording mutates vectors).
    mutable std::mutex histogramMutex;
    std::unordered_map<std::string, HistogramData> histograms;

    // Trace span ID generator.
    std::atomic<uint64_t> nextSpanId{1};

    // Health state.
    mutable std::mutex healthMutex;
    std::string serviceName{"cgs"};
    std::unordered_map<std::string, HealthStatus> componentHealth;
};

// ── Construction / destruction / move ───────────────────────────────────────

GameMetrics::GameMetrics() : impl_(std::make_unique<Impl>()) {}

GameMetrics::~GameMetrics() = default;

GameMetrics::GameMetrics(GameMetrics&&) noexcept = default;

GameMetrics& GameMetrics::operator=(GameMetrics&&) noexcept = default;

// ── Counters ────────────────────────────────────────────────────────────────

void GameMetrics::incrementCounter(std::string_view name, uint64_t value) {
    std::lock_guard lock(impl_->counterMutex);
    impl_->counters[std::string(name)].fetch_add(value, std::memory_order_relaxed);
}

uint64_t GameMetrics::counterValue(std::string_view name) const {
    std::lock_guard lock(impl_->counterMutex);
    auto it = impl_->counters.find(std::string(name));
    if (it == impl_->counters.end()) {
        return 0;
    }
    return it->second.load(std::memory_order_relaxed);
}

// ── Gauges ──────────────────────────────────────────────────────────────────

void GameMetrics::setGauge(std::string_view name, double value) {
    std::lock_guard lock(impl_->gaugeMutex);
    atomicStore(impl_->gauges[std::string(name)], value);
}

void GameMetrics::incrementGauge(std::string_view name, double delta) {
    std::lock_guard lock(impl_->gaugeMutex);
    atomicAdd(impl_->gauges[std::string(name)], delta);
}

void GameMetrics::decrementGauge(std::string_view name, double delta) {
    std::lock_guard lock(impl_->gaugeMutex);
    atomicAdd(impl_->gauges[std::string(name)], -delta);
}

double GameMetrics::gaugeValue(std::string_view name) const {
    std::lock_guard lock(impl_->gaugeMutex);
    auto it = impl_->gauges.find(std::string(name));
    if (it == impl_->gauges.end()) {
        return 0.0;
    }
    return atomicLoad(it->second);
}

// ── Histograms ──────────────────────────────────────────────────────────────

void GameMetrics::registerHistogram(std::string_view name, HistogramBuckets buckets) {
    std::lock_guard lock(impl_->histogramMutex);
    auto key = std::string(name);
    if (impl_->histograms.find(key) == impl_->histograms.end()) {
        impl_->histograms.emplace(std::move(key), HistogramData(std::move(buckets.boundaries)));
    }
}

void GameMetrics::recordHistogram(std::string_view name, double value) {
    std::lock_guard lock(impl_->histogramMutex);
    auto it = impl_->histograms.find(std::string(name));
    if (it != impl_->histograms.end()) {
        it->second.record(value);
    }
}

// ── Tracing ─────────────────────────────────────────────────────────────────

TraceSpan GameMetrics::startSpan(std::string_view name) {
    TraceSpan span;
    span.id = impl_->nextSpanId.fetch_add(1, std::memory_order_relaxed);
    span.name = std::string(name);
    span.startTime = std::chrono::steady_clock::now();
    span.ended = false;
    return span;
}

void GameMetrics::endSpan(TraceSpan& span) {
    span.ended = true;
}

// ── Health ───────────────────────────────────────────────────────────────────

void GameMetrics::setComponentHealth(std::string_view component, HealthStatus status) {
    std::lock_guard lock(impl_->healthMutex);
    impl_->componentHealth[std::string(component)] = status;
}

HealthCheckResult GameMetrics::healthCheck() const {
    std::lock_guard lock(impl_->healthMutex);

    HealthCheckResult result;
    result.serviceName = impl_->serviceName;
    result.timestamp = std::chrono::system_clock::now();
    result.components = impl_->componentHealth;

    // Overall status = worst component status
    result.status = HealthStatus::Healthy;
    for (const auto& [_, status] : impl_->componentHealth) {
        if (status == HealthStatus::Unhealthy) {
            result.status = HealthStatus::Unhealthy;
            break;
        }
        if (status == HealthStatus::Degraded) {
            result.status = HealthStatus::Degraded;
        }
    }
    return result;
}

// ── Prometheus scrape ───────────────────────────────────────────────────────

std::string GameMetrics::scrape() const {
    std::ostringstream out;

    // Counters
    {
        std::lock_guard lock(impl_->counterMutex);
        for (const auto& [name, value] : impl_->counters) {
            out << "# TYPE " << name << " counter\n";
            out << name << " " << value.load(std::memory_order_relaxed) << "\n";
        }
    }

    // Gauges
    {
        std::lock_guard lock(impl_->gaugeMutex);
        for (const auto& [name, value] : impl_->gauges) {
            out << "# TYPE " << name << " gauge\n";
            out << name << " " << formatDouble(atomicLoad(value)) << "\n";
        }
    }

    // Histograms
    {
        std::lock_guard lock(impl_->histogramMutex);
        for (const auto& [name, data] : impl_->histograms) {
            out << "# TYPE " << name << " histogram\n";

            // Cumulative bucket counts
            for (std::size_t i = 0; i < data.boundaries.size(); ++i) {
                out << name << "_bucket{le=\"" << formatDouble(data.boundaries[i]) << "\"} "
                    << data.bucketCounts[i] << "\n";
            }
            out << name << "_bucket{le=\"+Inf\"} " << data.bucketCounts.back() << "\n";

            out << name << "_sum " << formatDouble(data.totalSum) << "\n";
            out << name << "_count " << data.totalCount << "\n";
        }
    }

    return out.str();
}

// ── Reset ───────────────────────────────────────────────────────────────────

void GameMetrics::reset() {
    {
        std::lock_guard lock(impl_->counterMutex);
        impl_->counters.clear();
    }
    {
        std::lock_guard lock(impl_->gaugeMutex);
        impl_->gauges.clear();
    }
    {
        std::lock_guard lock(impl_->histogramMutex);
        impl_->histograms.clear();
    }
    {
        std::lock_guard lock(impl_->healthMutex);
        impl_->componentHealth.clear();
    }
}

// ── Singleton ───────────────────────────────────────────────────────────────

GameMetrics& GameMetrics::instance() {
    static GameMetrics inst;
    return inst;
}

}  // namespace cgs::foundation
