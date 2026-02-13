/// @file snapshot_manager.cpp
/// @brief SnapshotManager implementation with binary format and retention policy.

#include "cgs/service/snapshot_manager.hpp"

#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_error.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <mutex>
#include <regex>

namespace cgs::service {

using cgs::foundation::ErrorCode;
using cgs::foundation::GameError;
using cgs::foundation::GameResult;

// -- Binary format helpers ---------------------------------------------------

namespace {

/// Snapshot file binary layout:
///   [8: walSequence] [8: timestampUs] [4: playerCount]
///   For each player:
///     [8: playerId] [4: instanceId] [4: dataSize] [N: data]

std::vector<uint8_t> serializeSnapshot(const Snapshot& snap) {
    // Calculate total size.
    std::size_t totalSize = 8 + 8 + 4;
    for (const auto& p : snap.players) {
        totalSize += 8 + 4 + 4 + p.data.size();
    }

    std::vector<uint8_t> buf(totalSize);
    auto* ptr = buf.data();

    std::memcpy(ptr, &snap.walSequence, 8);
    ptr += 8;
    std::memcpy(ptr, &snap.timestampUs, 8);
    ptr += 8;
    auto count = static_cast<uint32_t>(snap.players.size());
    std::memcpy(ptr, &count, 4);
    ptr += 4;

    for (const auto& p : snap.players) {
        auto pid = p.playerId.value();
        std::memcpy(ptr, &pid, 8);
        ptr += 8;
        std::memcpy(ptr, &p.instanceId, 4);
        ptr += 4;
        auto dataSize = static_cast<uint32_t>(p.data.size());
        std::memcpy(ptr, &dataSize, 4);
        ptr += 4;
        if (!p.data.empty()) {
            std::memcpy(ptr, p.data.data(), p.data.size());
            ptr += p.data.size();
        }
    }

    return buf;
}

GameResult<Snapshot> deserializeSnapshot(const std::vector<uint8_t>& buf) {
    constexpr std::size_t kHeaderSize = 8 + 8 + 4;
    if (buf.size() < kHeaderSize) {
        return GameResult<Snapshot>::err(
            GameError(ErrorCode::SnapshotCorrupted, "snapshot too small for header"));
    }

    Snapshot snap;
    const auto* ptr = buf.data();

    std::memcpy(&snap.walSequence, ptr, 8);
    ptr += 8;
    std::memcpy(&snap.timestampUs, ptr, 8);
    ptr += 8;
    uint32_t count = 0;
    std::memcpy(&count, ptr, 4);
    ptr += 4;

    const auto* end = buf.data() + buf.size();

    for (uint32_t i = 0; i < count; ++i) {
        if (ptr + 16 > end) {
            return GameResult<Snapshot>::err(GameError(
                ErrorCode::SnapshotCorrupted, "snapshot truncated at player " + std::to_string(i)));
        }

        PlayerSnapshot player;
        uint64_t pid = 0;
        std::memcpy(&pid, ptr, 8);
        ptr += 8;
        player.playerId = cgs::foundation::PlayerId(pid);
        std::memcpy(&player.instanceId, ptr, 4);
        ptr += 4;
        uint32_t dataSize = 0;
        std::memcpy(&dataSize, ptr, 4);
        ptr += 4;

        if (ptr + dataSize > end) {
            return GameResult<Snapshot>::err(
                GameError(ErrorCode::SnapshotCorrupted, "player data truncated"));
        }

        player.data.assign(ptr, ptr + dataSize);
        ptr += dataSize;

        snap.players.push_back(std::move(player));
    }

    return GameResult<Snapshot>::ok(std::move(snap));
}

uint64_t nowMicros() {
    auto now = std::chrono::system_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count());
}

}  // namespace

// -- Impl -------------------------------------------------------------------

struct SnapshotManager::Impl {
    SnapshotConfig config;
    mutable std::mutex mutex;
    bool open = false;

    explicit Impl(SnapshotConfig cfg) : config(std::move(cfg)) {}

    /// Generate a snapshot filename with timestamp for ordering.
    std::filesystem::path snapshotPath(uint64_t timestampUs) const {
        return config.directory / ("snapshot_" + std::to_string(timestampUs) + ".bin");
    }

