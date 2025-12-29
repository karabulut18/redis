#pragma once

#include <stddef.h>
#include <stdint.h>

#include <assert.h>

struct HNode
{
    HNode* next = nullptr;
    uint64_t code = 0;
};

struct HashTable
{
    HNode** table = nullptr;
    size_t mask = 0; // power of  2, array size 2^n -1
    size_t size = 0; // number of keys
    static void init(HashTable* table, size_t size);
    static void insert(HashTable* table, HNode* node);
    static HNode** lookup(HashTable* tab, HNode* key, bool (*eq)(HNode*, HNode*));
    static HNode* detach(HashTable* tab, HNode** from);
};

struct HashMap
{
    HashTable older;
    HashTable newer;
    size_t migrate_position = 0;
    const size_t k_max_load_factor = 8;
    const size_t k_rehashing_work = 128;

    static HNode* lookup(HashMap* hmap, HNode* key, bool (*eq)(HNode*, HNode*));
    static void trigger_rehashing(HashMap* hmap);
    static void insert(HashMap* hmap, HNode* node);
    static HNode* remove(HashMap* hmap, HNode* key, bool (*eq)(HNode*, HNode*));
    static void help_rehashing(HashMap* hmap);
    static void clear(HashMap* hmap);
};