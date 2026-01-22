#include "Error.h"
#include "string.h"


Error::Error()
{
    _isSet = false;
};

void Error::Set(int code, const char* name)
{
    _isSet = true;
    _code = code;
    strncpy(_name, name, ERROR_NAME_SIZE-1);
    _name[ERROR_NAME_SIZE-1] = '\0';
};

void Error::Clear()
{
    _isSet = false;
    _code = 0;
    memset(_name, 0, ERROR_NAME_SIZE);
};