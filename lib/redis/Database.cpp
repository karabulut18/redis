#include "Database.h"
#include <cstddef>

// --- Time utility ---

int64_t currentTimeMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// --- Entry ---

Entry::Entry(const std::string& k, const std::string& v) : key(k), value(v)
{
    hashNode.next = nullptr;
    hashNode.code = str_hash(key);
}

bool Entry::isExpired() const
{
    return hasExpiry() && currentTimeMs() >= expiresAt;
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

// --- Database internals ---

Entry* Database::findEntryRaw(const std::string& key)
{
    LookupKey lk(key);
    HNode* found = _map.lookup(&lk.hashNode, LookupKey::cmp);
    return Entry::fromHash(found);
}

Entry* Database::findEntry(const std::string& key)
{
    Entry* entry = findEntryRaw(key);
    if (!entry)
        return nullptr;

    // Lazy expiration: if expired, delete and return null
    if (entry->isExpired())
    {
        removeEntry(entry);
        return nullptr;
    }
    return entry;
}

void Database::removeEntry(Entry* entry)
{
    LookupKey lk(entry->key);
    HNode* removed = _map.remove(&lk.hashNode, LookupKey::cmp);
    if (removed)
    {
        delete Entry::fromHash(removed);
        _size--;
    }
}

// --- Database public API ---

Database::~Database()
{
    _map.clear();
}

bool Database::set(const std::string& key, const std::string& value, int64_t ttlMs)
{
    Entry* existing = findEntryRaw(key);

    if (existing)
    {
        existing->value = value;
        existing->expiresAt = (ttlMs >= 0) ? currentTimeMs() + ttlMs : -1;
        return false;
    }

    Entry* entry = new Entry(key, value);
    if (ttlMs >= 0)
        entry->expiresAt = currentTimeMs() + ttlMs;
    _map.insert(&entry->hashNode);
    _size++;
    return true;
}

const std::string* Database::get(const std::string& key)
{
    Entry* entry = findEntry(key); // lazy expiration
    return entry ? &entry->value : nullptr;
}

bool Database::del(const std::string& key)
{
    Entry* entry = findEntryRaw(key); // delete even if expired
    if (!entry)
        return false;

    removeEntry(entry);
    return true;
}

bool Database::expire(const std::string& key, int64_t ttlMs)
{
    Entry* entry = findEntry(key);
    if (!entry)
        return false;

    entry->expiresAt = currentTimeMs() + ttlMs;
    return true;
}

bool Database::persist(const std::string& key)
{
    Entry* entry = findEntry(key);
    if (!entry || !entry->hasExpiry())
        return false;

    entry->expiresAt = -1;
    return true;
}

int64_t Database::pttl(const std::string& key)
{
    Entry* entry = findEntryRaw(key);
    if (!entry || entry->isExpired())
        return -2; // key does not exist

    if (!entry->hasExpiry())
        return -1; // no expiry set

    int64_t remaining = entry->expiresAt - currentTimeMs();
    return remaining > 0 ? remaining : 0;
}
