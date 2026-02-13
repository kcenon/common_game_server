#include "cgs/foundation/config_manager.hpp"

#include <fstream>

namespace cgs::foundation {

GameResult<void> ConfigManager::load(const std::filesystem::path& path) {
    std::lock_guard lock(mutex_);
    try {
        auto root = YAML::LoadFile(path.string());
        entries_.clear();
        flatten("", root);
        return GameResult<void>::ok();
    } catch (const YAML::BadFile&) {
        return GameResult<void>::err(
            GameError(ErrorCode::ConfigLoadFailed, "failed to open config file: " + path.string()));
    } catch (const YAML::ParserException& e) {
        return GameResult<void>::err(
            GameError(ErrorCode::ConfigLoadFailed, std::string("YAML parse error: ") + e.what()));
    }
}

void ConfigManager::watch(std::string_view key, ConfigWatchCallback callback) {
    std::lock_guard lock(mutex_);
    watchers_[std::string(key)].push_back(std::move(callback));
}

bool ConfigManager::hasKey(std::string_view key) const {
    std::lock_guard lock(mutex_);
    return entries_.count(std::string(key)) > 0;
}

void ConfigManager::flatten(const std::string& prefix, const YAML::Node& node) {
    if (node.IsMap()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            auto childKey = it->first.as<std::string>();
            auto fullKey = prefix.empty() ? childKey : prefix + "." + childKey;
            flatten(fullKey, it->second);
        }
    } else {
        // Leaf node (scalar, sequence, null) — store with its dotted key.
        entries_[prefix] = YAML::Clone(node);
    }
}

void ConfigManager::notifyWatchers(std::string_view key) {
    std::lock_guard lock(mutex_);
    auto it = watchers_.find(std::string(key));
    if (it != watchers_.end()) {
        for (auto& cb : it->second) {
            cb(key);
        }
    }
}

}  // namespace cgs::foundation
