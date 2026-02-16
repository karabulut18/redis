#include "Database.h"
#include <cstddef>

// --- Time utility ---

int64_t currentTimeMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// --- Entry ---

Entry::Entry(const std::string& k, const std::string& v) : key(k), type(EntryType::STRING), value(v)
{
    hashNode.next = nullptr;
    hashNode.code = str_hash(key);
}

Entry::Entry(const std::string& k, ZSet* z) : key(k), type(EntryType::ZSET), zset(z)
{
    hashNode.next = nullptr;
    hashNode.code = str_hash(key);
}

Entry::Entry(const std::string& k, HashMap* h) : key(k), type(EntryType::HASH), hash(h)
{
    hashNode.next = nullptr;
    hashNode.code = str_hash(key);
}

Entry::~Entry()
{
    if (type == EntryType::ZSET && zset)
    {
        delete zset;
    }
    if (type == EntryType::HASH && hash)
    {
        // Safely delete all HEntries
        auto safelyDeleteEntries = [](HashTable& ht)
        {
            for (size_t i = 0; !ht.empty() && i <= ht.mask(); ++i)
            {
                HNode* node = ht.bucketAt(i);
                while (node)
                {
                    HNode* next = node->next;
                    delete HEntry::fromHash(node);
                    node = next;
                }
            }
        };

        safelyDeleteEntries(const_cast<HashTable&>(hash->newer()));
        safelyDeleteEntries(const_cast<HashTable&>(hash->older()));
        delete hash;
    }
    else if (type == EntryType::LIST && list)
    {
        delete list;
    }
    else if (type == EntryType::SET && set)
    {
        delete set;
    }
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

Entry* Database::findEntry(const std::string& key, std::optional<EntryType> expectedType)
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

    if (expectedType && entry->type != *expectedType)
        return nullptr; // WRONGTYPE

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
    // Safely delete all entries from both tables
    for (size_t i = 0; !_map.newer().empty() && i <= _map.newer().mask(); ++i)
    {
        HNode* node = _map.newer().bucketAt(i);
        while (node)
        {
            HNode* next = node->next;
            delete Entry::fromHash(node);
            node = next;
        }
    }
    for (size_t i = 0; !_map.older().empty() && i <= _map.older().mask(); ++i)
    {
        HNode* node = _map.older().bucketAt(i);
        while (node)
        {
            HNode* next = node->next;
            delete Entry::fromHash(node);
            node = next;
        }
    }
    _map.clear();
}

