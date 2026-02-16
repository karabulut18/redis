#include "HashMap.h"

void HashTable::init(size_t size)
{
    assert(size > 0 && ((size - 1) & size) == 0); // must be power of two
    _table.assign(size, nullptr);
    _mask = size - 1;
    _size = 0;
}

void HashTable::insert(HNode* node)
{
    size_t index = node->code & _mask;
    node->next = _table[index];
    _table[index] = node;
    _size++;
}

HNode** HashTable::lookup(HNode* key, const HNodeEq& eq)
{
    if (_table.empty())
        return nullptr;

    size_t index = key->code & _mask;

    HNode** from = &_table[index];
    for (HNode* curr; (curr = *from) != nullptr; from = &curr->next)
    {
        if (curr->code == key->code && eq(curr, key))
            return from;
    }

    return nullptr;
}

HNode* HashTable::detach(HNode** from)
{
    HNode* node = *from;
    *from = node->next;
    _size--;
    return node;
}

void HashTable::clear()
{
    _table.clear();
    _mask = 0;
    _size = 0;
}

// --- HashMap ---

HNode* HashMap::lookup(HNode* key, const HNodeEq& eq)
{
    helpRehashing();
    HNode** from = _older.lookup(key, eq);

    if (!from)
        from = _newer.lookup(key, eq);

    return from ? *from : nullptr;
}

void HashMap::triggerRehashing()
{
    assert(!_newer.empty());
    _older = std::move(_newer);
    _newer = HashTable();
    _newer.init((_older.mask() + 1) * 2);
    _migratePosition = 0;
}

HNode* HashMap::remove(HNode* key, const HNodeEq& eq)
{
    helpRehashing();
    if (HNode** from = _newer.lookup(key, eq))
        return _newer.detach(from);

    if (HNode** from = _older.lookup(key, eq))
        return _older.detach(from);

    return nullptr;
}

void HashMap::insert(HNode* node)
{
    if (_newer.empty())
        _newer.init(4);

    _newer.insert(node);

    if (_older.empty())
    {
        size_t threshold = (_newer.mask() + 1) * MAX_LOAD_FACTOR;
        if (_newer.size() >= threshold)
            triggerRehashing();
    }
    helpRehashing();
}

void HashMap::helpRehashing()
{
    size_t nwork = 0;
    while (nwork < REHASHING_WORK && _older.size() > 0)
    {
        HNode*& slot = _older.bucketAt(_migratePosition);
        if (!slot)
        {
            _migratePosition++;
            continue;
        }
        _newer.insert(_older.detach(&slot));
        nwork++;
    }
    if (_older.size() == 0 && !_older.empty())
    {
        _older.clear();
    }
}

void HashMap::clear()
{
    _older.clear();
    _newer.clear();
}