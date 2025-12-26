#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
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

struct RespValue;

using RespVariant = std::variant<std::monostate, std::string_view, int64_t, std::vector<RespValue>,
                                 std::vector<std::pair<RespValue, RespValue>>, bool>;

struct RespValue
{
    RespType type = RespType::None;
    RespVariant value;

    // Helpers
    int64_t& getInt()
    {
        return std::get<int64_t>(value);
    }
    const int64_t& getInt() const
    {
        return std::get<int64_t>(value);
    }

    std::string_view& getString()
    {
        return std::get<std::string_view>(value);
    }
    const std::string_view& getString() const
    {
        return std::get<std::string_view>(value);
    }

    bool& getBool()
    {
        return std::get<bool>(value);
    }
    const bool& getBool() const
    {
        return std::get<bool>(value);
    }

    std::vector<RespValue>& getArray()
    {
        return std::get<std::vector<RespValue>>(value);
    }
    const std::vector<RespValue>& getArray() const
    {
        return std::get<std::vector<RespValue>>(value);
    }

    std::vector<RespValue>& getSet()
    {
        return std::get<std::vector<RespValue>>(value);
    }
    const std::vector<RespValue>& getSet() const
    {
        return std::get<std::vector<RespValue>>(value);
    }

    std::vector<std::pair<RespValue, RespValue>>& getMap()
    {
        return std::get<std::vector<std::pair<RespValue, RespValue>>>(value);
    }
    const std::vector<std::pair<RespValue, RespValue>>& getMap() const
    {
        return std::get<std::vector<std::pair<RespValue, RespValue>>>(value);
    }
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