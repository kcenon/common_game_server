/// @file write_ahead_log.cpp
/// @brief WriteAheadLog implementation with CRC32 integrity and sequential I/O.

#include "cgs/service/write_ahead_log.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <mutex>
#include <numeric>

#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_error.hpp"

namespace cgs::service {

using cgs::foundation::ErrorCode;
using cgs::foundation::GameError;
using cgs::foundation::GameResult;

// -- CRC32 (ISO 3309 polynomial) --------------------------------------------

namespace {

/// Compute CRC32 over a byte range.
uint32_t crc32(const uint8_t* data, std::size_t length) {
    static constexpr uint32_t kPolynomial = 0xEDB88320;
    static const auto table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i;
            for (int j = 0; j < 8; ++j) {
                crc = (crc >> 1) ^ ((crc & 1) ? kPolynomial : 0);
            }
            t[i] = crc;
        }
        return t;
    }();

    uint32_t crc = 0xFFFFFFFF;
    for (std::size_t i = 0; i < length; ++i) {
        crc = (crc >> 8) ^ table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

/// Serialize a WalEntry to binary (excluding CRC and total_size header).
std::vector<uint8_t> serializeEntry(const WalEntry& entry) {
    // Layout: sequence(8) + timestamp(8) + playerId(8) + op(1) + dataSize(4) + data(N)
    constexpr std::size_t kHeaderSize = 8 + 8 + 8 + 1 + 4;
    std::vector<uint8_t> buf(kHeaderSize + entry.data.size());

    auto* p = buf.data();

    std::memcpy(p, &entry.sequence, 8);     p += 8;
    std::memcpy(p, &entry.timestampUs, 8);  p += 8;
    auto pid = entry.playerId.value();
    std::memcpy(p, &pid, 8);               p += 8;
    *p = static_cast<uint8_t>(entry.operation); p += 1;
    auto dataSize = static_cast<uint32_t>(entry.data.size());
    std::memcpy(p, &dataSize, 4);           p += 4;

    if (!entry.data.empty()) {
        std::memcpy(p, entry.data.data(), entry.data.size());
    }

    return buf;
}

/// Deserialize a WalEntry from binary buffer (excluding CRC and total_size).
bool deserializeEntry(const uint8_t* data, std::size_t length, WalEntry& out) {
    constexpr std::size_t kHeaderSize = 8 + 8 + 8 + 1 + 4;
    if (length < kHeaderSize) {
        return false;
    }

    const auto* p = data;
    std::memcpy(&out.sequence, p, 8);      p += 8;
    std::memcpy(&out.timestampUs, p, 8);   p += 8;
    uint64_t pid = 0;
    std::memcpy(&pid, p, 8);              p += 8;
    out.playerId = cgs::foundation::PlayerId(pid);
    out.operation = static_cast<WalOperation>(*p); p += 1;
    uint32_t dataSize = 0;
    std::memcpy(&dataSize, p, 4);          p += 4;

    if (kHeaderSize + dataSize > length) {
        return false;
    }

    out.data.assign(p, p + dataSize);
    return true;
}

/// Get current time in microseconds since epoch.
uint64_t nowMicros() {
    auto now = std::chrono::system_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count());
}

} // namespace

// -- Impl -------------------------------------------------------------------

struct WriteAheadLog::Impl {
    WalConfig config;
    mutable std::mutex mutex;
    std::ofstream writer;
    bool open = false;
    uint64_t nextSequence = 1;
    std::size_t currentFileSize = 0;
    std::size_t totalEntries = 0;

    // In-memory index of entries for replay/truncation.
    // Each entry: (sequence, file offset within the active WAL file)
    // For simplicity, we keep entries in memory. Production would use
    // a file-based index, but for the game server's WAL this is practical
    // since entries are truncated after each snapshot (every ~60s).
    struct IndexEntry {
        uint64_t sequence;
        WalEntry entry;
    };
    std::vector<IndexEntry> entries;

    explicit Impl(WalConfig cfg) : config(std::move(cfg)) {}

    std::filesystem::path walFilePath() const {
        return config.directory / "wal.bin";
    }

    GameResult<void> ensureDirectory() {
        std::error_code ec;
        std::filesystem::create_directories(config.directory, ec);
        if (ec) {
            return GameResult<void>::err(
                GameError(ErrorCode::PersistenceError,
                          "failed to create WAL directory: " + ec.message()));
        }
        return GameResult<void>::ok();
    }

