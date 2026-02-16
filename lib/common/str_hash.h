#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

uint64_t str_hash(const uint8_t* str, size_t len);

inline uint64_t str_hash(const std::string& s)
{
    return str_hash(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}