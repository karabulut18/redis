#pragma once

#include "HashMap.h"
#include "IDataVisitor.h"
#include "ZSet.h"
#include "str_hash.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

enum class EntryType
{
    NONE,
    STRING,
    ZSET,
    HASH,
    LIST,
    SET
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
    std::string value;                              // For STRING
    ZSet* zset = nullptr;                           // For ZSET
    HashMap* hash = nullptr;                        // For HASH
    std::deque<std::string>* list = nullptr;        // For LIST
    std::unordered_set<std::string>* set = nullptr; // For SET
    int64_t expiresAt = -1;                         // millisecond timestamp, -1 = no expiry

    Entry(std::string k, std::string v);
    Entry(std::string k, ZSet* z);
    Entry(std::string k, HashMap* h);
    Entry(std::string k, std::deque<std::string>* l);
    Entry(std::string k, std::unordered_set<std::string>* s);
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
    std::string_view key;

    explicit LookupKey(std::string_view k);

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
    bool set(std::string_view key, std::string_view value, int64_t ttlMs = -1);

    // GET key — returns pointer to value if found (and not expired), nullptr otherwise.
    const std::string* get(std::string_view key);

    // INCR key — increments value by 1. Returns new value.
    int64_t incr(std::string_view key);

    // INCRBY key increment — increments value by increment. Returns new value.
    int64_t incrby(std::string_view key, int64_t increment);

    // DECR key — decrements value by 1. Returns new value.
    int64_t decr(std::string_view key);

    // DECRBY key decrement — decrements value by decrement. Returns new value.
    int64_t decrby(std::string_view key, int64_t decrement);

    // --- ZSet Commands ---

    // ZADD key score member
    bool zadd(std::string_view key, double score, std::string_view member);

    // ZREM key member
    bool zrem(std::string_view key, std::string_view member);

    // ZCARD key
    int64_t zcard(std::string_view key);

    // ZSCORE key member
    std::optional<double> zscore(std::string_view key, std::string_view member);

    // ZRANK key member
    std::optional<int64_t> zrank(std::string_view key, std::string_view member);

    // ZRANGE key start stop
    struct ZRangeResult
    {
        std::string_view member;
        double score;
    };
    std::vector<ZRangeResult> zrange(std::string_view key, int64_t start, int64_t stop);

    // ZRANGEBYSCORE key min max
    std::vector<ZRangeResult> zrangebyscore(std::string_view key, double min, double max);

    // --- Hash Commands ---

    // Returns 1 if field is new and value was set.
    // Returns 0 if field already existed and was updated.
    // Returns -1 if WRONGTYPE.
    int hset(std::string_view key, std::string_view field, std::string_view value);

    std::optional<std::string_view> hget(std::string_view key, std::string_view field);

    // Returns 1 if field was removed, 0 if not found. -1 if WRONGTYPE.
    int hdel(std::string_view key, std::string_view field);

    int64_t hlen(std::string_view key);

    struct HGetAllResult
    {
        std::string field;
        std::string value;
    };
    std::vector<HGetAllResult> hgetall(std::string_view key);

    // --- List Commands ---

    // Returns the length of the list after the push operation.
    int64_t lpush(std::string_view key, std::string_view value);
    int64_t rpush(std::string_view key, std::string_view value);

    // Returns popped value or std::nullopt if list is empty or key not found.
    std::optional<std::string> lpop(std::string_view key);
    std::optional<std::string> rpop(std::string_view key);

    // Returns list length. 0 if key does not exist.
    int64_t llen(std::string_view key);

    // Returns list elements in range [start, stop].
    std::vector<std::string> lrange(std::string_view key, int64_t start, int64_t stop);

    // --- Set Commands ---

    // Returns 1 if member was added, 0 if it was already present.
    int sadd(std::string_view key, std::string_view member);

    // Returns 1 if member was removed, 0 if it was not present.
    int srem(std::string_view key, std::string_view member);

    // Returns 1 if member is present, 0 if not.
    int sismember(std::string_view key, std::string_view member);

    // Returns all members.
    std::vector<std::string> smembers(std::string_view key);

    // Returns set cardinality (size).
    int64_t scard(std::string_view key);

    // --- Key Management ---

    // DEL key — removes key. Returns true if removed, false if not found.
    bool del(std::string_view key);

    // EXPIRE key — set expiry in milliseconds from now.
    bool expire(std::string_view key, int64_t ttlMs);

    // PERSIST key — remove expiry.
    bool persist(std::string_view key);

    // TTL key — returns remaining time.
    int64_t pttl(std::string_view key);

    // EXISTS key — returns true if key exists.
    bool exists(std::string_view key);

    // TYPE key — returns key type.
    EntryType getType(std::string_view key);

    // KEYS pattern — returns all keys matching pattern (* = all).
    std::vector<std::string> keys(std::string_view pattern);

    // RENAME key newkey — renames key.
    bool rename(std::string_view key, std::string_view newkey);

    size_t size() const
    {
        return _size;
    }

    // --- Persistence & State ---

    // Clears all data from the database
    void clear();

    // Use Visitor pattern to scan database state.
    void accept(IDataVisitor& visitor);

private:
    // Find entry (returns nullptr if not found, WRONGTYPE, or expired)
    Entry* findEntry(std::string_view key, std::optional<EntryType> expectedType = std::nullopt);

    // Raw lookup without expiry or type check
    Entry* findEntryRaw(std::string_view key);

    void removeEntry(Entry* entry);

    HashMap _map;
    size_t _size = 0;
};
