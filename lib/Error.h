#pragma once

#define ERROR_NAME_SIZE 256


class Error
{
public:
    bool    _isSet;
    int     _code;
    char    _name[ERROR_NAME_SIZE];
    Error();

    void Set(int code, const char* name);
    void Clear();
};
