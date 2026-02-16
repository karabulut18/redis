#pragma once

#include "AVLTree.h"
#include "HashMap.h"
#include <cstdint>
#include <string>

struct ZNode
{
    AVLNode treeNode;
    HNode hashNode;

    double score = 0;
    std::string name;

    ZNode(const std::string& name, double score);

    static bool less(AVLNode* ln, AVLNode* rn);
    static bool less(AVLNode* ln, double score, const std::string& name);
    static ZNode* fromTree(AVLNode* treeNode);
    static ZNode* fromHash(HNode* hashNode);
    static ZNode* offset(ZNode* node, int64_t offset);
};

class ZSet
{
public:
    ZSet() = default;
    ~ZSet();

    ZNode* lookUp(const std::string& name);
    bool insert(const std::string& name, double score);
    void update(ZNode* node, double score);
    void remove(ZNode* node);

    ZNode* seekGe(double score, const std::string& name);

    size_t size() const
    {
        return _size;
    }

    AVLTree& tree()
    {
        return _tree;
    }

private:
    void treeInsert(ZNode* node);

    AVLTree _tree;
    HashMap _map;
    size_t _size = 0;
};

namespace ZSET
{
    struct HKey
    {
        HNode hashNode;
        std::string name;

        static bool cmp(HNode* a, HNode* b);
    };

} // namespace ZSET
