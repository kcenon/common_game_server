#pragma once

/// @file file_watcher.hpp
/// @brief Polling-based file change detector for plugin hot reload.
///
/// Monitors plugin shared library files (.so/.dll/.dylib) for
/// modifications by comparing filesystem timestamps.
///
/// @see SRS-PLG-005.1
/// @see SDS-MOD-023

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace cgs::plugin {

/// Callback invoked when a watched file is modified.
/// @param path  The path of the modified file.
using FileChangeCallback = std::function<void(const std::filesystem::path& path)>;

/// Polling-based file change detector.
///
/// Watches shared library files by periodically checking their
/// last-write timestamps.  Changes within a configurable debounce
/// window are coalesced into a single callback invocation.
///
/// Usage:
/// @code
///   FileWatcher watcher;
///   watcher.SetCallback([](const std::filesystem::path& p) {
///       std::cout << "Modified: " << p << "\n";
///   });
///   watcher.Watch("/path/to/plugin.so");
///   watcher.Poll();  // Call periodically.
/// @endcode
class FileWatcher {
public:
    FileWatcher();
    ~FileWatcher() = default;

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    FileWatcher(FileWatcher&&) = delete;
    FileWatcher& operator=(FileWatcher&&) = delete;

    /// Set the callback invoked when a file modification is detected.
    void SetCallback(FileChangeCallback callback);

    /// Start watching a file for modifications.
    ///
    /// Records the current last-write time as baseline.
    /// @return true if the file exists and watching started.
    bool Watch(const std::filesystem::path& path);

    /// Stop watching a specific file.
    void Unwatch(const std::filesystem::path& path);

    /// Stop watching all files and clear state.
    void UnwatchAll();

    /// Poll all watched files for modifications.
    ///
    /// Compares current timestamps against stored baselines.
    /// If a change is detected and the debounce window has elapsed,
    /// the callback is invoked.
    void Poll();

    /// Set the debounce duration in milliseconds.
    ///
    /// File changes within this window after the last detected change
    /// are coalesced into a single callback.
    void SetDebounceMs(uint32_t ms);

    /// Return the number of watched files.
    [[nodiscard]] std::size_t WatchCount() const;

    /// Check whether a specific path is being watched.
    [[nodiscard]] bool IsWatching(const std::filesystem::path& path) const;

private:
    struct WatchEntry {
        std::filesystem::file_time_type lastWriteTime;
        std::chrono::steady_clock::time_point lastChangeDetected;
        bool pendingCallback = false;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, WatchEntry> entries_;
    FileChangeCallback callback_;
    std::chrono::milliseconds debounceMs_{200};
};

} // namespace cgs::plugin
