#pragma once

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

enum class CommandId
{
    Unknown,
    Ping,
    Echo,
    Set,
    Get,
    Del,
    Expire,
    PExpire,
    Ttl,
    PTtl,
    Persist,
    Incr,
    IncrBy,
    Decr,
    DecrBy,
    Type,
    ZAdd,
    ZRem,
    ZScore,
    ZRank,
    ZRange,
    ZRangeByScore,
    ZCard,
    HSet,
    HGet,
    HDel,
    HGetAll,
    HLen,
    HMSet,
    HMGet,
    LPush,
    RPush,
    LPop,
    RPop,
    LLen,
    LRange,
    SAdd,
    SRem,
    SIsMember,
    SMembers,
    SCard,
    Client,
    FlushAll,
    Config,
    BgRewriteAof
};

inline CommandId GetCommandId(const std::string& command)
{
    static const std::unordered_map<std::string, CommandId> commandMap = {{"PING", CommandId::Ping},
                                                                          {"ECHO", CommandId::Echo},
                                                                          {"SET", CommandId::Set},
                                                                          {"GET", CommandId::Get},
                                                                          {"DEL", CommandId::Del},
                                                                          {"EXPIRE", CommandId::Expire},
                                                                          {"PEXPIRE", CommandId::PExpire},
                                                                          {"TTL", CommandId::Ttl},
                                                                          {"PTTL", CommandId::PTtl},
                                                                          {"PERSIST", CommandId::Persist},
                                                                          {"INCR", CommandId::Incr},
                                                                          {"INCRBY", CommandId::IncrBy},
                                                                          {"DECR", CommandId::Decr},
                                                                          {"DECRBY", CommandId::DecrBy},
                                                                          {"TYPE", CommandId::Type},
                                                                          {"ZADD", CommandId::ZAdd},
                                                                          {"ZREM", CommandId::ZRem},
                                                                          {"ZSCORE", CommandId::ZScore},
                                                                          {"ZRANK", CommandId::ZRank},
                                                                          {"ZRANGE", CommandId::ZRange},
                                                                          {"ZRANGEBYSCORE", CommandId::ZRangeByScore},
                                                                          {"ZCARD", CommandId::ZCard},
                                                                          {"HSET", CommandId::HSet},
                                                                          {"HGET", CommandId::HGet},
                                                                          {"HDEL", CommandId::HDel},
                                                                          {"HGETALL", CommandId::HGetAll},
                                                                          {"HLEN", CommandId::HLen},
                                                                          {"HMSET", CommandId::HMSet},
                                                                          {"HMGET", CommandId::HMGet},
                                                                          {"LPUSH", CommandId::LPush},
                                                                          {"RPUSH", CommandId::RPush},
                                                                          {"LPOP", CommandId::LPop},
                                                                          {"RPOP", CommandId::RPop},
                                                                          {"LLEN", CommandId::LLen},
                                                                          {"LRANGE", CommandId::LRange},
                                                                          {"SADD", CommandId::SAdd},
                                                                          {"SREM", CommandId::SRem},
                                                                          {"SISMEMBER", CommandId::SIsMember},
                                                                          {"SMEMBERS", CommandId::SMembers},
                                                                          {"SCARD", CommandId::SCard},
                                                                          {"CLIENT", CommandId::Client},
                                                                          {"FLUSHALL", CommandId::FlushAll},
                                                                          {"CONFIG", CommandId::Config},
                                                                          {"BGREWRITEAOF", CommandId::BgRewriteAof}};

    std::string upperCmd = command;
    std::transform(upperCmd.begin(), upperCmd.end(), upperCmd.begin(), ::toupper);

    auto it = commandMap.find(upperCmd);
    if (it != commandMap.end())
    {
        return it->second;
    }
    return CommandId::Unknown;
}

inline bool IsWriteCommand(CommandId id)
{
    switch (id)
    {
    case CommandId::Set:
    case CommandId::Del:
    case CommandId::Expire:
    case CommandId::PExpire:
    case CommandId::Persist:
    case CommandId::Incr:
    case CommandId::IncrBy:
    case CommandId::Decr:
    case CommandId::DecrBy:
    case CommandId::ZAdd:
    case CommandId::ZRem:
    case CommandId::HSet:
    case CommandId::HMSet:
    case CommandId::HDel:
    case CommandId::LPush:
    case CommandId::RPush:
    case CommandId::LPop:
    case CommandId::RPop:
    case CommandId::SAdd:
    case CommandId::SRem:
    case CommandId::FlushAll:
        return true;
    default:
        return false;
    }
}
