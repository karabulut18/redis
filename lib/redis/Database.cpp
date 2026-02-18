#include "Database.h"
#include <cstddef>

// --- Time utility ---

int64_t currentTimeMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// --- Entry ---

Entry::Entry(std::string k, std::string v) : key(std::move(k)), type(EntryType::STRING), value(std::move(v))
{
    hashNode.next = nullptr;
    hashNode.code = str_hash(key);
}

Entry::Entry(std::string k, ZSet* z) : key(std::move(k)), type(EntryType::ZSET), zset(z)
{
    hashNode.next = nullptr;
    hashNode.code = str_hash(key);
}

Entry::Entry(std::string k, HashMap* h) : key(std::move(k)), type(EntryType::HASH), hash(h)
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

LookupKey::LookupKey(std::string_view k) : key(k)
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

Entry* Database::findEntryRaw(std::string_view key)
{
    LookupKey lk(key);
    HNode* found = _map.lookup(&lk.hashNode, LookupKey::cmp);
    return Entry::fromHash(found);
}

Entry* Database::findEntry(std::string_view key, std::optional<EntryType> expectedType)
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

void Database::clear()
{
    // Safely delete all entries from both tables
    auto clearTable = [&](HashTable& ht)
    {
        for (size_t i = 0; !ht.empty() && i <= ht.mask(); ++i)
        {
            HNode* node = ht.bucketAt(i);
            while (node)
            {
                HNode* next = node->next;
                delete Entry::fromHash(node);
                node = next;
            }
        }
    };

    clearTable(_map.newer());
    clearTable(_map.older());
    _map.clear();
    _size = 0;
}

bool Database::set(std::string_view key, std::string_view value, int64_t ttlMs)
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
            existing->value = std::string(value);
            existing->expiresAt = (ttlMs >= 0) ? currentTimeMs() + ttlMs : -1;
            return false;
        }
    }

    Entry* entry = new Entry(std::string(key), std::string(value));
    if (ttlMs >= 0)
        entry->expiresAt = currentTimeMs() + ttlMs;
    _map.insert(&entry->hashNode);
    _size++;
    return true;
}

const std::string* Database::get(std::string_view key)
{
    Entry* entry = findEntry(key, EntryType::STRING); // lazy expiration + type check
    return entry ? &entry->value : nullptr;
}

int64_t Database::incr(std::string_view key)
{
    return incrby(key, 1);
}

int64_t Database::incrby(std::string_view key, int64_t increment)
{
    Entry* entry = findEntry(key);
    if (!entry)
    {
        std::string valStr = std::to_string(increment);
        Entry* newEntry = new Entry(std::string(key), std::move(valStr));
        _map.insert(&newEntry->hashNode);
        _size++;
        return increment;
    }

    if (entry->type != EntryType::STRING)
    {
        throw std::runtime_error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    try
    {
        int64_t val = std::stoll(entry->value);
        val += increment;
        entry->value = std::to_string(val);
        return val;
    }
    catch (...)
    {
        throw std::runtime_error("ERR value is not an integer or out of range");
    }
}

int64_t Database::decr(std::string_view key)
{
    return decrby(key, 1);
}

int64_t Database::decrby(std::string_view key, int64_t decrement)
{
    return incrby(key, -decrement);
}

EntryType Database::getType(std::string_view key)
{
    Entry* entry = findEntry(key);
    if (!entry)
        return EntryType::NONE;
    return entry->type;
}

bool Database::zadd(std::string_view key, double score, std::string_view member)
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
        z->insert(std::string(member), score);
        entry = new Entry(std::string(key), z);
        _map.insert(&entry->hashNode);
        _size++;
        return true;
    }

    return entry->zset->insert(std::string(member), score);
}

bool Database::zrem(std::string_view key, std::string_view member)
{
    Entry* entry = findEntry(key, EntryType::ZSET);
    if (!entry)
        return false;

    ZNode* znode = entry->zset->lookUp(member);
    if (!znode)
        return false;

    entry->zset->remove(znode); // Fixed: using remove instead of removeUnsafe
    if (entry->zset->size() == 0)
    {
        removeEntry(entry);
    }
    return true;
}

int64_t Database::zcard(std::string_view key)
{
    Entry* entry = findEntry(key, EntryType::ZSET);
    return entry ? static_cast<int64_t>(entry->zset->size()) : 0;
}

std::optional<double> Database::zscore(std::string_view key, std::string_view member)
{
    Entry* entry = findEntry(key, EntryType::ZSET);
    if (!entry)
        return std::nullopt;

    ZNode* znode = entry->zset->lookUp(member);
    return znode ? std::optional<double>(znode->score) : std::nullopt;
}

