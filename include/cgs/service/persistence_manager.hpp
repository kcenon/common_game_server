#pragma once

/// @file persistence_manager.hpp
/// @brief Coordinates WAL and snapshots for crash-safe player data persistence.
///
/// PersistenceManager is the high-level interface that service entry points
/// use. It runs a background snapshot timer, coordinates WAL truncation
/// after successful snapshots, and provides a unified recovery API.
///
/// Part of SRS-NFR-013 (zero data loss on crash).

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>

#include "cgs/foundation/game_result.hpp"
#include "cgs/service/snapshot_manager.hpp"
#include "cgs/service/write_ahead_log.hpp"

namespace cgs::service {

/// Configuration for the PersistenceManager.
struct PersistenceConfig {
    /// WAL configuration.
    WalConfig wal;

    /// Snapshot configuration.
    SnapshotConfig snapshot;

    /// Interval between periodic snapshots. Default: 60 seconds.
    std::chrono::seconds snapshotInterval{60};
};

/// Callback invoked to collect current player states for a snapshot.
using StateCollector = std::function<std::vector<PlayerSnapshot>()>;

/// Callback invoked to restore player states from a snapshot during recovery.
using StateRestorer = std::function<void(const Snapshot&)>;

/// Callback invoked to apply a WAL entry during recovery replay.
using WalApplier = std::function<void(const WalEntry&)>;

/// Coordinates WAL and snapshot subsystems for crash-safe persistence.
///
/// Usage:
/// @code
///   PersistenceConfig config;
///   config.wal.directory = "/var/cgs/wal";
///   config.snapshot.directory = "/var/cgs/snapshots";
///   config.snapshotInterval = std::chrono::seconds(60);
///
///   PersistenceManager persistence(config);
///   persistence.start(
///       [&]() { return collectPlayerStates(); },
///       [&](const Snapshot& s) { restorePlayerStates(s); },
///       [&](const WalEntry& e) { applyWalEntry(e); }
///   );
///
///   // Record state changes via WAL
///   WalEntry entry;
///   entry.playerId = PlayerId(42);
///   entry.operation = WalOperation::StateUpdate;
///   persistence.recordChange(entry);
///
///   // On shutdown:
///   persistence.stop();  // Takes final snapshot + flushes WAL
/// @endcode
///
/// Thread-safe: all operations use internal synchronization.
class PersistenceManager {
public:
    explicit PersistenceManager(PersistenceConfig config);
    ~PersistenceManager();

    PersistenceManager(const PersistenceManager&) = delete;
    PersistenceManager& operator=(const PersistenceManager&) = delete;

    /// Start the persistence subsystem.
    ///
    /// Opens WAL and snapshot stores, performs recovery if needed,
    /// and starts the periodic snapshot timer.
    ///
    /// @param collector Called to gather player states for snapshots.
    /// @param restorer Called to restore world state from a snapshot.
    /// @param applier Called to replay individual WAL entries after snapshot.
    [[nodiscard]] cgs::foundation::GameResult<void> start(
        StateCollector collector,
        StateRestorer restorer,
        WalApplier applier);

    /// Stop the persistence subsystem.
    ///
    /// Takes a final snapshot, flushes the WAL, and stops the timer.
    void stop();

    /// Record a player state change in the WAL.
    [[nodiscard]] cgs::foundation::GameResult<uint64_t> recordChange(
        WalEntry entry);

    /// Trigger an immediate snapshot (outside the periodic schedule).
    [[nodiscard]] cgs::foundation::GameResult<void> takeSnapshot();

    /// Check if the persistence subsystem is running.
    [[nodiscard]] bool isRunning() const;

    /// Get the number of WAL entries since the last snapshot.
    [[nodiscard]] std::size_t pendingWalEntries() const;

    /// Get the current WAL sequence number.
    [[nodiscard]] uint64_t currentWalSequence() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cgs::service
