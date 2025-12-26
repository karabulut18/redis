#include "RespParser.h"
#include <cstring>
#include <string>

bool RespParser::findCRLF(const char* data, size_t length, size_t& pos)
{
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
        out += "+" + value.str_val + "\r\n";
        break;
    case RespType::Error:
        out += "-" + value.str_val + "\r\n";
        break;
    case RespType::Integer:
        out += ":" + std::to_string(value.int_val) + "\r\n";
        break;
    case RespType::BulkString:
        out += "$" + std::to_string(value.str_val.length()) + "\r\n" + value.str_val + "\r\n";
        break;
    case RespType::Null:
        out += "$-1\r\n";
        break;
    case RespType::Array:
        out += "*" + std::to_string(value.array_val.size()) + "\r\n";
        for (const auto& item : value.array_val)
            out += encode(item);
        break;
    case RespType::Map:
        out += "%" + std::to_string(value.map_val.size()) + "\r\n";
        for (const auto& item : value.map_val)
            out += encode(item.first) + encode(item.second);
        break;
    case RespType::Set:
        out += "~" + std::to_string(value.set_val.size()) + "\r\n";
        for (const auto& item : value.set_val)
            out += encode(item);
        break;
    case RespType::Boolean:
        out += "#" + std::string(value.bool_val ? "t" : "f") + "\r\n";
        break;
    case RespType::BigNumber:
        out += "(" + value.str_val + "\r\n";
        break;
    default:
        break;
    }
    return out;
}

RespStatus RespParser::decode(const char* data, size_t length, RespValue& result, size_t& bytesRead, size_t depth)
{
    bytesRead = 0;
    if (length == 0)
        return RespStatus::Incomplete;

    if (depth > MAX_RECURSION_DEPTH)
        return RespStatus::Invalid;

    char type = data[0];
    switch (type)
    {
    case '+':
        return parseSimpleString(data, length, result, bytesRead);
    case '-':
        return parseError(data, length, result, bytesRead);
    case ':':
        return parseInteger(data, length, result, bytesRead);
    case '$':
        return parseBulkString(data, length, result, bytesRead);
    case '*':
        return parseArray(data, length, result, bytesRead, depth);
    case '%':
        return parseMap(data, length, result, bytesRead, depth);
    case '~':
        return parseSet(data, length, result, bytesRead, depth);
    case '#':
        return parseBoolean(data, length, result, bytesRead);
    case '(':
        return parseBigNumber(data, length, result, bytesRead);
    default:
        return RespStatus::Invalid;
    }
}

RespStatus RespParser::parseSimpleString(const char* data, size_t length, RespValue& result, size_t& bytesRead)
{
    size_t crlfPos;
    if (!findCRLF(data, length, crlfPos))
        return RespStatus::Incomplete;

    result.type = RespType::SimpleString;
    result.str_val = std::string(data + 1, crlfPos - 1);

    bytesRead = crlfPos + 2; // +2 for \r\n
    return RespStatus::Ok;
}

RespStatus RespParser::parseError(const char* data, size_t length, RespValue& result, size_t& bytesRead)
{
    size_t crlfPos;
    if (!findCRLF(data, length, crlfPos))
        return RespStatus::Incomplete;

    result.type = RespType::Error;
    result.str_val = std::string(data + 1, crlfPos - 1);

    bytesRead = crlfPos + 2;
    return RespStatus::Ok;
}

RespStatus RespParser::parseInteger(const char* data, size_t length, RespValue& result, size_t& bytesRead)
{
    size_t crlfPos;
    if (!findCRLF(data, length, crlfPos))
        return RespStatus::Incomplete;

    result.type = RespType::Integer;
    std::string intStr(data + 1, crlfPos - 1);

    try
    {
        result.int_val = std::stoll(intStr);
    }
    catch (...)
    {
        return RespStatus::Invalid;
    }

    bytesRead = crlfPos + 2;
    return RespStatus::Ok;
}

RespStatus RespParser::parseBulkString(const char* data, size_t length, RespValue& result, size_t& bytesRead)
{
    size_t crlfPos;
    if (!findCRLF(data, length, crlfPos))
        return RespStatus::Incomplete;

    std::string lenStr(data + 1, crlfPos - 1);
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

    // Check if we have enough data: header + \r\n + content + \r\n
    size_t contentStart = crlfPos + 2;
    size_t totalNeeded = contentStart + bulkLen + 2;

    if (length < totalNeeded)
    {
        return RespStatus::Incomplete;
    }

    result.type = RespType::BulkString;
    result.str_val = std::string(data + contentStart, bulkLen);

    bytesRead = totalNeeded;
    return RespStatus::Ok;
}

