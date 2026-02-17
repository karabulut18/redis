#include "RespParser.h"
#include <cassert>
#include <cstring>
#include <string>

bool RespParser::findCRLF(const char* data, size_t length, size_t& pos)
{
    if (length < 2)
        return false;
    for (size_t i = 0; i < length - 1; ++i)
    {
        if (data[i] == '\r' && data[i + 1] == '\n')
        {
            pos = i;
            return true;
        }
    }
    return false;
}

std::string RespParser::encode(const RespValue& value)
{
    std::string out;
    switch (value.type)
    {
    case RespType::SimpleString:
        out.append("+");
        out.append(value.toString());
        out.append("\r\n");
        break;
    case RespType::Error:
        out.append("-");
        out.append(value.toString());
        out.append("\r\n");
        break;
    case RespType::Integer:
        out += ":" + std::to_string(std::get<int64_t>(value.value)) + "\r\n";
        break;
    case RespType::BulkString:
    {
        std::string s = value.toString();
        out += "$" + std::to_string(s.length()) + "\r\n";
        out.append(s);
        out.append("\r\n");
        break;
    }
    case RespType::Null:
        out += "$-1\r\n";
        break;

    case RespType::Array:
    {
        const auto& arr = *std::get<std::shared_ptr<std::vector<RespValue>>>(value.value);
        out += "*" + std::to_string(arr.size()) + "\r\n";
        for (const auto& item : arr)
            out += encode(item);
        break;
    }
    case RespType::Map:
    {
        const auto& map = *std::get<std::shared_ptr<std::vector<std::pair<RespValue, RespValue>>>>(value.value);
        out += "%" + std::to_string(map.size()) + "\r\n";
        for (const auto& pair : map)
        {
            out += encode(pair.first);
            out += encode(pair.second);
        }
        break;
    }
    case RespType::Set:
    {
        const auto& set = *std::get<std::shared_ptr<std::vector<RespValue>>>(value.value);
        out += "~" + std::to_string(set.size()) + "\r\n";
        for (const auto& item : set)
            out += encode(item);
        break;
    }
    case RespType::Boolean:
        out += "#" + std::string(std::get<bool>(value.value) ? "t" : "f") + "\r\n";
        break;
    case RespType::BigNumber:
        out.append("(");
        out.append(value.toString());
        out.append("\r\n");
        break;
    default:
        break;
    }
    return out;
}

RespStatus RespParser::decode(const char* data, size_t length, RespValue& result, size_t& bytesRead, size_t depth)
{
    return decodeInternal(std::string_view(data, length), nullptr, result, bytesRead, depth);
}

RespStatus RespParser::decode(SegmentedBuffer& buffer, RespValue& result, size_t& bytesRead, size_t depth)
{
    // Peek as much as possible to handle large messages
    std::string_view data = buffer.peekContiguous(buffer.size());
    return decodeInternal(data, buffer.getFrontAnchor(), result, bytesRead, depth);
}

RespStatus RespParser::decodeInternal(std::string_view data, std::shared_ptr<BufferSegment> anchor, RespValue& result,
                                      size_t& bytesRead, size_t depth)
{
    bytesRead = 0;
    if (data.empty())
        return RespStatus::Incomplete;

    if (depth > MAX_RECURSION_DEPTH)
        return RespStatus::Invalid;

    char typeChar = data[0];
    switch (typeChar)
    {
    case '+':
        return parseSimpleString(data, result, bytesRead);
    case '-':
        return parseError(data, result, bytesRead);
    case ':':
        return parseInteger(data, result, bytesRead);
    case '$':
        return parseBulkString(data, anchor, result, bytesRead);
    case '*':
        return parseArray(data, anchor, result, bytesRead, depth);
    case '%':
        return parseMap(data, anchor, result, bytesRead, depth);
    case '~':
        return parseSet(data, anchor, result, bytesRead, depth);
    case '#':
        return parseBoolean(data, result, bytesRead);
    case '(':
        return parseBigNumber(data, result, bytesRead);
    default:
        return RespStatus::Invalid;
    }
}

RespStatus RespParser::parseSimpleString(std::string_view data, RespValue& result, size_t& bytesRead)
{
    size_t crlfPos;
    if (!findCRLF(data.data(), data.size(), crlfPos))
        return RespStatus::Incomplete;

    result.type = RespType::SimpleString;
    result.value = std::string(data.data() + 1, crlfPos - 1);
    bytesRead = crlfPos + 2;
    return RespStatus::Ok;
}

RespStatus RespParser::parseError(std::string_view data, RespValue& result, size_t& bytesRead)
{
    size_t crlfPos;
    if (!findCRLF(data.data(), data.size(), crlfPos))
        return RespStatus::Incomplete;

    result.type = RespType::Error;
    result.value = std::string(data.data() + 1, crlfPos - 1);
    bytesRead = crlfPos + 2;
    return RespStatus::Ok;
}

RespStatus RespParser::parseInteger(std::string_view data, RespValue& result, size_t& bytesRead)
{
    size_t crlfPos;
    if (!findCRLF(data.data(), data.size(), crlfPos))
        return RespStatus::Incomplete;

    std::string intStr(data.data() + 1, crlfPos - 1);
    try
    {
        result.value = std::stoll(intStr);
    }
    catch (...)
    {
        return RespStatus::Invalid;
    }

    result.type = RespType::Integer;
    bytesRead = crlfPos + 2;
    return RespStatus::Ok;
}

