#pragma once

#include <string>
#include "../lib/HashMap.h" 

struct DataBase
{
    HashMap _db;
};

struct Entry
{
    HashNode    _node;
    std::string _key;
    std::string _val;
    static bool entry_eq(HashNode* lhs, HashNode* rhs);
};

bool Entry::entry_eq(HashNode* lhs, HashNode* rhs)
{
    Entry* le = container_of(lhs, Entry, _node);
    Entry* re = container_of(rhs, Entry, _node);
    return re->_key == le->_key;
}