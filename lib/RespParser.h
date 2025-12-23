#pragma once
#include <_types/_uint64_t.h>
#include <cmath>
#include <cstdint>
#include <string>

// a prototype for resp parsing
// later it will be advanced to be compatible with real resp

enum resp_tag
{
    TAG_NIL, // nil
    TAG_ERR, // error code + msg
    TAG_STR, // string
    TAG_INT, // int64
    TAG_DBL, // double
    TAG_ARR, // array
};

enum error_code
{
    EC_OK,
    EC_ERR,
};

class RespParser_Callback
{
public:
    virtual void on_error(uint8_t errorTag, uint64_t errorMessagelenght, const char *errorMessage) = 0;
    virtual void on_str(uint64_t stringLength, const char *stringBuff) = 0;
    virtual void on_int(int64_t number) = 0;
    virtual void on_dbl(double_t number) = 0;
    virtual void on_null() = 0;
    virtual void on_arr(uint64_t arrayLength, int64_t *array) = 0;
};

class RespParser
{
    RespParser_Callback *_callbackOwner = nullptr;

public:
    RespParser(RespParser_Callback *callbackOwner) : _callbackOwner(callbackOwner){};
    bool parse(const char *data);

private:
    void parse_undefined(const char *data);
    void parse_null(const char *data);
    void parse_error(const char *data);
    void parse_string(const char *data);
    void parse_int(const char *data);
    void parse_double(const char *data);
    void parse_array(const char *data);
};