std::optional<int64_t> Database::zrank(std::string_view key, std::string_view member)
{
    Entry* entry = findEntry(key, EntryType::ZSET);
    if (!entry)
        return std::nullopt;

    int64_t rank = entry->zset->getRank(member);
    if (rank < 0)
        return std::nullopt;
    return rank;
}

std::vector<Database::ZRangeResult> Database::zrange(std::string_view key, int64_t start, int64_t stop)
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

std::vector<Database::ZRangeResult> Database::zrangebyscore(std::string_view key, double min, double max)
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

int Database::hset(std::string_view key, std::string_view field, std::string_view value)
{
    Entry* entry = findEntryRaw(key);
    if (entry && entry->isExpired())
    {
        removeEntry(entry);
        entry = nullptr;
    }

    if (entry && entry->type != EntryType::HASH)
        return -1; // WRONGTYPE

    if (!entry)
    {
        HashMap* h = new HashMap();
        HEntry* he = new HEntry();
        he->key = std::string(field);
        he->value = std::string(value);
        he->node.code = str_hash(he->key);
        h->insert(&he->node);

        entry = new Entry(std::string(key), h);
        _map.insert(&entry->hashNode);
        _size++;
        return 1;
    }

    // Check if field exists
    HEntry searchHe;
    searchHe.key = std::string(field); // Temporary string for hash and comparison
    searchHe.node.code = str_hash(searchHe.key);
    HNode* found = entry->hash->lookup(&searchHe.node, [](HNode* a, HNode* b)
                                       { return HEntry::fromHash(a)->key == HEntry::fromHash(b)->key; });

    if (found)
    {
        HEntry::fromHash(found)->value = std::string(value);
        return 0;
    }

    HEntry* he = new HEntry();
    he->key = std::string(field);
    he->value = std::string(value);
    he->node.code = str_hash(he->key);
    entry->hash->insert(&he->node);
    return 1;
}

std::optional<std::string_view> Database::hget(std::string_view key, std::string_view field)
{
    Entry* entry = findEntry(key, EntryType::HASH);
    if (!entry)
        return std::nullopt;

    HEntry searchHe;
    searchHe.key = std::string(field); // Temporary string for hash and comparison
    searchHe.node.code = str_hash(searchHe.key);
    HNode* found = entry->hash->lookup(&searchHe.node, [](HNode* a, HNode* b)
                                       { return HEntry::fromHash(a)->key == HEntry::fromHash(b)->key; });

    if (!found)
        return std::nullopt;
    return std::string_view(HEntry::fromHash(found)->value);
}

int Database::hdel(std::string_view key, std::string_view field)
{
    Entry* entry = findEntry(key, EntryType::HASH);
    if (!entry)
        return 0;

    HEntry searchHe;
    searchHe.key = std::string(field); // Temporary string for hash and comparison
    searchHe.node.code = str_hash(searchHe.key);
    HNode* found = entry->hash->remove(&searchHe.node, [](HNode* a, HNode* b)
                                       { return HEntry::fromHash(a)->key == HEntry::fromHash(b)->key; });

    if (!found)
        return 0;

    delete HEntry::fromHash(found);
    if (entry->hash->newer().size() == 0 && entry->hash->older().size() == 0)
    {
        removeEntry(entry);
    }
    return 1;
}

int64_t Database::hlen(std::string_view key)
{
    Entry* entry = findEntry(key, EntryType::HASH);
    if (!entry)
        return 0;
    return static_cast<int64_t>(entry->hash->newer().size() + entry->hash->older().size());
}

std::vector<Database::HGetAllResult> Database::hgetall(std::string_view key)
{
    Entry* entry = findEntry(key, EntryType::HASH);
    if (!entry)
        return {};

    std::vector<HGetAllResult> result;
    auto& newer = entry->hash->newer();
    HT_FOREACH(newer, node)
    {
        HEntry* he = HEntry::fromHash(node);
        result.push_back({he->key, he->value});
    }
    auto& older = entry->hash->older();
    HT_FOREACH(older, node)
    {
        HEntry* he = HEntry::fromHash(node);
        result.push_back({he->key, he->value});
    }
    return result;
}

// --- List Commands ---

int64_t Database::lpush(std::string_view key, std::string_view value)
{
    Entry* entry = findEntryRaw(key);
    if (entry && entry->isExpired())
    {
        removeEntry(entry);
        entry = nullptr;
    }

    if (entry && entry->type != EntryType::LIST)
        return -1; // WRONGTYPE

    if (!entry)
    {
        std::deque<std::string>* l = new std::deque<std::string>();
        l->push_front(std::string(value));
        entry = new Entry(std::string(key), l);
        _map.insert(&entry->hashNode);
        _size++;
        return 1;
    }

    entry->list->push_front(std::string(value));
    return static_cast<int64_t>(entry->list->size());
}

