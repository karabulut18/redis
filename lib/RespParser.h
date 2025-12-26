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
    Null
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
};

class RespParser
{
public:
    RespParser() = default;

    // Parses a single RESP message from the buffer.
    // [out] result: The parsed value.
    // [out] bytesRead: Number of bytes consumed from buffer.
    RespStatus decode(const char *data, size_t length, RespValue &result, size_t &bytesRead);

private:
    // Helper to find CRLF in buffer
    bool findCRLF(const char *data, size_t length, size_t &pos);

    // Parsers for individual types
    RespStatus parseSimpleString(const char *data, size_t length, RespValue &result, size_t &bytesRead);
    RespStatus parseError(const char *data, size_t length, RespValue &result, size_t &bytesRead);
    RespStatus parseInteger(const char *data, size_t length, RespValue &result, size_t &bytesRead);
    RespStatus parseBulkString(const char *data, size_t length, RespValue &result, size_t &bytesRead);
    RespStatus parseArray(const char *data, size_t length, RespValue &result, size_t &bytesRead);
};