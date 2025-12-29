#include "ZSet.h"

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