bool Database::set(const std::string& key, const std::string& value, int64_t ttlMs)
{
    Entry* existing = findEntryRaw(key);

    if (existing)
    {
        if (existing->type != EntryType::STRING)
        {
            removeEntry(existing);
        }
        else
        {
            existing->value = value;
            existing->expiresAt = (ttlMs >= 0) ? currentTimeMs() + ttlMs : -1;
            return false;
        }
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
    Entry* entry = findEntry(key, EntryType::STRING); // lazy expiration + type check
    return entry ? &entry->value : nullptr;
}

bool Database::zadd(const std::string& key, double score, const std::string& member)
{
    Entry* entry = findEntryRaw(key);
    if (entry && entry->isExpired())
    {
        removeEntry(entry);
        entry = nullptr;
    }

    if (entry && entry->type != EntryType::ZSET)
        return false; // WRONGTYPE

    if (!entry)
    {
        ZSet* z = new ZSet();
        z->insert(member, score);
        entry = new Entry(key, z);
        _map.insert(&entry->hashNode);
        _size++;
        return true;
    }

    return entry->zset->insert(member, score);
}

bool Database::zrem(const std::string& key, const std::string& member)
{
    Entry* entry = findEntry(key, EntryType::ZSET);
    if (!entry)
        return false;

    ZNode* znode = entry->zset->lookUp(member);
    if (!znode)
        return false;

    entry->zset->remove(znode);
    if (entry->zset->size() == 0)
    {
        removeEntry(entry);
    }
    return true;
}

int64_t Database::zcard(const std::string& key)
{
    Entry* entry = findEntry(key, EntryType::ZSET);
    return entry ? static_cast<int64_t>(entry->zset->size()) : 0;
}

std::optional<double> Database::zscore(const std::string& key, const std::string& member)
{
    Entry* entry = findEntry(key, EntryType::ZSET);
    if (!entry)
        return std::nullopt;

    ZNode* znode = entry->zset->lookUp(member);
    return znode ? std::optional<double>(znode->score) : std::nullopt;
}

std::vector<Database::ZRangeResult> Database::zrange(const std::string& key, int64_t start, int64_t stop)
{
    Entry* entry = findEntry(key, EntryType::ZSET);
    if (!entry)
        return {};

    int64_t n = static_cast<int64_t>(entry->zset->size());
    if (start < 0)
        start += n;
    if (stop < 0)
        stop += n;
    if (start < 0)
        start = 0;
    if (stop >= n)
        stop = n - 1;
    if (start > stop || start >= n)
        return {};

    std::vector<ZRangeResult> result;
    ZNode* znode = ZNode::fromTree(AVLNode::findMin(entry->zset->tree().root()));
    znode = ZNode::offset(znode, start);

    for (int64_t i = start; i <= stop && znode; i++)
    {
        result.push_back({znode->name, znode->score});
        znode = ZNode::fromTree(AVLNode::successor(&znode->treeNode));
    }
    return result;
}

std::vector<Database::ZRangeResult> Database::zrangebyscore(const std::string& key, double min, double max)
{
    Entry* entry = findEntry(key, EntryType::ZSET);
    if (!entry)
        return {};

    std::vector<ZRangeResult> result;
    ZNode* znode = entry->zset->seekGe(min, ""); // Empty string as sentinel member
    while (znode && znode->score <= max)
    {
        result.push_back({znode->name, znode->score});
        znode = ZNode::fromTree(AVLNode::successor(&znode->treeNode));
    }
    return result;
}

// --- Hash Commands ---

int Database::hset(const std::string& key, const std::string& field, const std::string& value)
{
    Entry* entry = findEntryRaw(key);
    if (entry && entry->type != EntryType::HASH)
        return -1; // WRONGTYPE

    if (!entry)
    {
        entry = new Entry(key, new HashMap());
        _map.insert(&entry->hashNode);
        _size++;
    }

    HEntry keyEntry;
    keyEntry.key = field;
    keyEntry.node.code = str_hash(field);

    HNode* node = entry->hash->lookup(&keyEntry.node, [&](HNode* existing, HNode* k)
                                      { return HEntry::fromHash(existing)->key == field; });

    if (node)
    {
        HEntry* existing = HEntry::fromHash(node);
        existing->value = value;
        return 0; // Updated
    }
    else
    {
        HEntry* newEntry = new HEntry();
        newEntry->key = field;
        newEntry->value = value;
        newEntry->node.code = str_hash(field);
        newEntry->node.next = nullptr;
        entry->hash->insert(&newEntry->node);
        return 1; // New
    }
}

std::optional<std::string_view> Database::hget(const std::string& key, const std::string& field)
{
    Entry* entry = findEntry(key, EntryType::HASH);
    if (!entry)
        return std::nullopt;

    HEntry keyEntry;
    keyEntry.key = field;
    keyEntry.node.code = str_hash(field);

    HNode* node = entry->hash->lookup(&keyEntry.node, [&](HNode* existing, HNode* k)
                                      { return HEntry::fromHash(existing)->key == field; });

    if (node)
        return HEntry::fromHash(node)->value;
    return std::nullopt;
}

int Database::hdel(const std::string& key, const std::string& field)
{
    Entry* entry = findEntry(key, EntryType::HASH);
    if (!entry)
        return 0; // Key not found or WRONGTYPE (handled by findEntry returning null for WRONGTYPE?)
                  // Wait, findEntry returns null for WRONGTYPE only if I requested expected type.
                  // I requested EntryType::HASH. So if it's ZSET, it returns null.
                  // But standard Redis HDEL returns 0 if key doesn't exist AND should return error if WRONGTYPE.
                  // My findEntry swallows WRONGTYPE into null.
                  // I should check WRONGTYPE explicitly if I want strict return codes.

    // Let's check explicitly for WRONGTYPE compliance
    Entry* raw = findEntryRaw(key);
    if (raw && raw->type != EntryType::HASH)
        return -1; // WRONGTYPE

    if (!raw)
        return 0; // Key doesn't exist

    HEntry keyEntry;
    keyEntry.key = field;
    keyEntry.node.code = str_hash(field);

    HNode* node = raw->hash->remove(&keyEntry.node, [&](HNode* existing, HNode* k)
                                    { return HEntry::fromHash(existing)->key == field; });

    if (node)
    {
        delete HEntry::fromHash(node);
        if (raw->hash->newer().size() == 0 && raw->hash->older().size() == 0) // crude empty check
        {
            // If empty, remove the key? Redis does this.
            // Helper specific to Database::removeEntry(raw);
            removeEntry(raw);
        }
        return 1;
    }
    return 0;
}

int64_t Database::hlen(const std::string& key)
{
    // Again, findEntry swallows WRONGTYPE.
    Entry* raw = findEntryRaw(key);
    if (raw && raw->type != EntryType::HASH)
        return -1; // Error indicator? Or 0? Redis HLEN returns 0 for non-existent. Returns error for WRONGTYPE.
                   // I'll return -1 because int64_t allows it. Client should handle it.

    if (!raw)
        return 0;

    // Hash size is sum of both tables
    return raw->hash->newer().size() + raw->hash->older().size();
}

std::vector<Database::HGetAllResult> Database::hgetall(const std::string& key)
{
    Entry* raw = findEntryRaw(key);
    if (raw && raw->type != EntryType::HASH)
        return {}; // WRONGTYPE -> empty list? Redis returns error. Client should check getType?
                   // No, I should return valid data or empty.
                   // Ideally client checks type before calling or I throw/return error.
                   // For now, returning empty is "safe" but hides errors.
                   // But wait, findEntry(key, HASH) returns null on wrong type.
                   // So I can't distinguish "missing" from "wrong type" easily with findEntry(type).

    if (!raw || raw->type != EntryType::HASH)
        return {};

    std::vector<HGetAllResult> result;

    auto collect = [&](const HashTable& ht)
    {
        for (size_t i = 0; !ht.empty() && i <= ht.mask(); ++i)
        {
            HNode* node = ht.bucketAt(i);
            while (node)
            {
                HEntry* he = HEntry::fromHash(node);
                result.push_back({he->key, he->value});
                node = node->next;
            }
        }
    };

    collect(raw->hash->newer());
    collect(raw->hash->older());
    return result;
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

bool Database::exists(const std::string& key)
{
    return findEntry(key) != nullptr;
}

EntryType Database::getType(const std::string& key)
{
    Entry* entry = findEntry(key);
    if (!entry)
        return EntryType::STRING; // Default for non-existent in my current logic, but TYPE command will handle it
                                  // correctly via exists() check
    return entry->type;
}

// Simple glob matching: supports "*", "prefix*", "*suffix", "pre*suf"
static bool matchPattern(const std::string& pattern, const std::string& str)
{
    if (pattern == "*")
        return true;

    size_t star = pattern.find('*');
    if (star == std::string::npos)
        return pattern == str; // exact match

    std::string prefix = pattern.substr(0, star);
    std::string suffix = pattern.substr(star + 1);

    if (str.size() < prefix.size() + suffix.size())
        return false;
    if (str.compare(0, prefix.size(), prefix) != 0)
        return false;
    if (!suffix.empty() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) != 0)
        return false;
    return true;
}

std::vector<std::string> Database::keys(const std::string& pattern)
{
    std::vector<std::string> result;

    HT_FOREACH(_map.newer(), node)
    {
        Entry* entry = Entry::fromHash(node);
        if (!entry->isExpired() && matchPattern(pattern, entry->key))
            result.push_back(entry->key);
    }
    HT_FOREACH(_map.older(), node)
    {
        Entry* entry = Entry::fromHash(node);
        if (!entry->isExpired() && matchPattern(pattern, entry->key))
            result.push_back(entry->key);
    }

    return result;
}

bool Database::rename(const std::string& key, const std::string& newkey)
{
    Entry* entry = findEntry(key);
    if (!entry)
        return false;

    // If newkey already exists, delete it first
    del(newkey);

    // Remove old key from map, update key+hash, reinsert
    LookupKey lk(key);
    HNode* removed = _map.remove(&lk.hashNode, LookupKey::cmp);
    if (!removed)
        return false;

    Entry* e = Entry::fromHash(removed);
    e->key = newkey;
    e->hashNode.code = str_hash(newkey);
    e->hashNode.next = nullptr;
    _map.insert(&e->hashNode);
    // _size stays the same (or decremented by del(newkey) above)
    return true;
}

Entry::Entry(const std::string& k, std::deque<std::string>* l) : key(k), type(EntryType::LIST), list(l)
{
    hashNode.next = nullptr;
    hashNode.code = str_hash(key);
}

// NOTE: Destructor logic for LIST is added via replace_file_content in the next step to inject into existing
// destructor.

// --- List Commands ---

int64_t Database::lpush(const std::string& key, const std::string& value)
{
    Entry* entry = findEntryRaw(key);
    if (entry && entry->type != EntryType::LIST)
        return -1; // Wrong type

    if (!entry)
    {
        auto list = new std::deque<std::string>();
        entry = new Entry(key, list);
        _map.insert(&entry->hashNode);
        _size++;
    }

    entry->list->push_front(value);
    return entry->list->size();
}

int64_t Database::rpush(const std::string& key, const std::string& value)
{
    Entry* entry = findEntryRaw(key);
    if (entry && entry->type != EntryType::LIST)
        return -1;

    if (!entry)
    {
        auto list = new std::deque<std::string>();
        entry = new Entry(key, list);
        _map.insert(&entry->hashNode);
        _size++;
    }

    entry->list->push_back(value);
    return entry->list->size();
}

std::optional<std::string> Database::lpop(const std::string& key)
{
    Entry* entry = findEntry(key, EntryType::LIST);
    if (!entry || entry->list->empty())
        return std::nullopt;

    std::string val = entry->list->front();
    entry->list->pop_front();

    if (entry->list->empty())
    {
        removeEntry(entry);
    }

    return val;
}

std::optional<std::string> Database::rpop(const std::string& key)
{
    Entry* entry = findEntry(key, EntryType::LIST);
    if (!entry || entry->list->empty())
        return std::nullopt;

    std::string val = entry->list->back();
    entry->list->pop_back();

    if (entry->list->empty())
    {
        removeEntry(entry);
    }

    return val;
}

int64_t Database::llen(const std::string& key)
{
    Entry* entry = findEntry(key, EntryType::LIST);
    if (!entry)
        return 0;
    return entry->list->size();
}

std::vector<std::string> Database::lrange(const std::string& key, int64_t start, int64_t stop)
{
    Entry* entry = findEntry(key, EntryType::LIST);
    if (!entry)
        return {};

    int64_t size = entry->list->size();

    // Normalize indices
    if (start < 0)
        start = size + start;
    if (stop < 0)
        stop = size + stop;

    if (start < 0)
        start = 0;

    // In Redis, stop is inclusive. If stop is beyond size, limit it.
    if (stop >= size)
        stop = size - 1;

    if (start > stop || start >= size)
        return {};

    std::vector<std::string> result;
    result.reserve(stop - start + 1);

    for (int64_t i = start; i <= stop; ++i)
    {
        result.push_back(entry->list->at(i));
    }

    return result;
}

Entry::Entry(const std::string& k, std::unordered_set<std::string>* s) : key(k), type(EntryType::SET), set(s)
{
    hashNode.next = nullptr;
    hashNode.code = str_hash(key);
}

// NOTE: Destructor cleanup for SET is handled via replace_file_content in the next step.

// --- Set Commands ---

int Database::sadd(const std::string& key, const std::string& member)
{
    Entry* entry = findEntryRaw(key);
    if (entry && entry->type != EntryType::SET)
        return -1; // Wrong type

    if (!entry)
    {
        auto s = new std::unordered_set<std::string>();
        entry = new Entry(key, s);
        _map.insert(&entry->hashNode);
        _size++;
    }

    auto result = entry->set->insert(member);
    return result.second ? 1 : 0;
}

int Database::srem(const std::string& key, const std::string& member)
{
    Entry* entry = findEntry(key, EntryType::SET);
    if (!entry)
        return 0;

    size_t removed = entry->set->erase(member);
    if (entry->set->empty())
    {
        removeEntry(entry);
    }
    return static_cast<int>(removed);
}

int Database::sismember(const std::string& key, const std::string& member)
{
    Entry* entry = findEntry(key, EntryType::SET);
    if (!entry)
        return 0;

    return entry->set->count(member) > 0 ? 1 : 0;
}

std::vector<std::string> Database::smembers(const std::string& key)
{
    Entry* entry = findEntry(key, EntryType::SET);
    if (!entry)
        return {};

    std::vector<std::string> members;
    members.reserve(entry->set->size());
    for (const auto& m : *entry->set)
    {
        members.push_back(m);
    }
    return members;
}

int64_t Database::scard(const std::string& key)
{
    Entry* entry = findEntry(key, EntryType::SET);
    if (!entry)
        return 0;

    return entry->set->size();
}
