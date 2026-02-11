/// @file file_watcher.cpp
/// @brief Polling-based file change detection implementation.
///
/// @see SDS-MOD-023

#include "cgs/plugin/file_watcher.hpp"

#include <utility>

namespace fs = std::filesystem;

namespace cgs::plugin {

FileWatcher::FileWatcher() = default;

void FileWatcher::SetCallback(FileChangeCallback callback) {
    std::lock_guard lock(mutex_);
    callback_ = std::move(callback);
}

bool FileWatcher::Watch(const fs::path& path) {
    std::error_code ec;
    auto writeTime = fs::last_write_time(path, ec);
    if (ec) {
        return false;
    }

    std::lock_guard lock(mutex_);
    auto key = path.string();
    entries_[key] = WatchEntry{
        writeTime,
        std::chrono::steady_clock::time_point{},
        false};
    return true;
}

void FileWatcher::Unwatch(const fs::path& path) {
    std::lock_guard lock(mutex_);
    entries_.erase(path.string());
}

void FileWatcher::UnwatchAll() {
    std::lock_guard lock(mutex_);
    entries_.clear();
}

void FileWatcher::Poll() {
    // Collect changes under the lock, invoke callbacks outside.
    std::vector<fs::path> changed;
    auto now = std::chrono::steady_clock::now();

    {
        std::lock_guard lock(mutex_);
        for (auto& [pathStr, entry] : entries_) {
            std::error_code ec;
            auto currentTime = fs::last_write_time(fs::path(pathStr), ec);
            if (ec) {
                continue;
            }

            if (currentTime != entry.lastWriteTime) {
                entry.lastWriteTime = currentTime;
                entry.lastChangeDetected = now;
                entry.pendingCallback = true;
            }

            if (entry.pendingCallback) {
                auto elapsed = now - entry.lastChangeDetected;
                if (elapsed >= debounceMs_) {
                    entry.pendingCallback = false;
                    changed.emplace_back(pathStr);
                }
            }
        }
    }

    // Invoke callbacks outside the lock to prevent deadlock.
    for (const auto& path : changed) {
        if (callback_) {
            callback_(path);
        }
    }
}

void FileWatcher::SetDebounceMs(uint32_t ms) {
    std::lock_guard lock(mutex_);
    debounceMs_ = std::chrono::milliseconds(ms);
}

std::size_t FileWatcher::WatchCount() const {
    std::lock_guard lock(mutex_);
    return entries_.size();
}

bool FileWatcher::IsWatching(const fs::path& path) const {
    std::lock_guard lock(mutex_);
    return entries_.count(path.string()) > 0;
}

} // namespace cgs::plugin
