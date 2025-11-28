#pragma once 

#include <string>
#include "HashMap.h" 

#define container_of(ptr, T, member) ({                  \
    const typeof( ((T *)0)->member ) *__mptr = (ptr);    \
    (T *)( (char *)__mptr - offsetof(T, member) ); })

struct DataBase
{
    HashMap mapping;
    static bool entry_eq(HashNode* ls, HashNode* rs);
};

bool DataBase::entry_eq(HashNode* ls, HashNode* rs)
{
    Entry* le = container_of(ls, Entry, Entry::hashNode);
    Entry* re = container_of(rs, Entry, Entry::hashNode);
    return le->_key == re->_key;
}

struct Entry
{
    HashNode  hashNode;
    std::string _key;
    std::string _value;
};