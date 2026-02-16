#include "ZSet.h"
#include "str_hash.h"
#include <cassert>
#include <cstddef>

// --- ZNode ---

ZNode::ZNode(const std::string& n, double s) : score(s), name(n)
{
    treeNode.init();
    hashNode.next = nullptr;
    hashNode.code = str_hash(reinterpret_cast<const uint8_t*>(name.data()), name.size());
}

ZNode* ZNode::fromTree(AVLNode* node)
{
    if (!node)
        return nullptr;
    return reinterpret_cast<ZNode*>(reinterpret_cast<char*>(node) - offsetof(ZNode, treeNode));
}

ZNode* ZNode::fromHash(HNode* node)
{
    if (!node)
        return nullptr;
    return reinterpret_cast<ZNode*>(reinterpret_cast<char*>(node) - offsetof(ZNode, hashNode));
}

bool ZNode::less(AVLNode* ln, AVLNode* rn)
{
    ZNode* zl = fromTree(ln);
    ZNode* zr = fromTree(rn);
    if (zl->score != zr->score)
        return zl->score < zr->score;
    return zl->name < zr->name;
}

bool ZNode::less(AVLNode* ln, double score, const std::string& name)
{
    ZNode* znode = fromTree(ln);
    if (znode->score != score)
        return znode->score < score;
    return znode->name < name;
}

ZNode* ZNode::offset(ZNode* node, int64_t off)
{
    AVLNode* treeN = node ? AVLNode::offset(&node->treeNode, off) : nullptr;
    return fromTree(treeN);
}

// --- ZSet ---

ZNode* ZSet::lookUp(const std::string& name)
{
    if (_tree.root() == nullptr)
        return nullptr;

    ZSET::HKey key;
    key.hashNode.code = str_hash(reinterpret_cast<const uint8_t*>(name.data()), name.size());
    key.name = name;
    HNode* found = _map.lookup(&key.hashNode, ZSET::HKey::cmp);
    return found ? ZNode::fromHash(found) : nullptr;
}

bool ZSet::insert(const std::string& name, double score)
{
    if (ZNode* found = lookUp(name))
    {
        update(found, score);
        return false;
    }

    ZNode* node = new ZNode(name, score);
    _map.insert(&node->hashNode);
    _tree.insert(&node->treeNode, static_cast<bool (*)(AVLNode*, AVLNode*)>(ZNode::less));
    _size++;
    return true;
}

void ZSet::update(ZNode* node, double score)
{
    _tree.setRoot(AVLNode::deleteNode(&node->treeNode));
    node->treeNode.init();
    node->score = score;
    treeInsert(node);
}

void ZSet::remove(ZNode* node)
{
    ZSET::HKey key;
    key.hashNode.code = node->hashNode.code;
    key.name = node->name;
    HNode* found = _map.remove(&key.hashNode, ZSET::HKey::cmp);
    assert(found);
    (void)found;

    _tree.setRoot(AVLNode::deleteNode(&node->treeNode));
    _size--;
    delete node;
}

ZNode* ZSet::seekGe(double score, const std::string& name)
{
    AVLNode* found = nullptr;
    for (AVLNode* cur = _tree.root(); cur != nullptr;)
    {
        if (ZNode::less(cur, score, name))
            cur = cur->right;
        else
        {
            found = cur;
            cur = cur->left;
        }
    }
    return ZNode::fromTree(found);
}

void ZSet::treeInsert(ZNode* node)
{
    AVLNode* parent = nullptr;
    AVLNode* root = _tree.root();
    AVLNode** from = &root;

    while (*from)
    {
        parent = *from;
        from = static_cast<bool (*)(AVLNode*, AVLNode*)>(ZNode::less)(&node->treeNode, parent) ? &parent->left
                                                                                               : &parent->right;
    }
    *from = &node->treeNode;
    node->treeNode.parent = parent;
    _tree.setRoot(AVLNode::balance(&node->treeNode));
}

ZSet::~ZSet()
{
    // Safely delete all ZNodes from both tables
    for (size_t i = 0; !_map.newer().empty() && i <= _map.newer().mask(); ++i)
    {
        HNode* node = _map.newer().bucketAt(i);
        while (node)
        {
            HNode* next = node->next;
            delete ZNode::fromHash(node);
            node = next;
        }
    }
    for (size_t i = 0; !_map.older().empty() && i <= _map.older().mask(); ++i)
    {
        HNode* node = _map.older().bucketAt(i);
        while (node)
        {
            HNode* next = node->next;
            delete ZNode::fromHash(node);
            node = next;
        }
    }
    _map.clear();
}

// --- ZSET::HKey ---

bool ZSET::HKey::cmp(HNode* node, HNode* keyNode)
{
    ZNode* znode = ZNode::fromHash(node);
    ZSET::HKey* hkey = reinterpret_cast<ZSET::HKey*>(reinterpret_cast<char*>(keyNode) - offsetof(ZSET::HKey, hashNode));

    return znode->name == hkey->name;
}