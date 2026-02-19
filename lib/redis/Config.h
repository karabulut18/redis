#pragma once
#include <string>

// Configuration loaded from a redis.conf-style key-value file.
// Keys and values are separated by whitespace; lines beginning with '#' are
// comments and are ignored. Unknown keys are silently skipped.
struct ServerConfig
{
    int port = 6379;
    std::string appendfilename = "appendonly.aof";
    // appendfsync: "always" | "everysec" | "no"
    std::string appendfsync = "everysec";
    int appendfsync_interval = 1; // seconds; used when appendfsync == "everysec"
};

// Parse a redis.conf-style file from `path`.
// Returns a ServerConfig populated with values found in the file.
// If `path` cannot be opened, the returned struct contains all defaults.
ServerConfig ParseConfig(const std::string& path);
