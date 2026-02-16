#include "Database.h"
#include <cstddef>

// --- Entry ---

Entry::Entry(const std::string& k, const std::string& v) : key(k), value(v)
{
    hashNode.next = nullptr;
    hashNode.code = str_hash(key);
}

Entry* Entry::fromHash(HNode* node)
{
    if (!node)
        return nullptr;
    return reinterpret_cast<Entry*>(reinterpret_cast<char*>(node) - offsetof(Entry, hashNode));
}

// --- LookupKey ---

LookupKey::LookupKey(const std::string& k) : key(k)
{
    hashNode.next = nullptr;
    hashNode.code = str_hash(key);
}

bool LookupKey::cmp(HNode* entryNode, HNode* keyNode)
{
    Entry* entry = Entry::fromHash(entryNode);
    LookupKey* lookup = reinterpret_cast<LookupKey*>(reinterpret_cast<char*>(keyNode) - offsetof(LookupKey, hashNode));
    return entry->key == lookup->key;
}

// --- Database ---

Database::~Database()
{
    // TODO: iterate and delete all entries
    // For now, the HashMap::clear() only frees the table, not the nodes
    _map.clear();
}

bool Database::set(const std::string& key, const std::string& value)
{
    // Check if key already exists
    LookupKey lk(key);
    HNode* found = _map.lookup(&lk.hashNode, LookupKey::cmp);

    if (found)
    {
        // Update existing entry
        Entry* entry = Entry::fromHash(found);
        entry->value = value;
        return false;
    }

    // Insert new entry
    Entry* entry = new Entry(key, value);
    _map.insert(&entry->hashNode);
    _size++;
    return true;
}

const std::string* Database::get(const std::string& key)
{
    LookupKey lk(key);
    HNode* found = _map.lookup(&lk.hashNode, LookupKey::cmp);

    if (!found)
        return nullptr;

    return &Entry::fromHash(found)->value;
}

bool Database::del(const std::string& key)
{
    LookupKey lk(key);
    HNode* removed = _map.remove(&lk.hashNode, LookupKey::cmp);

    if (!removed)
        return false;

    Entry* entry = Entry::fromHash(removed);
    delete entry;
    _size--;
    return true;
}
