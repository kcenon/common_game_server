#pragma once

/// @file snapshot_manager.hpp
/// @brief Periodic player data snapshot manager for crash recovery.
///
/// Captures full player state at configurable intervals and stores
/// binary snapshots on disk. Combined with the WAL, provides the
/// foundation for zero-data-loss recovery.
///
/// Part of SRS-NFR-013 (zero data loss on crash).

#include "cgs/foundation/game_result.hpp"
#include "cgs/foundation/types.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cgs::service {

/// Serialized state for one player.
struct PlayerSnapshot {
    cgs::foundation::PlayerId playerId;
    uint32_t instanceId = 0;
    std::vector<uint8_t> data;  ///< Opaque serialized player state.
};

/// Complete snapshot of all player states at a point in time.
struct Snapshot {
    uint64_t walSequence = 0;  ///< WAL sequence at snapshot time.
    uint64_t timestampUs = 0;  ///< Microseconds since epoch.
    std::vector<PlayerSnapshot> players;
};

/// Configuration for the SnapshotManager.
struct SnapshotConfig {
    /// Directory where snapshots are stored.
    std::filesystem::path directory = "/var/cgs/snapshots";

    /// Maximum number of snapshots to retain (oldest are pruned).
    uint32_t maxRetained = 3;
};

/// Callback type for collecting current player states.
///
/// The snapshot manager calls this to gather the data to persist.
/// Returns a vector of all active player snapshots.
using PlayerStateCollector = std::function<std::vector<PlayerSnapshot>()>;

/// Manages creation, storage, and retrieval of player data snapshots.
///
/// Usage:
/// @code
///   SnapshotManager snapshots({.directory = "/var/cgs/snapshots"});
///   snapshots.open();
///
///   // Save a snapshot
///   auto states = collectPlayerStates();
///   Snapshot snap{.walSequence = wal.currentSequence(), .players = states};
///   snapshots.save(snap);
///
///   // On recovery, load the latest
///   auto latest = snapshots.loadLatest();
///   if (latest.hasValue()) {
///       restoreWorld(latest.value());
///   }
/// @endcode
///
/// Thread-safe: all operations use internal synchronization.
class SnapshotManager {
public:
    explicit SnapshotManager(SnapshotConfig config);
    ~SnapshotManager();

    SnapshotManager(const SnapshotManager&) = delete;
    SnapshotManager& operator=(const SnapshotManager&) = delete;

    /// Open or create the snapshot directory.
    [[nodiscard]] cgs::foundation::GameResult<void> open();

    /// Close the snapshot manager.
    void close();

    /// Save a snapshot to disk.
    ///
    /// Old snapshots beyond maxRetained are pruned automatically.
    [[nodiscard]] cgs::foundation::GameResult<void> save(const Snapshot& snapshot);

    /// Load the most recent snapshot from disk.
    ///
    /// @return The latest snapshot, or an error if none exists.
    [[nodiscard]] cgs::foundation::GameResult<Snapshot> loadLatest() const;

    /// Get the number of snapshots on disk.
    [[nodiscard]] std::size_t snapshotCount() const;

    /// Check if the manager is open.
    [[nodiscard]] bool isOpen() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace cgs::service
