#pragma once

/// @file write_ahead_log.hpp
/// @brief Write-Ahead Log for crash-safe player data persistence.
///
/// Records player state mutations as append-only binary entries with
/// CRC32 integrity checksums. On crash recovery, entries are replayed
/// to restore the last consistent state.
///
/// Part of SRS-NFR-013 (zero data loss on crash).

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "cgs/foundation/game_result.hpp"
#include "cgs/foundation/types.hpp"

namespace cgs::service {

/// Type of WAL entry operation.
enum class WalOperation : uint8_t {
    PlayerJoin = 1,    ///< Player entered the world.
    PlayerLeave = 2,   ///< Player left the world.
    StateUpdate = 3,   ///< Player state mutation (position, stats, etc.).
    InventoryChange = 4, ///< Inventory modification.
    QuestUpdate = 5,   ///< Quest progress update.
};

/// A single WAL entry representing one player state mutation.
///
/// Binary layout (on disk):
///   [4 bytes: total_size] [8 bytes: sequence] [8 bytes: timestamp_us]
///   [8 bytes: player_id]  [1 byte: operation]  [4 bytes: data_size]
///   [N bytes: data]       [4 bytes: crc32]
struct WalEntry {
    uint64_t sequence = 0;         ///< Monotonically increasing sequence number.
    uint64_t timestampUs = 0;      ///< Microseconds since epoch.
    cgs::foundation::PlayerId playerId;
    WalOperation operation = WalOperation::StateUpdate;
    std::vector<uint8_t> data;     ///< Opaque serialized payload.
};

/// Configuration for the WriteAheadLog.
struct WalConfig {
    /// Directory where WAL files are stored.
    std::filesystem::path directory = "/var/cgs/wal";

    /// Maximum WAL file size before rotation (bytes). Default 64 MB.
    std::size_t maxFileSize = 64 * 1024 * 1024;

    /// Whether to call fsync after each write for durability.
    bool syncOnWrite = true;
};

/// Append-only Write-Ahead Log for player state durability.
///
/// Usage:
/// @code
///   WriteAheadLog wal({.directory = "/var/cgs/wal"});
///   wal.open();
///
///   WalEntry entry;
///   entry.playerId = PlayerId(42);
///   entry.operation = WalOperation::StateUpdate;
///   entry.data = serialize(playerState);
///   wal.append(entry);
///
///   // On recovery:
///   wal.replay(lastSnapshotSequence, [](const WalEntry& e) {
///       applyToWorld(e);
///   });
///
///   // After successful snapshot:
///   wal.truncateBefore(snapshotSequence);
/// @endcode
///
/// Thread-safe: all operations use internal synchronization.
class WriteAheadLog {
public:
    explicit WriteAheadLog(WalConfig config);
    ~WriteAheadLog();

    WriteAheadLog(const WriteAheadLog&) = delete;
    WriteAheadLog& operator=(const WriteAheadLog&) = delete;

    /// Open or create the WAL directory and prepare for writing.
    [[nodiscard]] cgs::foundation::GameResult<void> open();

    /// Close the WAL, flushing any pending data.
    void close();

    /// Append an entry to the log.
    ///
    /// The sequence number is assigned automatically.
    /// @return The assigned sequence number, or an error.
    [[nodiscard]] cgs::foundation::GameResult<uint64_t> append(WalEntry entry);

    /// Replay all entries with sequence > afterSequence.
    ///
    /// @param afterSequence Replay entries after this sequence (0 = replay all).
    /// @param callback Called for each entry in order.
    /// @return Number of entries replayed, or an error.
    [[nodiscard]] cgs::foundation::GameResult<uint64_t> replay(
        uint64_t afterSequence,
        std::function<void(const WalEntry&)> callback) const;

    /// Remove all entries with sequence <= beforeSequence.
    ///
    /// Called after a successful snapshot to reclaim disk space.
    [[nodiscard]] cgs::foundation::GameResult<void> truncateBefore(
        uint64_t beforeSequence);

    /// Flush any buffered data to disk.
    [[nodiscard]] cgs::foundation::GameResult<void> flush();

    /// Get the current (highest) sequence number.
    [[nodiscard]] uint64_t currentSequence() const;

    /// Get the total number of entries currently in the log.
    [[nodiscard]] std::size_t entryCount() const;

    /// Check if the WAL is open.
    [[nodiscard]] bool isOpen() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cgs::service
