#pragma once

#include "IDataVisitor.h"
#include <fstream>
#include <string>

// Magic String: "RDB001"
// 1 byte type identifier
// 8 byte expiry timestamp (-1 for no expiry)
// Length-prefixed string for the Key
// Type-specific payload encoding

class RdbVisitor : public IDataVisitor
{
public:
    explicit RdbVisitor(std::ofstream& outFile);
    ~RdbVisitor() override;

    void onString(const std::string& key, const std::string& value, int64_t expiresAt) override;
    void onList(const std::string& key, const std::deque<std::string>& list, int64_t expiresAt) override;
    void onSet(const std::string& key, const std::unordered_set<std::string>& set, int64_t expiresAt) override;
    void onHash(const std::string& key, const HashMap& hash, int64_t expiresAt) override;
    void onZSet(const std::string& key, const ZSet& zset, int64_t expiresAt) override;

    // Call after all elements have been visited to write the EOF marker.
    void writeEOF();

private:
    std::ofstream& _file;

    void writeHeader();
    void writeType(uint8_t type);
    void writeExpiry(int64_t expiresAt);
    void writeLength(uint64_t len);
    void writeString(const std::string& str);

    // Type definitions
    static constexpr uint8_t TYPE_EOF = 0;
    static constexpr uint8_t TYPE_STRING = 1;
    static constexpr uint8_t TYPE_LIST = 2;
    static constexpr uint8_t TYPE_SET = 3;
    static constexpr uint8_t TYPE_HASH = 4;
    static constexpr uint8_t TYPE_ZSET = 5;
};
