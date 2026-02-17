#pragma once
#include "../common/BufferSegment.h"
#include "../common/SegmentedBuffer.h"
#include <cstddef>
#include <cstdint>
#include <memory>
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

using RespVariant =
    std::variant<std::monostate, std::string, std::string_view, int64_t, std::shared_ptr<std::vector<RespValue>>,
                 std::shared_ptr<std::vector<std::pair<RespValue, RespValue>>>, bool>;

struct RespValue
{
    RespType type = RespType::None;
    RespVariant value;
    std::shared_ptr<BufferSegment> anchor; // Keeps the memory segment alive for string_view

    bool isNull() const
    {
        return type == RespType::Null;
    }

    std::string toString() const
    {
        if (std::holds_alternative<std::string>(value))
            return std::get<std::string>(value);
        if (std::holds_alternative<std::string_view>(value))
            return std::string(std::get<std::string_view>(value));
        if (std::holds_alternative<int64_t>(value))
            return std::to_string(std::get<int64_t>(value));
        return "";
    }

    void setStringOwned(std::string s)
    {
        value = std::move(s);
    }

    void setArray(std::vector<RespValue> arr)
    {
        type = RespType::Array;
        value = std::make_shared<std::vector<RespValue>>(std::move(arr));
    }

    void setMap(std::vector<std::pair<RespValue, RespValue>> map)
    {
        type = RespType::Map;
        value = std::make_shared<std::vector<std::pair<RespValue, RespValue>>>(std::move(map));
    }

    void setSet(std::vector<RespValue> set)
    {
        type = RespType::Set;
        value = std::make_shared<std::vector<RespValue>>(std::move(set));
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
        return *std::get<std::shared_ptr<std::vector<RespValue>>>(value);
    }
    const std::vector<RespValue>& getArray() const
    {
        return *std::get<std::shared_ptr<std::vector<RespValue>>>(value);
    }

    std::vector<RespValue>& getSet()
    {
        return *std::get<std::shared_ptr<std::vector<RespValue>>>(value);
    }
    const std::vector<RespValue>& getSet() const
    {
        return *std::get<std::shared_ptr<std::vector<RespValue>>>(value);
    }

    std::vector<std::pair<RespValue, RespValue>>& getMap()
    {
        return *std::get<std::shared_ptr<std::vector<std::pair<RespValue, RespValue>>>>(value);
    }
    const std::vector<std::pair<RespValue, RespValue>>& getMap() const
    {
        return *std::get<std::shared_ptr<std::vector<std::pair<RespValue, RespValue>>>>(value);
    }
};

class RespParser
{
public:
    RespParser() = default;

    static const size_t MAX_RECURSION_DEPTH = 32;

    // Parses a single RESP message from a SegmentedBuffer.
    // [out] result: The parsed value.
    // [out] bytesRead: Number of bytes consumed from buffer.
    // [in] depth: Current recursion depth (internal use).
    RespStatus decode(SegmentedBuffer& buffer, RespValue& result, size_t& bytesRead, size_t depth = 0);

    // Old signature for compatibility/testing if needed (can be removed later)
    RespStatus decode(const char* data, size_t length, RespValue& result, size_t& bytesRead, size_t depth = 0);

    // Encodes a RespValue into a RESP string (for sending).
    static std::string encode(const RespValue& value);

private:
    // Helper to find CRLF in buffer
    bool findCRLF(const char* data, size_t length, size_t& pos);

    // Parsers for individual types
    RespStatus parseSimpleString(std::string_view data, RespValue& result, size_t& bytesRead);
    RespStatus parseError(std::string_view data, RespValue& result, size_t& bytesRead);
    RespStatus parseInteger(std::string_view data, RespValue& result, size_t& bytesRead);
    RespStatus parseBulkString(std::string_view data, std::shared_ptr<BufferSegment> anchor, RespValue& result,
                               size_t& bytesRead);
    RespStatus parseArray(std::string_view data, std::shared_ptr<BufferSegment> anchor, RespValue& result,
                          size_t& bytesRead, size_t depth);
    RespStatus parseMap(std::string_view data, std::shared_ptr<BufferSegment> anchor, RespValue& result,
                        size_t& bytesRead, size_t depth);
    RespStatus parseSet(std::string_view data, std::shared_ptr<BufferSegment> anchor, RespValue& result,
                        size_t& bytesRead, size_t depth);
    RespStatus parseBoolean(std::string_view data, RespValue& result, size_t& bytesRead);
    RespStatus parseBigNumber(std::string_view data, RespValue& result, size_t& bytesRead);

private:
    RespStatus decodeInternal(std::string_view data, std::shared_ptr<BufferSegment> anchor, RespValue& result,
                              size_t& bytesRead, size_t depth);
};