RespStatus RespParser::parseArray(const char* data, size_t length, RespValue& result, size_t& bytesRead, size_t depth)
{
    size_t crlfPos;
    if (!findCRLF(data, length, crlfPos))
        return RespStatus::Incomplete;

    std::string countStr(data + 1, crlfPos - 1);
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
        result.type = RespType::Null; // RESP2 Null Array
        bytesRead = crlfPos + 2;
        return RespStatus::Ok;
    }

    if (count < 0)
        return RespStatus::Invalid;

    result.type = RespType::Array;
    result.array_val.clear();
    result.array_val.reserve(count);

    size_t currentPos = crlfPos + 2;
    for (int64_t i = 0; i < count; ++i)
    {
        RespValue element;
        size_t elemBytes = 0;

        // Recursive call with depth + 1
        RespStatus status = decode(data + currentPos, length - currentPos, element, elemBytes, depth + 1);

        if (status != RespStatus::Ok)
        {
            return status; // Incomplete or Invalid
        }

        result.array_val.push_back(std::move(element));
        currentPos += elemBytes;
    }

    bytesRead = currentPos;
    return RespStatus::Ok;
}

RespStatus RespParser::parseMap(const char* data, size_t length, RespValue& result, size_t& bytesRead, size_t depth)
{
    size_t crlfPos;
    if (!findCRLF(data, length, crlfPos))
        return RespStatus::Incomplete;

    std::string countStr(data + 1, crlfPos - 1);
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

    result.type = RespType::Map;
    result.map_val.clear();
    result.map_val.reserve(count);

    size_t currentPos = crlfPos + 2;
    for (int64_t i = 0; i < count; ++i)
    {
        RespValue element;
        size_t elemBytes = 0;

        // Recursive call with depth + 1
        RespValue key;
        RespValue value;
        RespStatus status = decode(data + currentPos, length - currentPos, key, elemBytes, depth + 1);

        if (status != RespStatus::Ok)
            return status;
        currentPos += elemBytes;

        status = decode(data + currentPos, length - currentPos, value, elemBytes, depth + 1);
        if (status != RespStatus::Ok)
            return status;

        result.map_val.push_back(std::make_pair(key, value));
        currentPos += elemBytes;
    }

    bytesRead = currentPos;
    return RespStatus::Ok;
}

RespStatus RespParser::parseSet(const char* data, size_t length, RespValue& result, size_t& bytesRead, size_t depth)
{
    size_t crlfPos;
    if (!findCRLF(data, length, crlfPos))
        return RespStatus::Incomplete;

    std::string countStr(data + 1, crlfPos - 1);
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
        result.type = RespType::Null; // RESP2 Null Array
        bytesRead = crlfPos + 2;
        return RespStatus::Ok;
    }

    if (count < 0)
        return RespStatus::Invalid;

    result.type = RespType::Set;
    result.set_val.clear();
    result.set_val.reserve(count);

    size_t currentPos = crlfPos + 2;
    for (int64_t i = 0; i < count; ++i)
    {
        RespValue element;
        size_t elemBytes = 0;

        // Recursive call with depth + 1
        RespStatus status = decode(data + currentPos, length - currentPos, element, elemBytes, depth + 1);

        if (status != RespStatus::Ok)
        {
            return status; // Incomplete or Invalid
        }

        result.set_val.push_back(std::move(element));
        currentPos += elemBytes;
    }

    bytesRead = currentPos;
    return RespStatus::Ok;
}

RespStatus RespParser::parseBoolean(const char* data, size_t length, RespValue& result, size_t& bytesRead)
{
    size_t crlfPos;
    if (!findCRLF(data, length, crlfPos))
        return RespStatus::Incomplete;

    // data[0] is '#', value is at data[1]
    if (crlfPos != 2) // Should be #t\r\n (pos 2) or #f\r\n
        return RespStatus::Invalid;

    char val = data[1];
    if (val == 't')
        result.bool_val = true;
    else if (val == 'f')
        result.bool_val = false;
    else
        return RespStatus::Invalid;

    result.type = RespType::Boolean;
    bytesRead = crlfPos + 2;
    return RespStatus::Ok;
}

RespStatus RespParser::parseBigNumber(const char* data, size_t length, RespValue& result, size_t& bytesRead)
{
    size_t crlfPos;
    if (!findCRLF(data, length, crlfPos))
        return RespStatus::Incomplete;

    result.type = RespType::BigNumber;
    result.str_val = std::string(data + 1, crlfPos - 1);

    bytesRead = crlfPos + 2;
    return RespStatus::Ok;
}