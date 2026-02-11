/// @file persistence_manager.cpp
/// @brief PersistenceManager coordinates WAL + snapshots with periodic timer.

#include "cgs/service/persistence_manager.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_error.hpp"
#include "cgs/foundation/game_metrics.hpp"

namespace cgs::service {

using cgs::foundation::ErrorCode;
using cgs::foundation::GameError;
using cgs::foundation::GameResult;

// -- Impl -------------------------------------------------------------------

struct PersistenceManager::Impl {
    PersistenceConfig config;

    WriteAheadLog wal;
    SnapshotManager snapshots;

    StateCollector collector;
    uint64_t lastSnapshotSequence = 0;

    // Snapshot timer thread
    std::thread timerThread;
    std::atomic<bool> running{false};
    std::mutex snapshotMutex;  // Protects snapshot operations.

    explicit Impl(PersistenceConfig cfg)
        : config(std::move(cfg))
        , wal(config.wal)
        , snapshots(config.snapshot) {}

    /// Perform recovery: load latest snapshot, replay WAL entries after it.
    GameResult<void> recover(const StateRestorer& restorer,
                             const WalApplier& applier) {
        // Try to load the latest snapshot.
        auto snapResult = snapshots.loadLatest();
        if (snapResult.hasValue()) {
            const auto& snap = snapResult.value();
            lastSnapshotSequence = snap.walSequence;
            restorer(snap);
        }
        // If no snapshot exists, that's fine — start from scratch.

        // Replay WAL entries after the snapshot.
        auto replayResult = wal.replay(lastSnapshotSequence, applier);
        if (replayResult.hasError()) {
            return GameResult<void>::err(
                GameError(ErrorCode::RecoveryFailed,
                          "WAL replay failed: " +
                          std::string(replayResult.error().message())));
        }

        return GameResult<void>::ok();
    }

    /// Take a snapshot using the current state collector.
    GameResult<void> doSnapshot() {
        std::lock_guard lock(snapshotMutex);

        if (!collector) {
            return GameResult<void>::err(
                GameError(ErrorCode::PersistenceError,
                          "no state collector registered"));
        }

        auto players = collector();

        auto now = std::chrono::system_clock::now();
        auto timestampUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()).count());

        Snapshot snap;
        snap.walSequence = wal.currentSequence();
        snap.timestampUs = timestampUs;
        snap.players = std::move(players);

        auto saveResult = snapshots.save(snap);
        if (saveResult.hasError()) {
            return saveResult;
        }

        // Truncate WAL entries that are now covered by this snapshot.
        auto truncResult = wal.truncateBefore(snap.walSequence);
        if (truncResult.hasError()) {
            // Non-fatal: snapshot succeeded, WAL just has extra entries.
            // They will be truncated on the next snapshot.
        }

        lastSnapshotSequence = snap.walSequence;

        // Update Prometheus gauges.
        auto& m = cgs::foundation::GameMetrics::instance();
        m.setGauge("cgs_last_snapshot_timestamp",
                    static_cast<double>(snap.timestampUs) / 1e6);
        m.setGauge("cgs_wal_pending_entries",
                    static_cast<double>(wal.entryCount()));

        return GameResult<void>::ok();
    }

    /// Background timer loop for periodic snapshots.
    void timerLoop() {
        using Clock = std::chrono::steady_clock;
        auto nextSnapshot = Clock::now() + config.snapshotInterval;

        while (running.load(std::memory_order_relaxed)) {
            auto now = Clock::now();
            if (now >= nextSnapshot) {
                (void)doSnapshot();
                nextSnapshot = Clock::now() + config.snapshotInterval;
            }

            // Sleep in small increments to allow prompt shutdown.
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
};

// -- Construction / destruction ----------------------------------------------

PersistenceManager::PersistenceManager(PersistenceConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

PersistenceManager::~PersistenceManager() {
    if (impl_ && impl_->running.load(std::memory_order_relaxed)) {
        stop();
    }
}

// -- Lifecycle ---------------------------------------------------------------

GameResult<void> PersistenceManager::start(
    StateCollector collector,
    StateRestorer restorer,
    WalApplier applier) {

    if (impl_->running.load(std::memory_order_relaxed)) {
        return GameResult<void>::err(
            GameError(ErrorCode::PersistenceAlreadyStarted,
                      "persistence manager is already running"));
    }

    // Open WAL and snapshot stores.
    auto walResult = impl_->wal.open();
    if (walResult.hasError()) {
        return walResult;
    }

    auto snapResult = impl_->snapshots.open();
    if (snapResult.hasError()) {
        impl_->wal.close();
        return snapResult;
    }

    // Register the collector.
    impl_->collector = std::move(collector);

    // Perform recovery.
    auto recoverResult = impl_->recover(restorer, applier);
    if (recoverResult.hasError()) {
        impl_->wal.close();
        impl_->snapshots.close();
        return GameResult<void>::err(recoverResult.error());
    }

    // Start the periodic snapshot timer.
    impl_->running.store(true, std::memory_order_relaxed);
    impl_->timerThread = std::thread([this]() { impl_->timerLoop(); });

    return GameResult<void>::ok();
}

void PersistenceManager::stop() {
    if (!impl_->running.load(std::memory_order_relaxed)) {
        return;
    }

    // Signal the timer to stop.
    impl_->running.store(false, std::memory_order_relaxed);

    if (impl_->timerThread.joinable()) {
        impl_->timerThread.join();
    }

    // Take a final snapshot before closing.
    (void)impl_->doSnapshot();

    // Flush and close.
    (void)impl_->wal.flush();
    impl_->wal.close();
    impl_->snapshots.close();
}

// -- Operations --------------------------------------------------------------

GameResult<uint64_t> PersistenceManager::recordChange(WalEntry entry) {
    auto result = impl_->wal.append(std::move(entry));
    if (result.hasValue()) {
        cgs::foundation::GameMetrics::instance().setGauge(
            "cgs_wal_pending_entries",
            static_cast<double>(impl_->wal.entryCount()));
    }
    return result;
}

GameResult<void> PersistenceManager::takeSnapshot() {
    if (!impl_->running.load(std::memory_order_relaxed)) {
        return GameResult<void>::err(
            GameError(ErrorCode::PersistenceNotStarted,
                      "persistence manager is not running"));
    }

    return impl_->doSnapshot();
}

// -- Queries -----------------------------------------------------------------

bool PersistenceManager::isRunning() const {
    return impl_->running.load(std::memory_order_relaxed);
}

std::size_t PersistenceManager::pendingWalEntries() const {
    return impl_->wal.entryCount();
}

uint64_t PersistenceManager::currentWalSequence() const {
    return impl_->wal.currentSequence();
}

} // namespace cgs::service
