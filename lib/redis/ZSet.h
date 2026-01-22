#pragma once

#include "AVLTree.h"
#include "HashMap.h"
#include <_types/_uint32_t.h>
#include <new>
#include <string>

struct ZNode
{
    AVLNode _treeN;
    HNode _hashN;

    double _score = 0;
    size_t _len = 0;
    char _name[0]; // flexible array

    static ZNode* createNode(const char* name, size_t len, double score);
    static void destroyNode(ZNode* node);
    static bool less(AVLNode* ln, AVLNode* rn);
    static bool less(AVLNode* ln, double score, const char* name, size_t len);
    static ZNode* offset(ZNode* node, int64_t offset);
};

struct ZSet
{
    AVLTree _tree;
    HashMap _map;

    ZNode* LookUp(const char* name, size_t len);
    bool Insert(ZNode* node, double score);
    void Update(ZNode* node, double score);
    void Delete(ZNode* node);

    ZNode* Seekge(double score, const char* name, size_t len);

private:
    void updateScore(ZNode* node, double score);
    void treeInsert(ZNode* node);
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

    struct HKey
    {
        HNode _hashN;
        const char* _name = nullptr;
        size_t _len = 0;
        static bool cmp(HNode* a, HNode* key);
    };

} // namespace ZSET
