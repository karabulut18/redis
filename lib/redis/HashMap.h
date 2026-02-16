#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

struct HNode
{
    HNode* next = nullptr;
    uint64_t code = 0;
};

// Comparator type: returns true if two HNodes are equal
using HNodeEq = std::function<bool(HNode*, HNode*)>;

class HashTable
{
public:
    HashTable() = default;

    void init(size_t size);
    void insert(HNode* node);
    HNode** lookup(HNode* key, const HNodeEq& eq);
    HNode* detach(HNode** from);

    size_t size() const
    {
        return _size;
    }
    bool empty() const
    {
        return _table.empty();
    }
    void clear();

    // Access to internals for rehashing and iteration
    HNode*& bucketAt(size_t index)
    {
        return _table[index];
    }
    HNode* const& bucketAt(size_t index) const
    {
        return _table[index];
    }
    size_t mask() const
    {
        return _mask;
    }

private:
    std::vector<HNode*> _table;
    size_t _mask = 0;
    size_t _size = 0;
};

// Iterate every HNode in a HashTable.
// `ht` must be a HashTable (or reference), `var` is the HNode* loop variable.
// Usage:
//     HT_FOREACH(table, node) {
//         Entry* e = Entry::fromHash(node);
//         ...
//     }
#define HT_FOREACH(ht, var)                                                                                            \
    for (size_t _hi_##var = 0; !(ht).empty() && _hi_##var <= (ht).mask(); _hi_##var++)                                 \
        for (HNode* var = (ht).bucketAt(_hi_##var); var; var = var->next)

class HashMap
{
public:
    HashMap() = default;

    HNode* lookup(HNode* key, const HNodeEq& eq);
    void insert(HNode* node);
    HNode* remove(HNode* key, const HNodeEq& eq);
    void clear();

    // Access to both tables for iteration macros
    const HashTable& newer() const
    {
        return _newer;
    }
    const HashTable& older() const
    {
        return _older;
    }

private:
    void triggerRehashing();
    void helpRehashing();

    HashTable _newer;
    HashTable _older;
    size_t _migratePosition = 0;

    static const size_t MAX_LOAD_FACTOR = 8;
    static const size_t REHASHING_WORK = 128;
};