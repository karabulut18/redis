#pragma once

#include "HashMap.h"
#include "str_hash.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Represents a single key-value entry in the database.
// Uses intrusive HNode for HashMap storage.
struct Entry
{
    HNode hashNode;
    std::string key;
    std::string value;
    int64_t expiresAt = -1; // millisecond timestamp, -1 = no expiry

    Entry(const std::string& k, const std::string& v);

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

    // SET key value — inserts or overwrites. Returns true if new, false if updated.
    // ttlMs: optional TTL in milliseconds (-1 = no expiry)
    bool set(const std::string& key, const std::string& value, int64_t ttlMs = -1);

    // GET key — returns pointer to value if found (and not expired), nullptr otherwise.
    const std::string* get(const std::string& key);

    // DEL key — removes entry. Returns true if key existed (even if expired).
    bool del(const std::string& key);

    // EXPIRE key — set expiry in milliseconds from now. Returns true if key exists.
    bool expire(const std::string& key, int64_t ttlMs);

    // PERSIST key — remove expiry. Returns true if key exists and had an expiry.
    bool persist(const std::string& key);

    // TTL key — returns remaining time in milliseconds.
    // Returns -1 if no expiry, -2 if key doesn't exist.
    int64_t pttl(const std::string& key);

    // EXISTS key — returns true if key exists and is not expired.
    bool exists(const std::string& key);

    // KEYS pattern — returns all keys matching pattern (* = all).
    std::vector<std::string> keys(const std::string& pattern);

    // RENAME key newkey — renames key. Returns false if key doesn't exist.
    bool rename(const std::string& key, const std::string& newkey);

    size_t size() const
    {
        return _size;
    }

private:
    // Find entry (returns nullptr if not found or expired, auto-deletes expired)
    Entry* findEntry(const std::string& key);

    // Find entry without expiry check (raw lookup)
    Entry* findEntryRaw(const std::string& key);

    // Remove and delete an entry by its HNode
    void removeEntry(Entry* entry);

    HashMap _map;
    size_t _size = 0;
};