    /// List all snapshot files sorted by timestamp (oldest first).
    std::vector<std::filesystem::path> listSnapshots() const {
        std::vector<std::filesystem::path> files;

        if (!std::filesystem::exists(config.directory)) {
            return files;
        }

        for (const auto& entry : std::filesystem::directory_iterator(config.directory)) {
            if (entry.is_regular_file() &&
                entry.path().filename().string().starts_with("snapshot_") &&
                entry.path().extension() == ".bin") {
                files.push_back(entry.path());
            }
        }

        std::sort(files.begin(), files.end());
        return files;
    }

    /// Prune old snapshots to keep only maxRetained.
    void pruneOldSnapshots() {
        auto files = listSnapshots();
        while (files.size() > config.maxRetained) {
            std::error_code ec;
            std::filesystem::remove(files.front(), ec);
            files.erase(files.begin());
        }
    }
};

// -- Construction / destruction ----------------------------------------------

SnapshotManager::SnapshotManager(SnapshotConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

SnapshotManager::~SnapshotManager() {
    if (impl_ && impl_->open) {
        close();
    }
}

// -- Lifecycle ---------------------------------------------------------------

GameResult<void> SnapshotManager::open() {
    std::lock_guard lock(impl_->mutex);

    if (impl_->open) {
        return GameResult<void>::ok();
    }

    std::error_code ec;
    std::filesystem::create_directories(impl_->config.directory, ec);
    if (ec) {
        return GameResult<void>::err(GameError(
            ErrorCode::PersistenceError, "failed to create snapshot directory: " + ec.message()));
    }

    impl_->open = true;
    return GameResult<void>::ok();
}

void SnapshotManager::close() {
    std::lock_guard lock(impl_->mutex);
    impl_->open = false;
}

// -- Save / Load -------------------------------------------------------------

GameResult<void> SnapshotManager::save(const Snapshot& snapshot) {
    std::lock_guard lock(impl_->mutex);

    if (!impl_->open) {
        return GameResult<void>::err(
            GameError(ErrorCode::PersistenceNotStarted, "snapshot manager is not open"));
    }

    auto timestampUs = snapshot.timestampUs > 0 ? snapshot.timestampUs : nowMicros();

    auto path = impl_->snapshotPath(timestampUs);
    auto data = serializeSnapshot(snapshot);

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return GameResult<void>::err(
            GameError(ErrorCode::SnapshotWriteFailed, "cannot open snapshot file for writing"));
    }

    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    file.flush();

    if (!file) {
        return GameResult<void>::err(
            GameError(ErrorCode::SnapshotWriteFailed, "failed to write snapshot data"));
    }

    file.close();

    // Prune old snapshots.
    impl_->pruneOldSnapshots();

    return GameResult<void>::ok();
}

GameResult<Snapshot> SnapshotManager::loadLatest() const {
    std::lock_guard lock(impl_->mutex);

    auto files = impl_->listSnapshots();
    if (files.empty()) {
        return GameResult<Snapshot>::err(
            GameError(ErrorCode::SnapshotReadFailed, "no snapshots found"));
    }

    // Load the newest (last sorted) snapshot.
    const auto& latestPath = files.back();

    std::ifstream file(latestPath, std::ios::binary);
    if (!file) {
        return GameResult<Snapshot>::err(
            GameError(ErrorCode::SnapshotReadFailed, "cannot open snapshot file"));
    }

    auto fileSize = std::filesystem::file_size(latestPath);
    std::vector<uint8_t> data(static_cast<std::size_t>(fileSize));
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(fileSize));

    if (static_cast<std::size_t>(file.gcount()) < fileSize) {
        return GameResult<Snapshot>::err(
            GameError(ErrorCode::SnapshotCorrupted, "snapshot file read incomplete"));
    }

    return deserializeSnapshot(data);
}

// -- Queries -----------------------------------------------------------------

std::size_t SnapshotManager::snapshotCount() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->listSnapshots().size();
}

bool SnapshotManager::isOpen() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->open;
}

}  // namespace cgs::service
