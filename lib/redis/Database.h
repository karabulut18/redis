#pragma once

#include "HashMap.h"
#include "ZSet.h"
#include "str_hash.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class EntryType
{
    STRING,
    ZSET,
    HASH
};

struct HEntry
{
    HNode node;
    std::string key;
    std::string value;

    // Helper to get HEntry from HNode
    static HEntry* fromHash(HNode* node)
    {
        if (!node)
            return nullptr;
        return reinterpret_cast<HEntry*>(reinterpret_cast<char*>(node) - offsetof(HEntry, node));
    }
};

// Represents a single key-value entry in the database.
// Uses intrusive HNode for HashMap storage.
struct Entry
{
    HNode hashNode;
    std::string key;
    EntryType type;
    std::string value;       // For STRING
    ZSet* zset = nullptr;    // For ZSET
    HashMap* hash = nullptr; // For HASH
    int64_t expiresAt = -1;  // millisecond timestamp, -1 = no expiry

    Entry(const std::string& k, const std::string& v);
    Entry(const std::string& k, ZSet* z);
    Entry(const std::string& k, HashMap* h);
    ~Entry();

    bool hasExpiry() const
    {
        return expiresAt >= 0;
    }
    bool isExpired() const;

    // Recover Entry pointer from its embedded HNode
    static Entry* fromHash(HNode* node);
};

// A lookup-only key for HashMap queries (avoids
// constructing a full Entry just to search).
struct LookupKey
{
    HNode hashNode;
    std::string key;

    explicit LookupKey(const std::string& k);

    // Compares an Entry's HNode against a LookupKey's HNode
    static bool cmp(HNode* entryNode, HNode* keyNode);
};

// Returns current time in milliseconds (monotonic clock)
int64_t currentTimeMs();

// The core key-value database.
// Supports GET, SET, DEL, TTL operations on string values.
class Database
{
public:
    Database() = default;
    ~Database();

    // --- String Commands ---

    // SET key value — inserts or overwrites. Returns true if new, false if updated.
    bool set(const std::string& key, const std::string& value, int64_t ttlMs = -1);

    // GET key — returns pointer to value if found (and not expired), nullptr otherwise.
    const std::string* get(const std::string& key);

    // --- ZSet Commands ---

    // ZADD key score member
    bool zadd(const std::string& key, double score, const std::string& member);

    // ZREM key member
    bool zrem(const std::string& key, const std::string& member);

    // ZCARD key
    int64_t zcard(const std::string& key);

    // ZSCORE key member
    std::optional<double> zscore(const std::string& key, const std::string& member);

    // ZRANGE key start stop
    struct ZRangeResult
    {
        std::string_view member;
        double score;
    };
    std::vector<ZRangeResult> zrange(const std::string& key, int64_t start, int64_t stop);

    // ZRANGEBYSCORE key min max
    std::vector<ZRangeResult> zrangebyscore(const std::string& key, double min, double max);

    // --- Hash Commands ---

    // Returns 1 if field is new and value was set.
    // Returns 0 if field already existed and was updated.
    // Returns -1 if WRONGTYPE.
    int hset(const std::string& key, const std::string& field, const std::string& value);

    std::optional<std::string_view> hget(const std::string& key, const std::string& field);

    // Returns 1 if field was removed, 0 if not found. -1 if WRONGTYPE.
    int hdel(const std::string& key, const std::string& field);

    int64_t hlen(const std::string& key);

    struct HGetAllResult
    {
        std::string field;
        std::string value;
    };
    std::vector<HGetAllResult> hgetall(const std::string& key);

    // --- Key Management ---

    // DEL key — removes entry. Returns true if key existed.
    bool del(const std::string& key);

    // EXPIRE key — set expiry in milliseconds from now.
    bool expire(const std::string& key, int64_t ttlMs);

    // PERSIST key — remove expiry.
    bool persist(const std::string& key);

    // TTL key — returns remaining time.
    int64_t pttl(const std::string& key);

    // EXISTS key — returns true if key exists.
    bool exists(const std::string& key);

    // TYPE key — returns key type.
    EntryType getType(const std::string& key);

    // KEYS pattern — returns all keys matching pattern (* = all).
    std::vector<std::string> keys(const std::string& pattern);

    // RENAME key newkey — renames key.
    bool rename(const std::string& key, const std::string& newkey);

    size_t size() const
    {
        return _size;
    }

private:
    // Find entry (returns nullptr if not found, WRONGTYPE, or expired)
    Entry* findEntry(const std::string& key, std::optional<EntryType> expectedType = std::nullopt);

    // Raw lookup without expiry or type check
    Entry* findEntryRaw(const std::string& key);

    void removeEntry(Entry* entry);

    HashMap _map;
    size_t _size = 0;
};
