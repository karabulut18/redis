#pragma once

#include "AVLTree.h"
#include "HashMap.h"
#include <_types/_uint32_t.h>
#include <new>
#include <string>

struct ZSet
{
    AVLTree _tree;
    HashMap _map;
};

struct ZNode
{
    AVLNode _treeN;
    HNode _hashN;

    double _score = 0;
    size_t len = 0;
    char name[0]; // flexible array

    static ZNode* createNode(const char* name, size_t len, double score);
    static void destroyNode(ZNode* node);
};

namespace ZSET
{
    enum
    {
        T_INIT = 0,
        T_STR = 1, // string
        T_ZSET = 2 // sorted set
    };

    struct Entry
    {
        HNode* _hashN;
        std::string _key;
        uint32_t _type = 0;
        union
        {
            std::string str;
            ZSet zset;
        };
        explicit Entry(uint32_t type);
        ~Entry();
    };
} // namespace ZSET
