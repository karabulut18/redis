#pragma once

#include <string>

#define PUTF(message) Output::GetInstance()->Write_F(message)
#define PUTF_LN(message) Output::GetInstance()->WriteLine_FC(message)
#define PUTFC_LN(message) Output::GetInstance()->WriteLine_FC(message)

class Output
{
    std::string _appName;
    bool _outputFileSet = false;
    std::string _outputFile;
    std::string GetTimeString_HHMMSS(char delimeter);

    const char c_delimeterInLine = ':';
    const char c_delimeterInFileName = '_';

public:
    static Output* GetInstance();
    void Init(std::string appName, std::string outputFile = "");

    void Write_F(std::string ss);
    void WriteLine_FC(std::string line);
    void WriteLine_F(std::string line);
    void WriteLine_C(std::string line);
};