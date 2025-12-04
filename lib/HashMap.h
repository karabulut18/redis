#pragma once

#include <stdint.h>
#include <stddef.h>


#include <assert.h>

#define container_of(ptr, T, member) ({                  \
    const typeof( ((T *)0)->member ) *__mptr = (ptr);    \
    (T *)( (char *)__mptr - offsetof(T, member) ); })


struct HashNode 
{
    HashNode*   next = nullptr;
    uint64_t    code = 0;
}; 

struct HashTable
{
    HashNode**  table = nullptr;
    size_t      mask = 0; // power of  2, array size 2^n -1
    size_t      size = 0; // number of keys
    static void         init(HashTable* table, size_t size);
    static void         insert(HashTable* table, HashNode* node);
    static HashNode**   lookup(HashTable* tab, HashNode* key, bool (*eq)(HashNode*, HashNode*));
    static HashNode*    detach(HashTable* tab, HashNode** from);
};

struct HashMap
{
    HashTable  older;
    HashTable  newer;
    size_t     migrate_position  = 0;
    const size_t  k_max_load_factor  = 8;
    const size_t  k_rehashing_work  = 128;

    static HashNode*    lookup(HashMap *hmap, HashNode *key, bool (*eq)(HashNode *, HashNode *));
    static void         trigger_rehashing(HashMap *hmap);
    static void         insert(HashMap *hmap, HashNode *node);
    static HashNode*    remove(HashMap *hmap, HashNode *key, bool (*eq)(HashNode *, HashNode *));
    static void         help_rehashing(HashMap *hmap);
};