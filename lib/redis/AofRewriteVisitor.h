#pragma once

#include "Database.h"
#include "HashMap.h"
#include "IDataVisitor.h"
#include "RespParser.h"
#include "ZSet.h"
#include <vector>

/**
 * Visitor that writes the database state as RESP commands to a file.
 * Used for AOF rewrite (compaction).
 */
class AofRewriteVisitor : public IDataVisitor
{
public:
    explicit AofRewriteVisitor(std::ostream& os) : _os(os)
    {
    }

    void onString(const std::string& key, const std::string& value, int64_t expiresAt) override
    {
        writeCommand({"SET", key, value});
        if (expiresAt >= 0)
        {
            writeCommand({"PEXPIREAT", key, std::to_string(expiresAt)});
        }
    }

    void onList(const std::string& key, const std::deque<std::string>& list, int64_t expiresAt) override
    {
        if (list.empty())
            return;

        // Use RPUSH to rebuild the list
        std::vector<std::string> args = {"RPUSH", key};
        for (const auto& item : list)
        {
            args.push_back(item);
            // Optionally batch RPUSH if list is very large
            if (args.size() > 1000)
            {
                writeCommand(args);
                args = {"RPUSH", key};
            }
        }
        if (args.size() > 2)
            writeCommand(args);

        if (expiresAt >= 0)
        {
            writeCommand({"PEXPIREAT", key, std::to_string(expiresAt)});
        }
    }

    void onSet(const std::string& key, const std::unordered_set<std::string>& set, int64_t expiresAt) override
    {
        if (set.empty())
            return;

        std::vector<std::string> args = {"SADD", key};
        for (const auto& item : set)
        {
            args.push_back(item);
            if (args.size() > 1000)
            {
                writeCommand(args);
                args = {"SADD", key};
            }
        }
        if (args.size() > 2)
            writeCommand(args);

        if (expiresAt >= 0)
        {
            writeCommand({"PEXPIREAT", key, std::to_string(expiresAt)});
        }
    }

    void onHash(const std::string& key, const HashMap& hash, int64_t expiresAt) override
    {
        std::vector<std::string> args = {"HMSET", key};

        auto collect = [&](const HashTable& ht)
        {
            HT_FOREACH(ht, node)
            {
                HEntry* he = HEntry::fromHash(node);
                args.push_back(he->key);
                args.push_back(he->value);
                if (args.size() > 1000)
                {
                    writeCommand(args);
                    args = {"HMSET", key};
                }
            }
        };

        collect(hash.newer());
        collect(hash.older());

        if (args.size() > 2)
            writeCommand(args);

        if (expiresAt >= 0)
        {
            writeCommand({"PEXPIREAT", key, std::to_string(expiresAt)});
        }
    }

    void onZSet(const std::string& key, const ZSet& zset, int64_t expiresAt) override
    {
        std::vector<std::string> args = {"ZADD", key};

        ZNode* znode = ZNode::fromTree(AVLNode::findMin(const_cast<ZSet&>(zset).tree().root()));
        while (znode)
        {
            args.push_back(std::to_string(znode->score));
            args.push_back(znode->name);

            if (args.size() > 1000)
            {
                writeCommand(args);
                args = {"ZADD", key};
            }
            znode = ZNode::fromTree(AVLNode::successor(&znode->treeNode));
        }

        if (args.size() > 2)
            writeCommand(args);

        if (expiresAt >= 0)
        {
            writeCommand({"PEXPIREAT", key, std::to_string(expiresAt)});
        }
    }

private:
    void writeCommand(const std::vector<std::string>& args)
    {
        std::vector<RespValue> respArgs;
        for (const auto& arg : args)
        {
            RespValue v;
            v.type = RespType::BulkString;
            v.value = arg;
            respArgs.push_back(v);
        }

        RespValue arr;
        arr.setArray(std::move(respArgs));
        _os << RespParser::encode(arr);
    }

    std::ostream& _os;
};
