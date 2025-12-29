#include "ZSet.h"
#include "HashMap.h"
#include "container_of.h"

ZNode* ZNode::createNode(const char* name, size_t len, double score)
{
    ZNode* node = (ZNode*)malloc(sizeof(ZNode) + len);
    AVLNode::initNode(&node->_treeN);
    node->_hashN.next = nullptr;
    // node->_hashN.code = str_hash(name, len);
    node->_score = score;
    node->len = len;
    memcpy(&node->name[0], name, len);
    return node;
}

void ZNode::destroyNode(ZNode* node)
{
    free(node);
}

bool ZNode::less(AVLNode* ln, AVLNode* rn)
{
    ZNode* zl = container_of(ln, ZNode, _treeN);
    ZNode* zr = container_of(rn, ZNode, _treeN);
    if (zl->_score != zr->_score)
        return zl->_score < zr->_score;

    int rv = memcmp(zl->name, zr->name, (zl->len < zr->len ? zl->len : zr->len));
    return (rv != 0) ? rv < 0 : zl->len < zr->len;
}

ZSET::Entry::Entry(uint32_t type) : _type(type)
{
    assert(_type == ZSET::T_STR || _type == ZSET::T_ZSET);
    // call constructor
    if (_type == ZSET::T_STR)
        new (&str) std::string();
    else if (_type == ZSET::T_ZSET)
        new (&zset) ZSet();
}

ZSET::Entry::~Entry()
{
    // call destructor
    if (_type == ZSET::T_STR)
        str.~basic_string();
    else if (_type == ZSET::T_ZSET)
        zset.~ZSet();
}

ZNode* ZSet::LookUp(const char* name, size_t len)
{
    if (_tree._root == nullptr)
        return nullptr;

    ZSET::HKey key;
    // key._hashN.code = str_hash((uint8_t*)name, len);
    key._name = name;
    key._len = len;
    HNode* found = HashMap::lookup(&_map, &key._hashN, ZSET::HKey::cmp);
    return found == nullptr ? nullptr : container_of(found, ZNode, _hashN);
}

bool ZSet::Insert(ZNode* node, double score)
{
    if (ZNode* found = LookUp(node->name, node->len)) // already exists, it is updated
    {
        updateScore(found, score);
        return false;
    }

    ZNode* newNode = ZNode::createNode(node->name, node->len, score);
    HashMap::insert(&_map, &newNode->_hashN);
    AVLTree::searchAndInsert(&_tree._root, &newNode->_treeN, ZNode::less);
    return true;
}
//
// void ZSet::updateScore(ZNode* node, double score)
//{
//    if (node->_score == score)
//        return;
//
//    _tree._root = AVLTree::searchAndDelete(&_tree._root, ZNode::less, node);
//    node->_score = score;
//    AVLTree::searchAndInsert(&_tree._root, node, ZNode::less);
//}

bool ZSET::HKey::cmp(HNode* node, HNode* keyHNode)
{
    ZNode* znode = container_of(node, ZNode, _hashN);
    ZSET::HKey* hkey = container_of(keyHNode, ZSET::HKey, _hashN);

    if (znode->len != hkey->_len)
        return false;
    return 0 == memcmp(znode->name, hkey->_name, znode->len);
}