    /// Re-read existing WAL file to rebuild in-memory index.
    GameResult<void> rebuildIndex() {
        auto path = walFilePath();
        if (!std::filesystem::exists(path)) {
            return GameResult<void>::ok();
        }

        std::ifstream reader(path, std::ios::binary);
        if (!reader) {
            return GameResult<void>::err(
                GameError(ErrorCode::WalReadFailed,
                          "cannot open WAL file for reading"));
        }

        entries.clear();
        totalEntries = 0;
        uint64_t maxSeq = 0;

        while (reader.good() && !reader.eof()) {
            // Read total_size
            uint32_t totalSize = 0;
            reader.read(reinterpret_cast<char*>(&totalSize), 4);
            if (reader.gcount() < 4) {
                break;  // EOF or partial read
            }

            if (totalSize < 4) {
                break;  // Corrupted: size too small for CRC
            }

            // Read payload + CRC
            std::vector<uint8_t> buf(totalSize);
            reader.read(reinterpret_cast<char*>(buf.data()),
                        static_cast<std::streamsize>(totalSize));
            if (static_cast<std::size_t>(reader.gcount()) < totalSize) {
                break;  // Truncated entry
            }

            // Verify CRC (last 4 bytes)
            uint32_t storedCrc = 0;
            std::memcpy(&storedCrc, buf.data() + totalSize - 4, 4);
            uint32_t computedCrc = crc32(buf.data(), totalSize - 4);

            if (storedCrc != computedCrc) {
                // Corrupted entry — stop replaying here.
                break;
            }

            // Deserialize entry
            WalEntry entry;
            if (!deserializeEntry(buf.data(), totalSize - 4, entry)) {
                break;
            }

            maxSeq = std::max(maxSeq, entry.sequence);
            entries.push_back({entry.sequence, std::move(entry)});
            ++totalEntries;
        }

        nextSequence = maxSeq + 1;
        currentFileSize = static_cast<std::size_t>(
            std::filesystem::file_size(path));

        return GameResult<void>::ok();
    }
};

// -- Construction / destruction ----------------------------------------------

