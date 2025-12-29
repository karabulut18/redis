#include "HashMap.h"

void HashTable::init(HashTable* hashTable, size_t size)
{
    assert(size > 0 && ((size - 1) & size) == 0); // n must be power of two
    hashTable->table = (HNode**)calloc(size, sizeof(HNode*));
    hashTable->mask = size - 1;
    hashTable->size = 0;
}

void HashTable::insert(HashTable* hashTable, HNode* node)
{
    size_t index = node->code & hashTable->mask;
    HNode* next = hashTable->table[index];
    node->next = next;
    hashTable->table[index] = node;
    hashTable->size++;
}

HNode** HashTable::lookup(HashTable* tab, HNode* key, bool (*eq)(HNode*, HNode*))
{
    if (tab->table == nullptr)
        return nullptr;

    size_t index = key->code & tab->mask;

    HNode** from = &tab->table[index];
    for (HNode *curr, **prev = from; (curr = *prev) != nullptr; prev = &curr->next)
    {
        if (curr->code == key->code && eq(curr, key))
            return from;
    }

    return nullptr;
}

HNode* HashTable::detach(HashTable* tab, HNode** from)
{
    HNode* node = *from;
    *from = node->next;
    tab->size--;
    return node;
}

HNode* HashMap::lookup(HashMap* hmap, HNode* key, bool (*eq)(HNode*, HNode*))
{
    HashMap::help_rehashing(hmap); // migrate some keys
    HNode** from = HashTable::lookup(&hmap->older, key, eq);

    if (from != nullptr)
        HashTable::lookup(&hmap->newer, key, eq);

    return from ? *from : nullptr;
};

void HashMap::trigger_rehashing(HashMap* hmap)
{
    assert(hmap->older.table != nullptr);
    hmap->older = hmap->newer; // (newer, older) <- (new_table, newer)
    HashTable::init(&hmap->newer, (hmap->newer.mask + 1) * 2);
    hmap->migrate_position = 0;
};

HNode* HashMap::remove(HashMap* hmap, HNode* key, bool (*eq)(HNode*, HNode*))
{
    HashMap::help_rehashing(hmap); // migrate some keys
    if (HNode** from = HashTable::lookup(&hmap->newer, key, eq))
        return HashTable::detach(&hmap->newer, from);

    if (HNode** from = HashTable::lookup(&hmap->older, key, eq))
        return HashTable::detach(&hmap->older, from);
    return nullptr;
};

void HashMap::insert(HashMap* hmap, HNode* node)
{
    if (!hmap->newer.table)
        HashTable::init(&hmap->newer, 4); // initialized it if empty

    HashTable::insert(&hmap->newer, node); // always insert to the newer table

    if (!hmap->older.table) // check whether we need to rehash
    {
        size_t shreshold = (hmap->newer.mask + 1) * hmap->k_max_load_factor;
        if (hmap->newer.size >= shreshold)
            HashMap::trigger_rehashing(hmap);
    }
    HashMap::help_rehashing(hmap); // migrate some keys
};

void HashMap::help_rehashing(HashMap* hmap)
{
    size_t nwork = 0;
    while (nwork < hmap->k_rehashing_work && hmap->older.size > 0)
    {
        HNode** from = &hmap->older.table[hmap->migrate_position];
        if (!*from)
        {
            hmap->migrate_position++;
            continue; // empty slot
        }
        // move the first list item to the newer table
        HashTable::insert(&hmap->newer, HashTable::detach(&hmap->older, from));
        nwork++;
    }
    // discard the old table if done
    if (hmap->older.size == 0 && hmap->older.table)
    {
        free(hmap->older.table);
        hmap->older = HashTable{};
    }
}

void HashMap::clear(HashMap* hmap)
{
    if (hmap->older.table)
        free(hmap->older.table);
    if (hmap->newer.table)
        free(hmap->newer.table);
    hmap->older = HashTable{};
    hmap->newer = HashTable{};
}