int64_t Database::rpush(std::string_view key, std::string_view value)
{
    Entry* entry = findEntryRaw(key);
    if (entry && entry->isExpired())
    {
        removeEntry(entry);
        entry = nullptr;
    }

    if (entry && entry->type != EntryType::LIST)
        return -1; // WRONGTYPE

    if (!entry)
    {
        std::deque<std::string>* l = new std::deque<std::string>();
        l->push_back(std::string(value));
        entry = new Entry(std::string(key), l);
        _map.insert(&entry->hashNode);
        _size++;
        return 1;
    }

    entry->list->push_back(std::string(value));
    return static_cast<int64_t>(entry->list->size());
}

std::optional<std::string> Database::lpop(std::string_view key)
{
    Entry* entry = findEntry(key, EntryType::LIST);
    if (!entry || entry->list->empty())
        return std::nullopt;

    std::string val = std::move(entry->list->front());
    entry->list->pop_front();
    if (entry->list->empty())
        removeEntry(entry);
    return val;
}

std::optional<std::string> Database::rpop(std::string_view key)
{
    Entry* entry = findEntry(key, EntryType::LIST);
    if (!entry || entry->list->empty())
        return std::nullopt;

    std::string val = std::move(entry->list->back());
    entry->list->pop_back();
    if (entry->list->empty())
        removeEntry(entry);
    return val;
}

int64_t Database::llen(std::string_view key)
{
    Entry* entry = findEntry(key, EntryType::LIST);
    return entry ? static_cast<int64_t>(entry->list->size()) : 0;
}

std::vector<std::string> Database::lrange(std::string_view key, int64_t start, int64_t stop)
{
    Entry* entry = findEntry(key, EntryType::LIST);
    if (!entry)
        return {};

    int64_t n = static_cast<int64_t>(entry->list->size());
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

    std::vector<std::string> result;
    for (int64_t i = start; i <= stop; i++)
    {
        result.push_back((*entry->list)[i]);
    }
    return result;
}

bool Database::del(std::string_view key)
{
    Entry* entry = findEntryRaw(key); // delete even if expired
    if (!entry)
        return false;

    removeEntry(entry);
    return true;
}

bool Database::expire(std::string_view key, int64_t ttlMs)
{
    Entry* entry = findEntry(key);
    if (!entry)
        return false;

    entry->expiresAt = currentTimeMs() + ttlMs;
    return true;
}

bool Database::persist(std::string_view key)
{
    Entry* entry = findEntry(key);
    if (!entry || !entry->hasExpiry())
        return false;

    entry->expiresAt = -1;
    return true;
}

int64_t Database::pttl(std::string_view key)
{
    Entry* entry = findEntryRaw(key);
    if (!entry || entry->isExpired())
        return -2; // key does not exist

    if (!entry->hasExpiry())
        return -1; // no expiry set

    int64_t remaining = entry->expiresAt - currentTimeMs();
    return remaining > 0 ? remaining : 0;
}

bool Database::exists(std::string_view key)
{
    return findEntry(key) != nullptr;
}
// Full Redis-compatible glob matching: supports *, ?, [abc], [a-z], [^...]
static bool matchPattern(const std::string& pattern, const std::string& str, size_t pi = 0, size_t si = 0)
{
    while (pi < pattern.size())
    {
        char p = pattern[pi];
        if (p == '*')
        {
            // Skip consecutive stars
            while (pi < pattern.size() && pattern[pi] == '*')
                pi++;
            if (pi == pattern.size())
                return true; // trailing * matches everything
            // Try matching the rest of the pattern at each position
            for (size_t i = si; i <= str.size(); i++)
                if (matchPattern(pattern, str, pi, i))
                    return true;
            return false;
        }
        else if (p == '?')
        {
            if (si >= str.size())
                return false;
            pi++;
            si++;
        }
        else if (p == '[')
        {
            if (si >= str.size())
                return false;
            size_t end = pattern.find(']', pi + 1);
            if (end == std::string::npos)
            {
                // Malformed bracket — treat '[' as literal
                if (str[si] != '[')
                    return false;
                pi++;
                si++;
                continue;
            }
            bool negate = (pi + 1 < end && pattern[pi + 1] == '^');
            size_t start = pi + 1 + (negate ? 1 : 0);
            bool matched = false;
            for (size_t j = start; j < end && !matched; j++)
            {
                if (j + 2 < end && pattern[j + 1] == '-')
                {
                    if (str[si] >= pattern[j] && str[si] <= pattern[j + 2])
                        matched = true;
                    j += 2;
                }
                else if (str[si] == pattern[j])
                {
                    matched = true;
                }
            }
            if (matched == negate)
                return false;
            pi = end + 1;
            si++;
        }
        else
        {
            if (si >= str.size() || str[si] != p)
                return false;
            pi++;
            si++;
        }
    }
    return si == str.size();
}

