#pragma once

/// @file config_manager.hpp
/// @brief YAML-based configuration management with typed access and watch support.

#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "cgs/foundation/game_result.hpp"

namespace cgs::foundation {

/// Callback invoked when a watched configuration key changes.
using ConfigWatchCallback = std::function<void(std::string_view key)>;

/// YAML-based configuration manager providing typed access to config values.
///
/// Supports loading from file, dotted-key access (e.g., "server.port"),
/// setting values at runtime, and registering callbacks for change notification.
///
/// Internally flattens the YAML tree into a key-value map to avoid
/// yaml-cpp reference-semantic pitfalls.
class ConfigManager {
public:
    ConfigManager() = default;

    /// Load configuration from a YAML file.
    /// @param path Filesystem path to the YAML config file.
    /// @return Success or ConfigLoadFailed error.
    GameResult<void> load(const std::filesystem::path& path);

    /// Retrieve a typed value by dotted key (e.g., "server.port").
    /// @return The value or ConfigKeyNotFound/ConfigTypeMismatch error.
    template <typename T>
    GameResult<T> get(std::string_view key) const;

    /// Set a value by dotted key (creates intermediate nodes as needed).
    /// Notifies any registered watchers for this key.
    template <typename T>
    void set(std::string_view key, const T& value);

    /// Register a callback that fires when the given key changes via set().
    void watch(std::string_view key, ConfigWatchCallback callback);

    /// Check if a key exists in the current configuration.
    [[nodiscard]] bool hasKey(std::string_view key) const;

private:
    /// Flatten a YAML node recursively into the entries_ map.
    void flatten(const std::string& prefix, const YAML::Node& node);

    /// Notify watchers registered for the given key.
    void notifyWatchers(std::string_view key);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, YAML::Node> entries_;
    std::unordered_map<std::string, std::vector<ConfigWatchCallback>> watchers_;
};

// --- Template implementations ---

template <typename T>
GameResult<T> ConfigManager::get(std::string_view key) const {
    std::lock_guard lock(mutex_);
    auto it = entries_.find(std::string(key));
    if (it == entries_.end()) {
        return GameResult<T>::err(
            GameError(ErrorCode::ConfigKeyNotFound,
                      std::string("config key not found: ") + std::string(key)));
    }
    try {
        return GameResult<T>::ok(it->second.as<T>());
    } catch (const YAML::BadConversion&) {
        return GameResult<T>::err(
            GameError(ErrorCode::ConfigTypeMismatch,
                      std::string("type mismatch for key: ") + std::string(key)));
    }
}

template <typename T>
void ConfigManager::set(std::string_view key, const T& value) {
    {
        std::lock_guard lock(mutex_);
        entries_[std::string(key)] = YAML::Node(value);
    }
    notifyWatchers(key);
}

} // namespace cgs::foundation
