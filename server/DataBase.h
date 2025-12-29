#pragma once

#include "../lib/HashMap.h"
#include <string>

struct DataBase
{
    HashMap _db;
};

struct Entry
{
    HNode _node;
    std::string _key;
    std::string _val;
    static bool entry_eq(HNode* lhs, HNode* rhs);
};

inline bool Entry::entry_eq(HNode* lhs, HNode* rhs)
{
    Entry* le = container_of(lhs, Entry, _node);
    Entry* re = container_of(rhs, Entry, _node);
    return re->_key == le->_key;
}