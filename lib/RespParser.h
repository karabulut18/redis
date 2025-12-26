#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class RespType
{
    None,
    SimpleString,
    Error,
    Integer,
    BulkString,
    Array,
    Null,
    Map,
    Set,
    Boolean,
    BigNumber
};

enum class RespStatus
{
    Ok,
    Incomplete,
    Invalid
};

struct RespValue
{
    RespType type = RespType::None;
    std::string str_val;
    int64_t int_val = 0;
    std::vector<RespValue> array_val;
    std::vector<std::pair<RespValue, RespValue>> map_val;
    std::vector<RespValue> set_val;
    bool bool_val = false;
};

class RespParser
{
public:
    RespParser() = default;

    static const size_t MAX_RECURSION_DEPTH = 32;

    // Parses a single RESP message from the buffer.
    // [out] result: The parsed value.
    // [out] bytesRead: Number of bytes consumed from buffer.
    // [in] depth: Current recursion depth (internal use).
    RespStatus decode(const char* data, size_t length, RespValue& result, size_t& bytesRead, size_t depth = 0);

    // Encodes a RespValue into a RESP string (for sending).
    static std::string encode(const RespValue& value);

private:
    // Helper to find CRLF in buffer
    bool findCRLF(const char* data, size_t length, size_t& pos);

    // Parsers for individual types
    RespStatus parseSimpleString(const char* data, size_t length, RespValue& result, size_t& bytesRead);
    RespStatus parseError(const char* data, size_t length, RespValue& result, size_t& bytesRead);
    RespStatus parseInteger(const char* data, size_t length, RespValue& result, size_t& bytesRead);
    RespStatus parseBulkString(const char* data, size_t length, RespValue& result, size_t& bytesRead);
    RespStatus parseArray(const char* data, size_t length, RespValue& result, size_t& bytesRead, size_t depth);
    RespStatus parseMap(const char* data, size_t length, RespValue& result, size_t& bytesRead, size_t depth);
    RespStatus parseSet(const char* data, size_t length, RespValue& result, size_t& bytesRead, size_t depth);
    RespStatus parseBoolean(const char* data, size_t length, RespValue& result, size_t& bytesRead);
    RespStatus parseBigNumber(const char* data, size_t length, RespValue& result, size_t& bytesRead);
};