WriteAheadLog::WriteAheadLog(WalConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

WriteAheadLog::~WriteAheadLog() {
    if (impl_ && impl_->open) {
        close();
    }
}

// -- Lifecycle ---------------------------------------------------------------

GameResult<void> WriteAheadLog::open() {
    std::lock_guard lock(impl_->mutex);

    if (impl_->open) {
        return GameResult<void>::ok();
    }

    auto dirResult = impl_->ensureDirectory();
    if (dirResult.hasError()) {
        return dirResult;
    }

    // Rebuild index from existing WAL file.
    auto indexResult = impl_->rebuildIndex();
    if (indexResult.hasError()) {
        return indexResult;
    }

    // Open file for appending.
    impl_->writer.open(impl_->walFilePath(),
                       std::ios::binary | std::ios::app);
    if (!impl_->writer) {
        return GameResult<void>::err(
            GameError(ErrorCode::WalWriteFailed,
                      "cannot open WAL file for writing"));
    }

    impl_->open = true;
    return GameResult<void>::ok();
}

void WriteAheadLog::close() {
    std::lock_guard lock(impl_->mutex);

    if (!impl_->open) {
        return;
    }

    if (impl_->writer.is_open()) {
        impl_->writer.flush();
        impl_->writer.close();
    }

    impl_->open = false;
}

// -- Write operations --------------------------------------------------------

GameResult<uint64_t> WriteAheadLog::append(WalEntry entry) {
    std::lock_guard lock(impl_->mutex);

    if (!impl_->open) {
        return GameResult<uint64_t>::err(
            GameError(ErrorCode::PersistenceNotStarted,
                      "WAL is not open"));
    }

    // Assign sequence and timestamp.
    entry.sequence = impl_->nextSequence++;
    entry.timestampUs = nowMicros();

    // Serialize entry body.
    auto body = serializeEntry(entry);

    // Compute CRC over body.
    uint32_t checksum = crc32(body.data(), body.size());

    // Total frame: [4: totalSize] [body] [4: crc]
    uint32_t totalSize = static_cast<uint32_t>(body.size() + 4);

    impl_->writer.write(reinterpret_cast<const char*>(&totalSize), 4);
    impl_->writer.write(reinterpret_cast<const char*>(body.data()),
                        static_cast<std::streamsize>(body.size()));
    impl_->writer.write(reinterpret_cast<const char*>(&checksum), 4);

    if (!impl_->writer) {
        return GameResult<uint64_t>::err(
            GameError(ErrorCode::WalWriteFailed,
                      "failed to write WAL entry"));
    }

    if (impl_->config.syncOnWrite) {
        impl_->writer.flush();
    }

    uint64_t seq = entry.sequence;
    impl_->currentFileSize += 4 + body.size() + 4;
    impl_->entries.push_back({seq, std::move(entry)});
    ++impl_->totalEntries;

    return GameResult<uint64_t>::ok(seq);
}

// -- Read operations ---------------------------------------------------------

GameResult<uint64_t> WriteAheadLog::replay(
    uint64_t afterSequence,
    std::function<void(const WalEntry&)> callback) const {

    std::lock_guard lock(impl_->mutex);

    uint64_t count = 0;
    for (const auto& idx : impl_->entries) {
        if (idx.sequence > afterSequence) {
            callback(idx.entry);
            ++count;
        }
    }

    return GameResult<uint64_t>::ok(count);
}

// -- Maintenance -------------------------------------------------------------

GameResult<void> WriteAheadLog::truncateBefore(uint64_t beforeSequence) {
    std::lock_guard lock(impl_->mutex);

    if (!impl_->open) {
        return GameResult<void>::err(
            GameError(ErrorCode::PersistenceNotStarted,
                      "WAL is not open"));
    }

    // Remove entries from in-memory index.
    auto it = std::remove_if(impl_->entries.begin(), impl_->entries.end(),
                              [beforeSequence](const Impl::IndexEntry& e) {
                                  return e.sequence <= beforeSequence;
                              });
    auto removed = static_cast<std::size_t>(
        std::distance(it, impl_->entries.end()));
    impl_->entries.erase(it, impl_->entries.end());
    impl_->totalEntries -= removed;

    // Rewrite the WAL file with remaining entries.
    impl_->writer.close();

    auto path = impl_->walFilePath();
    std::ofstream rewriter(path, std::ios::binary | std::ios::trunc);
    if (!rewriter) {
        // Re-open in append mode even on failure.
        impl_->writer.open(path, std::ios::binary | std::ios::app);
        return GameResult<void>::err(
            GameError(ErrorCode::WalTruncateFailed,
                      "failed to rewrite WAL after truncation"));
    }

    std::size_t newFileSize = 0;
    for (const auto& idx : impl_->entries) {
        auto body = serializeEntry(idx.entry);
        uint32_t checksum = crc32(body.data(), body.size());
        uint32_t totalSize = static_cast<uint32_t>(body.size() + 4);

        rewriter.write(reinterpret_cast<const char*>(&totalSize), 4);
        rewriter.write(reinterpret_cast<const char*>(body.data()),
                       static_cast<std::streamsize>(body.size()));
        rewriter.write(reinterpret_cast<const char*>(&checksum), 4);
        newFileSize += 4 + body.size() + 4;
    }

    rewriter.flush();
    rewriter.close();

    impl_->currentFileSize = newFileSize;

    // Re-open in append mode.
    impl_->writer.open(path, std::ios::binary | std::ios::app);
    if (!impl_->writer) {
        return GameResult<void>::err(
            GameError(ErrorCode::WalWriteFailed,
                      "failed to re-open WAL after truncation"));
    }

    return GameResult<void>::ok();
}

GameResult<void> WriteAheadLog::flush() {
    std::lock_guard lock(impl_->mutex);

    if (!impl_->open) {
        return GameResult<void>::err(
            GameError(ErrorCode::PersistenceNotStarted,
                      "WAL is not open"));
    }

    impl_->writer.flush();
    if (!impl_->writer) {
        return GameResult<void>::err(
            GameError(ErrorCode::WalWriteFailed,
                      "WAL flush failed"));
    }

    return GameResult<void>::ok();
}

// -- Queries -----------------------------------------------------------------

uint64_t WriteAheadLog::currentSequence() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->nextSequence > 0 ? impl_->nextSequence - 1 : 0;
}

std::size_t WriteAheadLog::entryCount() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->totalEntries;
}

bool WriteAheadLog::isOpen() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->open;
}

} // namespace cgs::service
