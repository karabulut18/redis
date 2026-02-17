#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_set>

class HashMap;
class ZSet;

// Visitor interface for iterating over the database state.
// Decouples the Database from the logic of how data is stored or transformed
// (e.g., AOF Rewrite, RDB Binary Export, or diagnostic logging).
class IDataVisitor
{
public:
    virtual ~IDataVisitor() = default;

    // Called for each String entry. expiresAt is -1 if no expiry is set.
    virtual void onString(const std::string& key, const std::string& value, int64_t expiresAt) = 0;

    // Called for each List entry.
    virtual void onList(const std::string& key, const std::deque<std::string>& list, int64_t expiresAt) = 0;

    // Called for each Set entry.
    virtual void onSet(const std::string& key, const std::unordered_set<std::string>& set, int64_t expiresAt) = 0;

    // Called for each Hash entry.
    virtual void onHash(const std::string& key, const HashMap& hash, int64_t expiresAt) = 0;

    // Called for each ZSet entry.
    virtual void onZSet(const std::string& key, const ZSet& zset, int64_t expiresAt) = 0;
};
