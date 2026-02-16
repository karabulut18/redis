#pragma once

#include "HashMap.h"
#include "str_hash.h"
#include <cstddef>
#include <string>

// Represents a single key-value entry in the database.
// Uses intrusive HNode for HashMap storage.
struct Entry
{
    HNode hashNode;
    std::string key;
    std::string value;

    Entry(const std::string& k, const std::string& v);

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

// The core key-value database.
// Supports GET, SET, DEL operations on string values.
class Database
{
public:
    Database() = default;
    ~Database();

    // SET key value — inserts or overwrites. Returns true if new, false if updated.
    bool set(const std::string& key, const std::string& value);

    // GET key — returns pointer to value if found, nullptr otherwise.
    const std::string* get(const std::string& key);

    // DEL key — removes entry. Returns true if key existed.
    bool del(const std::string& key);

    // Number of keys in the database
    size_t size() const
    {
        return _size;
    }

private:
    HashMap _map;
    size_t _size = 0;
};
