#include "HashMap.h"


void HashTable::init(HashTable* hashTable, size_t size)
{
    assert(size > 0 && ((size-1) & size) == 0); // n must be power of two
    hashTable->table = (HashNode**)calloc(size, sizeof(HashNode*));
    hashTable->mask = size - 1;
    hashTable->size = 0;
}

void HashTable::insert(HashTable* hashTable, HashNode* node)
{
    size_t index = node->code & hashTable->mask;
    HashNode* next = hashTable->table[index];
    node->next = next;
    hashTable->table[index] = node;
    hashTable->size++;
}

HashNode** HashTable::lookup(HashTable* tab, HashNode* key, bool (*eq)(HashNode*, HashNode*))
{
    if(tab->table == nullptr)
        return nullptr;

    size_t index = key->code & tab->mask;

    HashNode** from = &tab->table[index];
    for(HashNode* curr, **prev = from; (curr = *prev) != nullptr; prev = &curr->next)
    {
        if(curr->code == key->code && eq(curr, key))
            return from;
    }

    return nullptr;
}

HashNode* HashTable::detach(HashTable* tab, HashNode** from)
{
    HashNode* node = *from;
    *from = node->next;
    tab->size--;
    return node;
}

HashNode*  HashMap::lookup(HashMap *hmap, HashNode *key, bool (*eq)(HashNode *, HashNode *))
{
    HashMap::help_rehashing(hmap);        // migrate some keys
    HashNode** from = HashTable::lookup(&hmap->older, key, eq);

    if(from != nullptr)
        HashTable::lookup(&hmap->newer, key, eq);

    return from ? *from : nullptr;
};


void HashMap::trigger_rehashing(HashMap *hmap)
{
    HashTable* old = &hmap->older;
    hmap->older = hmap->newer; // (newer, older) <- (new_table, newer)
    HashTable::init(&hmap->newer, (hmap->newer.mask +1) *2);
    hmap->migrate_position = 0;
};

HashNode* HashMap::remove(HashMap *hmap, HashNode *key, bool (*eq)(HashNode *, HashNode *))
{
    HashMap::help_rehashing(hmap);        // migrate some keys
    if(HashNode** from = HashTable::lookup(&hmap->newer, key, eq))
        return HashTable::detach(&hmap->newer, from);

    if(HashNode** from = HashTable::lookup(&hmap->older, key, eq))
        return HashTable::detach(&hmap->older, from);
    return nullptr;
};

void  HashMap::insert(HashMap *hmap, HashNode *node)
{
    if (!hmap->newer.table)
        HashTable::init(&hmap->newer, 4);    // initialized it if empty

    HashTable::insert(&hmap->newer, node);   // always insert to the newer table

    if (!hmap->older.table)         // check whether we need to rehash
    {
        size_t shreshold = (hmap->newer.mask + 1) * hmap->k_max_load_factor;
        if (hmap->newer.size >= shreshold)
            HashMap::trigger_rehashing(hmap);
    }
    HashMap::help_rehashing(hmap);        // migrate some keys
};

void HashMap::help_rehashing(HashMap *hmap)
{
    size_t nwork = 0;
    while (nwork < hmap->k_rehashing_work && hmap->older.size > 0)
    {
        HashNode **from = &hmap->older.table[hmap->migrate_position];
        if (!*from) {
            hmap->migrate_position++;
            continue;   // empty slot
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