std::vector<std::string> Database::keys(std::string_view pattern)
{
    std::vector<std::string> result;
    std::string patternStr(pattern);

    HT_FOREACH(_map.newer(), node)
    {
        Entry* entry = Entry::fromHash(node);
        if (!entry->isExpired() && matchPattern(patternStr, entry->key))
            result.push_back(entry->key);
    }
    HT_FOREACH(_map.older(), node)
    {
        Entry* entry = Entry::fromHash(node);
        if (!entry->isExpired() && matchPattern(patternStr, entry->key))
            result.push_back(entry->key);
    }

    return result;
}

bool Database::rename(std::string_view key, std::string_view newkey)
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
    e->key = std::string(newkey);
    e->hashNode.code = str_hash(e->key);
    e->hashNode.next = nullptr;
    _map.insert(&e->hashNode);
    // _size stays the same (or decremented by del(newkey) above)
    return true;
}

Entry::Entry(std::string k, std::deque<std::string>* l) : key(std::move(k)), type(EntryType::LIST), list(l)
{
    hashNode.next = nullptr;
    hashNode.code = str_hash(key);
}

// NOTE: Destructor logic for LIST is added via replace_file_content in the next step to inject into existing
// destructor.

Entry::Entry(std::string k, std::unordered_set<std::string>* s) : key(std::move(k)), type(EntryType::SET), set(s)
{
    hashNode.next = nullptr;
    hashNode.code = str_hash(key);
}

// NOTE: Destructor cleanup for SET is handled via replace_file_content in the next step.

// --- Set Commands ---

int Database::sadd(std::string_view key, std::string_view member)
{
    Entry* entry = findEntryRaw(key);
    if (entry && entry->isExpired())
    {
        removeEntry(entry);
        entry = nullptr;
    }

    if (entry && entry->type != EntryType::SET)
        return -1; // WRONGTYPE

    if (!entry)
    {
        std::unordered_set<std::string>* s = new std::unordered_set<std::string>();
        s->insert(std::string(member));
        entry = new Entry(std::string(key), s);
        _map.insert(&entry->hashNode);
        _size++;
        return 1;
    }

    auto res = entry->set->insert(std::string(member));
    return res.second ? 1 : 0;
}

int Database::srem(std::string_view key, std::string_view member)
{
    Entry* entry = findEntry(key, EntryType::SET);
    if (!entry)
        return 0;

    auto it = entry->set->find(std::string(member));
    if (it == entry->set->end())
        return 0;

    entry->set->erase(it);
    if (entry->set->empty())
    {
        removeEntry(entry);
    }
    return 1;
}

int Database::sismember(std::string_view key, std::string_view member)
{
    Entry* entry = findEntry(key, EntryType::SET);
    if (!entry)
        return 0;
    return entry->set->count(std::string(member)) ? 1 : 0;
}

std::vector<std::string> Database::smembers(std::string_view key)
{
    Entry* entry = findEntry(key, EntryType::SET);
    if (!entry)
        return {};

    std::vector<std::string> result;
    for (const auto& member : *entry->set)
    {
        result.push_back(member);
    }
    return result;
}

int64_t Database::scard(std::string_view key)
{
    Entry* entry = findEntry(key, EntryType::SET);
    return entry ? static_cast<int64_t>(entry->set->size()) : 0;
}

void Database::accept(IDataVisitor& visitor)
{
    auto visitTable = [&](const HashTable& ht)
    {
        for (size_t i = 0; !ht.empty() && i <= ht.mask(); ++i)
        {
            HNode* node = ht.bucketAt(i);
            while (node)
            {
                Entry* entry = Entry::fromHash(node);
                if (entry->isExpired())
                {
                    node = node->next;
                    continue;
                }

                switch (entry->type)
                {
                case EntryType::STRING:
                    visitor.onString(entry->key, entry->value, entry->expiresAt);
                    break;
                case EntryType::LIST:
                    if (entry->list)
                        visitor.onList(entry->key, *entry->list, entry->expiresAt);
                    break;
                case EntryType::SET:
                    if (entry->set)
                        visitor.onSet(entry->key, *entry->set, entry->expiresAt);
                    break;
                case EntryType::HASH:
                    if (entry->hash)
                        visitor.onHash(entry->key, *entry->hash, entry->expiresAt);
                    break;
                case EntryType::ZSET:
                    if (entry->zset)
                        visitor.onZSet(entry->key, *entry->zset, entry->expiresAt);
                    break;
                default:
                    break;
                }
                node = node->next;
            }
        }
    };

    visitTable(_map.newer());
    visitTable(_map.older());
}
