#include "RespParser.h"
#include <_types/_uint8_t.h>
#include <sys/types.h>

bool RespParser::parse(const char *data)
{
    uint64_t index = 0;
    uint8_t tag = data[index++];
    const char *payload = data + index;

    switch (tag)
    {
    case TAG_NIL:
        parse_null(payload);
        break;
    case TAG_ERR:
        parse_error(payload);
        break;
    case TAG_STR:
        parse_string(payload);
        break;
    case TAG_INT:
        parse_int(payload);
        break;
    case TAG_DBL:
        parse_double(payload);
        break;
    case TAG_ARR:
        parse_array(payload);
        break;
    default:
        return false;
    }
    return true;
};

void RespParser::parse_null(const char *data)
{
    _callbackOwner->on_null();
}

void RespParser::parse_error(const char *data)
{
    uint64_t index = 0;
    uint8_t errorTag = data[index++];
    uint64_t errorMessagelength = 0;
    memcpy(&errorMessagelength, data, sizeof(errorMessagelength));
    index += sizeof(errorMessagelength);
    char *errorMessage = (char *)calloc(errorMessagelength + 1, sizeof(char));
    memcpy(errorMessage, data + index, errorMessagelength);
    _callbackOwner->on_error(errorTag, errorMessagelength, errorMessage);
    free(errorMessage);
}

void RespParser::parse_string(const char *data)
{
    uint64_t index = 0;
    uint64_t stringLength = 0;
    memcpy(&stringLength, data, sizeof(stringLength));
    index += sizeof(stringLength);
    char *stringBuff = (char *)calloc(stringLength + 1, sizeof(char));
    memcpy(stringBuff, data + index, stringLength);
    _callbackOwner->on_str(stringLength, stringBuff);
    free(stringBuff);
}

void RespParser::parse_int(const char *data)
{
    int64_t number = 0;
    memcpy(&number, data, sizeof(number));
    _callbackOwner->on_int(number);
}

void RespParser::parse_double(const char *data)
{
    double_t number = 0;
    memcpy(&number, data, sizeof(number));
    _callbackOwner->on_dbl(number);
}

void RespParser::parse_array(const char *data)
{
    uint64_t arrayLength = 0;
    uint64_t index = 0;
    memcpy(&arrayLength, data, sizeof(arrayLength));
    index += sizeof(arrayLength);
    int64_t *array = (int64_t *)calloc(arrayLength, sizeof(int64_t));
    memcpy(array, data + index, arrayLength);
    _callbackOwner->on_arr(arrayLength, array);
    free(array);
}