RespStatus RespParser::parseBulkString(std::string_view data, std::shared_ptr<BufferSegment> anchor, RespValue& result,
                                       size_t& bytesRead)
{
    size_t crlfPos;
    if (!findCRLF(data.data(), data.size(), crlfPos))
        return RespStatus::Incomplete;

    std::string lenStr(data.data() + 1, crlfPos - 1);
    int64_t bulkLen = 0;
    try
    {
        bulkLen = std::stoll(lenStr);
    }
    catch (...)
    {
        return RespStatus::Invalid;
    }

    if (bulkLen == -1)
    {
        result.type = RespType::Null;
        bytesRead = crlfPos + 2;
        return RespStatus::Ok;
    }

    if (bulkLen < 0)
        return RespStatus::Invalid;

    size_t contentStart = crlfPos + 2;
    size_t totalNeeded = contentStart + bulkLen + 2;

    if (data.size() < totalNeeded)
        return RespStatus::Incomplete;

    result.type = RespType::BulkString;
    // ZERO COPY: Point string_view and anchor it
    result.value = std::string_view(data.data() + contentStart, bulkLen);
    result.anchor = anchor;

    bytesRead = totalNeeded;
    return RespStatus::Ok;
}

RespStatus RespParser::parseArray(std::string_view data, std::shared_ptr<BufferSegment> anchor, RespValue& result,
                                  size_t& bytesRead, size_t depth)
{
    size_t crlfPos;
    if (!findCRLF(data.data(), data.size(), crlfPos))
        return RespStatus::Incomplete;

    std::string countStr(data.data() + 1, crlfPos - 1);
    int64_t count = 0;
    try
    {
        count = std::stoll(countStr);
    }
    catch (...)
    {
        return RespStatus::Invalid;
    }

    if (count == -1)
    {
        result.type = RespType::Null;
        bytesRead = crlfPos + 2;
        return RespStatus::Ok;
    }

    if (count < 0)
        return RespStatus::Invalid;

    result.type = RespType::Array;
    std::vector<RespValue> arr;
    arr.reserve(count);

    size_t currentPos = crlfPos + 2;
    for (int64_t i = 0; i < count; ++i)
    {
        RespValue element;
        size_t elemBytes = 0;

        RespStatus status = decodeInternal(data.substr(currentPos), anchor, element, elemBytes, depth + 1);
        if (status != RespStatus::Ok)
            return status;

        arr.push_back(std::move(element));
        currentPos += elemBytes;
    }

    result.value = std::make_shared<std::vector<RespValue>>(std::move(arr));
    bytesRead = currentPos;
    return RespStatus::Ok;
}

RespStatus RespParser::parseMap(std::string_view data, std::shared_ptr<BufferSegment> anchor, RespValue& result,
                                size_t& bytesRead, size_t depth)
{
    size_t crlfPos;
    if (!findCRLF(data.data(), data.size(), crlfPos))
        return RespStatus::Incomplete;

    std::string countStr(data.data() + 1, crlfPos - 1);
    int64_t count = std::stoll(countStr);
    if (count < 0)
        return RespStatus::Invalid;

    result.type = RespType::Map;
    auto map = std::make_shared<std::vector<std::pair<RespValue, RespValue>>>();
    map->reserve(count);

    size_t currentPos = crlfPos + 2;
    for (int64_t i = 0; i < count; ++i)
    {
        RespValue key, val;
        size_t kBytes = 0, vBytes = 0;

        RespStatus s1 = decodeInternal(data.substr(currentPos), anchor, key, kBytes, depth + 1);
        if (s1 != RespStatus::Ok)
            return s1;
        currentPos += kBytes;

        RespStatus s2 = decodeInternal(data.substr(currentPos), anchor, val, vBytes, depth + 1);
        if (s2 != RespStatus::Ok)
            return s2;
        currentPos += vBytes;

        map->push_back({std::move(key), std::move(val)});
    }

    result.value = map;
    bytesRead = currentPos;
    return RespStatus::Ok;
}

RespStatus RespParser::parseSet(std::string_view data, std::shared_ptr<BufferSegment> anchor, RespValue& result,
                                size_t& bytesRead, size_t depth)
{
    RespStatus status = parseArray(data, anchor, result, bytesRead, depth);
    if (status == RespStatus::Ok)
    {
        result.type = RespType::Set;
    }
    return status;
}

RespStatus RespParser::parseBoolean(std::string_view data, RespValue& result, size_t& bytesRead)
{
    if (data.size() < 4)
        return RespStatus::Incomplete;
    if (data[0] != '#' || data[2] != '\r' || data[3] != '\n')
        return RespStatus::Invalid;

    result.type = RespType::Boolean;
    result.value = (data[1] == 't');
    bytesRead = 4;
    return RespStatus::Ok;
}

RespStatus RespParser::parseBigNumber(std::string_view data, RespValue& result, size_t& bytesRead)
{
    size_t crlfPos;
    if (!findCRLF(data.data(), data.size(), crlfPos))
        return RespStatus::Incomplete;

    result.type = RespType::BigNumber;
    result.value = std::string(data.data() + 1, crlfPos - 1);
    bytesRead = crlfPos + 2;
    return RespStatus::Ok;
}