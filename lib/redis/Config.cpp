#include "Config.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

ServerConfig ParseConfig(const std::string& path)
{
    ServerConfig cfg;

    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "[Config] Could not open '" << path << "', using defaults.\n";
        return cfg;
    }

    std::string line;
    while (std::getline(file, line))
    {
        // Strip leading whitespace
        auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos)
            continue;
        line = line.substr(start);

        // Skip comments and empty lines
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        std::string key, value;
        if (!(iss >> key >> value))
            continue;

        // Lower-case the key for case-insensitive matching
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        if (key == "port")
        {
            try
            {
                cfg.port = std::stoi(value);
            }
            catch (...)
            {
                std::cerr << "[Config] Invalid port value: " << value << "\n";
            }
        }
        else if (key == "appendfilename")
        {
            // Strip surrounding quotes if present
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
                value = value.substr(1, value.size() - 2);
            cfg.appendfilename = value;
        }
        else if (key == "appendfsync")
        {
            cfg.appendfsync = value;
        }
        else if (key == "appendfsync-interval")
        {
            try
            {
                cfg.appendfsync_interval = std::stoi(value);
            }
            catch (...)
            {
                std::cerr << "[Config] Invalid appendfsync-interval value: " << value << "\n";
            }
        }
        // Unknown keys are silently ignored (real redis.conf compat)
    }

    std::cout << "[Config] Loaded '" << path << "': port=" << cfg.port << ", aof=" << cfg.appendfilename
              << ", appendfsync=" << cfg.appendfsync << "\n";
    return cfg;
}
