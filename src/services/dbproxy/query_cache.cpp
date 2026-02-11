/// @file query_cache.cpp
/// @brief QueryCache implementation using a doubly-linked list + hash map
///        for O(1) LRU eviction and lookup.

#include "cgs/service/query_cache.hpp"

#include <algorithm>
#include <atomic>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

namespace cgs::service {

// ── Cache entry stored in the LRU list ──────────────────────────────────────

struct CacheEntry {
    std::string sql;
    cgs::foundation::QueryResult result;
    std::chrono::steady_clock::time_point expiresAt;
};

// ── Impl ────────────────────────────────────────────────────────────────────

struct QueryCache::Impl {
    CacheConfig config;

    // LRU list: front = most recently used, back = least recently used.
    std::list<CacheEntry> lruList;

    // Map from SQL key to iterator into the LRU list for O(1) lookup.
    std::unordered_map<std::string, std::list<CacheEntry>::iterator> index;

    mutable std::mutex mutex;

    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};

    // Move an entry to the front of the LRU list.
    void touch(std::list<CacheEntry>::iterator it) {
        lruList.splice(lruList.begin(), lruList, it);
    }

    // Evict the least recently used entry.
    void evictLru() {
        if (lruList.empty()) {
            return;
        }
        auto& back = lruList.back();
        index.erase(back.sql);
        lruList.pop_back();
    }

    // Remove expired entries from the back of the list.
    void evictExpired() {
        auto now = std::chrono::steady_clock::now();

        // Walk from the back (least recently used) and remove expired.
        auto it = lruList.end();
        while (it != lruList.begin()) {
            --it;
            if (it->expiresAt <= now) {
                auto toRemove = it;
                it = std::next(it) == lruList.end() ? lruList.begin() : std::next(it);
                index.erase(toRemove->sql);
                lruList.erase(toRemove);
                if (it == lruList.begin()) {
                    break;
                }
            }
        }
    }
};

// ── Construction / destruction / move ───────────────────────────────────────

QueryCache::QueryCache(CacheConfig config)
    : impl_(std::make_unique<Impl>()) {
    impl_->config = std::move(config);
}

QueryCache::~QueryCache() = default;

QueryCache::QueryCache(QueryCache&&) noexcept = default;
QueryCache& QueryCache::operator=(QueryCache&&) noexcept = default;

// ── get() ───────────────────────────────────────────────────────────────────

std::optional<cgs::foundation::QueryResult> QueryCache::get(
    std::string_view sql) {
    std::lock_guard lock(impl_->mutex);

    auto it = impl_->index.find(std::string(sql));
    if (it == impl_->index.end()) {
        impl_->misses.fetch_add(1, std::memory_order_relaxed);
        return std::nullopt;
    }

    auto listIt = it->second;

    // Check TTL expiration.
    if (listIt->expiresAt <= std::chrono::steady_clock::now()) {
        impl_->lruList.erase(listIt);
        impl_->index.erase(it);
        impl_->misses.fetch_add(1, std::memory_order_relaxed);
        return std::nullopt;
    }

    // Move to front (most recently used).
    impl_->touch(listIt);
    impl_->hits.fetch_add(1, std::memory_order_relaxed);
    return listIt->result;
}

// ── put() ───────────────────────────────────────────────────────────────────

void QueryCache::put(std::string_view sql,
                     const cgs::foundation::QueryResult& result) {
    put(sql, result, impl_->config.defaultTtl);
}

void QueryCache::put(std::string_view sql,
                     const cgs::foundation::QueryResult& result,
                     std::chrono::seconds ttl) {
    std::lock_guard lock(impl_->mutex);

    auto key = std::string(sql);

    // If key already exists, update in place.
    auto it = impl_->index.find(key);
    if (it != impl_->index.end()) {
        auto listIt = it->second;
        listIt->result = result;
        listIt->expiresAt = std::chrono::steady_clock::now() + ttl;
        impl_->touch(listIt);
        return;
    }

    // Evict if at capacity.
    if (impl_->lruList.size() >= impl_->config.maxEntries) {
        impl_->evictLru();
    }

    // Insert at front.
    CacheEntry entry;
    entry.sql = key;
    entry.result = result;
    entry.expiresAt = std::chrono::steady_clock::now() + ttl;

    impl_->lruList.push_front(std::move(entry));
    impl_->index[key] = impl_->lruList.begin();
}

// ── invalidate() ────────────────────────────────────────────────────────────

bool QueryCache::invalidate(std::string_view sql) {
    std::lock_guard lock(impl_->mutex);

    auto it = impl_->index.find(std::string(sql));
    if (it == impl_->index.end()) {
        return false;
    }

    impl_->lruList.erase(it->second);
    impl_->index.erase(it);
    return true;
}

// ── invalidateByTable() ─────────────────────────────────────────────────────

std::size_t QueryCache::invalidateByTable(std::string_view tableName) {
    std::lock_guard lock(impl_->mutex);

    std::size_t count = 0;

    for (auto it = impl_->lruList.begin(); it != impl_->lruList.end();) {
        // Case-insensitive substring search for the table name in the SQL.
        // This is conservative: may invalidate queries that merely mention
        // the table name in comments or string literals, but guarantees
        // we never serve stale data.
        bool found = false;
        const auto& sql = it->sql;

        // Simple case-insensitive search.
        auto sqlLower = sql;
        auto tableLower = std::string(tableName);
        std::transform(sqlLower.begin(), sqlLower.end(), sqlLower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        std::transform(tableLower.begin(), tableLower.end(), tableLower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (sqlLower.find(tableLower) != std::string::npos) {
            found = true;
        }

        if (found) {
            impl_->index.erase(it->sql);
            it = impl_->lruList.erase(it);
            ++count;
        } else {
            ++it;
        }
    }

    return count;
}

// ── clear() ─────────────────────────────────────────────────────────────────

void QueryCache::clear() {
    std::lock_guard lock(impl_->mutex);
    impl_->lruList.clear();
    impl_->index.clear();
}

// ── Accessors ───────────────────────────────────────────────────────────────

std::size_t QueryCache::size() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->lruList.size();
}

uint64_t QueryCache::hitCount() const {
    return impl_->hits.load(std::memory_order_relaxed);
}

uint64_t QueryCache::missCount() const {
    return impl_->misses.load(std::memory_order_relaxed);
}

double QueryCache::hitRate() const {
    auto h = impl_->hits.load(std::memory_order_relaxed);
    auto m = impl_->misses.load(std::memory_order_relaxed);
    auto total = h + m;
    if (total == 0) {
        return 0.0;
    }
    return static_cast<double>(h) / static_cast<double>(total);
}

const CacheConfig& QueryCache::config() const noexcept {
    return impl_->config;
}

} // namespace cgs::service
