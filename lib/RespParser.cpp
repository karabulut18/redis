#include "RespParser.h"
#include <cstring>
#include <string>

bool RespParser::findCRLF(const char *data, size_t length, size_t &pos)
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

RespStatus RespParser::decode(const char *data, size_t length, RespValue &result, size_t &bytesRead)
{
    bytesRead = 0;
    if (length == 0)
        return RespStatus::Incomplete;

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
        return parseArray(data, length, result, bytesRead);
    default:
        return RespStatus::Invalid;
    }
}

RespStatus RespParser::parseSimpleString(const char *data, size_t length, RespValue &result, size_t &bytesRead)
{
    size_t crlfPos;
    if (!findCRLF(data, length, crlfPos))
    {
        return RespStatus::Incomplete;
    }

    result.type = RespType::SimpleString;
    result.str_val = std::string(data + 1, crlfPos - 1);

    bytesRead = crlfPos + 2; // +2 for \r\n
    return RespStatus::Ok;
}

RespStatus RespParser::parseError(const char *data, size_t length, RespValue &result, size_t &bytesRead)
{
    size_t crlfPos;
    if (!findCRLF(data, length, crlfPos))
    {
        return RespStatus::Incomplete;
    }

    result.type = RespType::Error;
    result.str_val = std::string(data + 1, crlfPos - 1);

    bytesRead = crlfPos + 2;
    return RespStatus::Ok;
}

RespStatus RespParser::parseInteger(const char *data, size_t length, RespValue &result, size_t &bytesRead)
{
    size_t crlfPos;
    if (!findCRLF(data, length, crlfPos))
    {
        return RespStatus::Incomplete;
    }

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

RespStatus RespParser::parseBulkString(const char *data, size_t length, RespValue &result, size_t &bytesRead)
{
    size_t crlfPos;
    if (!findCRLF(data, length, crlfPos))
    {
        return RespStatus::Incomplete;
    }

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
        result.type = RespType::Null; // RESP2 Null Bulk String
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

RespStatus RespParser::parseArray(const char *data, size_t length, RespValue &result, size_t &bytesRead)
{
    size_t crlfPos;
    if (!findCRLF(data, length, crlfPos))
    {
        return RespStatus::Incomplete;
    }

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

        // Recursive call
        RespStatus status = decode(data + currentPos, length - currentPos, element, elemBytes);

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