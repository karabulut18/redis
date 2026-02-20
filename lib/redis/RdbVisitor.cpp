#include "RdbVisitor.h"
#include "Database.h"
#include "HashMap.h"
#include "ZSet.h"
#include <arpa/inet.h>
#include <cstring>

RdbVisitor::RdbVisitor(std::ofstream& outFile) : _file(outFile)
{
    writeHeader();
}

RdbVisitor::~RdbVisitor()
{
}

void RdbVisitor::writeHeader()
{
    // Write magic string "RDB001"
    _file.write("RDB001", 6);
}

void RdbVisitor::writeType(uint8_t type)
{
    _file.write(reinterpret_cast<const char*>(&type), 1);
}

void RdbVisitor::writeExpiry(int64_t expiresAt)
{
    // Write 8 bytes, little endian is common but let's just write raw bytes for simplicity on same arch
    // To be perfectly portable, we should strictly define endianness. Let's use little-endian.
    uint64_t exp = static_cast<uint64_t>(expiresAt);
    char buf[8];
    for (int i = 0; i < 8; ++i)
    {
        buf[i] = static_cast<char>((exp >> (i * 8)) & 0xFF);
    }
    _file.write(buf, 8);
}

void RdbVisitor::writeLength(uint64_t len)
{
    // Write 8 bytes little endian
    char buf[8];
    for (int i = 0; i < 8; ++i)
    {
        buf[i] = static_cast<char>((len >> (i * 8)) & 0xFF);
    }
    _file.write(buf, 8);
}

void RdbVisitor::writeString(const std::string& str)
{
    writeLength(str.size());
    if (!str.empty())
    {
        _file.write(str.data(), str.size());
    }
}

void RdbVisitor::onString(const std::string& key, const std::string& value, int64_t expiresAt)
{
    writeType(TYPE_STRING);
    writeExpiry(expiresAt);
    writeString(key);
    writeString(value);
}

void RdbVisitor::onList(const std::string& key, const std::deque<std::string>& list, int64_t expiresAt)
{
    writeType(TYPE_LIST);
    writeExpiry(expiresAt);
    writeString(key);
    writeLength(list.size());
    for (const auto& item : list)
    {
        writeString(item);
    }
}

void RdbVisitor::onSet(const std::string& key, const std::unordered_set<std::string>& set, int64_t expiresAt)
{
    writeType(TYPE_SET);
    writeExpiry(expiresAt);
    writeString(key);
    writeLength(set.size());
    for (const auto& item : set)
    {
        writeString(item);
    }
}

void RdbVisitor::onHash(const std::string& key, const HashMap& hash, int64_t expiresAt)
{
    writeType(TYPE_HASH);
    writeExpiry(expiresAt);
    writeString(key);

    uint64_t size = hash.newer().size() + hash.older().size();
    writeLength(size);

    HT_FOREACH(hash.newer(), node)
    {
        HEntry* entry = HEntry::fromHash(node);
        writeString(entry->key);
        writeString(entry->value);
    }

    HT_FOREACH(hash.older(), node)
    {
        HEntry* entry = HEntry::fromHash(node);
        writeString(entry->key);
        writeString(entry->value);
    }
}

void RdbVisitor::onZSet(const std::string& key, const ZSet& zset, int64_t expiresAt)
{
    writeType(TYPE_ZSET);
    writeExpiry(expiresAt);
    writeString(key);

    uint64_t size = zset.size();
    writeLength(size);

    const HashMap& map = zset.map();

    HT_FOREACH(map.newer(), node)
    {
        ZNode* entry = ZNode::fromHash(node);
        writeString(entry->name);
        uint64_t scoreRaw;
        std::memcpy(&scoreRaw, &entry->score, 8);
        writeLength(scoreRaw);
    }

    HT_FOREACH(map.older(), node)
    {
        ZNode* entry = ZNode::fromHash(node);
        writeString(entry->name);
        uint64_t scoreRaw;
        std::memcpy(&scoreRaw, &entry->score, 8);
        writeLength(scoreRaw);
    }
}

void RdbVisitor::writeEOF()
{
    writeType(TYPE_EOF);
}
