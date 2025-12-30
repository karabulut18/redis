#include "str_hash.h"

// this will be handled later
// FNV hash
uint64_t str_hash(const uint8_t* str, size_t len)
{
    uint64_t hash = 0xcbf29ce484222325; // FNV_offset_basis
    for (size_t i = 0; i < len; i++)
    {
        hash ^= str[i];
        hash *= 0x100000001b3; // FNV_prime
    }
    return hash;
}