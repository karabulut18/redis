#include "Output.h"

#include <iostream>
#include <fstream>

Output* Output::GetInstance()
{
    static Output* instance = new Output();
    return instance;
}

void Output::Init(std::string appName, std::string outputFile)
{
    if(outputFile == "")
        _outputFile = appName;
    else
        _outputFile = outputFile;

    _appName = appName;
    _outputFileSet = true;
    _outputFile = _outputFile +  "_" + GetTimeString_HHMMSS(&delimeterInFileName) + ".log";


    // if output file exists, delete it
    std::ifstream file(_outputFile);
    if(file.good())
        std::remove(_outputFile.c_str());
    std::string initLine = _outputFile + " initiated by " + _appName;
    WriteLine_F(initLine);
}

void Output::Write_F(std::string ss)
{
    if(_outputFileSet)
    {
        std::ofstream file(_outputFile, std::ios::app);
        file << ss;
        file.close();
    }
}

void Output::WriteLine_F(std::string line)
{
    if(_outputFileSet)
    {
        std::ofstream file(_outputFile, std::ios::app);
        file << GetTimeString_HHMMSS(&delimeterInLine) << " " << line << std::endl;
        file.close();
    }
}

void Output::WriteLine_FC(std::string ss)
{
    WriteLine_F(ss);
    WriteLine_C(ss);
}

void Output::WriteLine_C(std::string line)
{
    std::cout << line << std::endl;
}

std::string Output::GetTimeString_HHMMSS(const char* delimeter)
{
    std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);

    struct tm time_parts;
    localtime_r(&now_c, &time_parts);

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d%s%02d%s%02d", time_parts.tm_hour, delimeter, time_parts.tm_min, delimeter, time_parts.tm_sec);
    return std::string(buffer);
}