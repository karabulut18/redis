#include "../lib/redis/Persistence.h"
#include "../lib/redis/RespParser.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// Count how many RESP arrays (commands) are in the AOF file.
// Each command starts with '*' on its own line.
static int countCommandsInFile(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        return -1;

    RespParser parser;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    int count = 0;
    size_t offset = 0;
    while (offset < content.size())
    {
        RespValue val;
        size_t consumed = 0;
        RespStatus status = parser.decode(content.data() + offset, content.size() - offset, val, consumed);
        if (status == RespStatus::Incomplete || status == RespStatus::Invalid)
            break;
        if (val.type == RespType::Array)
            count++;
        offset += consumed;
    }
    return count;
}

void test_aof_no_double_write_always_flush()
{
    std::cout << "Persistence: always-flush mode writes each command exactly once... ";
    const std::string tmpPath = "/tmp/test_aof_always_flush.aof";
    std::remove(tmpPath.c_str());

    {
        Persistence p(tmpPath);
        p.SetFlushInterval(0); // always-flush mode

        // Append 3 commands
        RespValue setCmd;
        setCmd.type = RespType::Array;
        setCmd.setArray({});
        auto makeArg = [](const std::string& s)
        {
            RespValue v;
            v.type = RespType::BulkString;
            v.value = s;
            return v;
        };

        // SET foo bar
        {
            std::vector<RespValue> args = {makeArg("SET"), makeArg("foo"), makeArg("bar")};
            p.Append(args);
        }
        // SET baz qux
        {
            std::vector<RespValue> args = {makeArg("SET"), makeArg("baz"), makeArg("qux")};
            p.Append(args);
        }
        // DEL foo
        {
            std::vector<RespValue> args = {makeArg("DEL"), makeArg("foo")};
            p.Append(args);
        }
    } // Persistence destructor flushes

    int count = countCommandsInFile(tmpPath);
    // Must be exactly 3 — not 6 (which would indicate double-write)
    assert(count == 3 && "AOF double-write bug: expected 3 commands, got more");

    std::remove(tmpPath.c_str());
    std::cout << "PASS\n";
}

void test_aof_no_double_write_periodic_flush()
{
    std::cout << "Persistence: periodic-flush mode writes each command exactly once... ";
    const std::string tmpPath = "/tmp/test_aof_periodic.aof";
    std::remove(tmpPath.c_str());

    {
        Persistence p(tmpPath);
        p.SetFlushInterval(1); // periodic mode

        auto makeArg = [](const std::string& s)
        {
            RespValue v;
            v.type = RespType::BulkString;
            v.value = s;
            return v;
        };

        std::vector<RespValue> args1 = {makeArg("SET"), makeArg("k1"), makeArg("v1")};
        std::vector<RespValue> args2 = {makeArg("SET"), makeArg("k2"), makeArg("v2")};
        p.Append(args1);
        p.Append(args2);
        p.Flush();
    }

    int count = countCommandsInFile(tmpPath);
    assert(count == 2 && "Periodic flush: expected exactly 2 commands");

    std::remove(tmpPath.c_str());
    std::cout << "PASS\n";
}

void test_aof_load_roundtrip()
{
    std::cout << "Persistence: load replays correct number of commands... ";
    const std::string tmpPath = "/tmp/test_aof_roundtrip.aof";
    std::remove(tmpPath.c_str());

    auto makeArg = [](const std::string& s)
    {
        RespValue v;
        v.type = RespType::BulkString;
        v.value = s;
        return v;
    };

    // Write 5 commands
    {
        Persistence p(tmpPath);
        p.SetFlushInterval(0);
        for (int i = 0; i < 5; i++)
        {
            std::vector<RespValue> args = {makeArg("SET"), makeArg("k" + std::to_string(i)),
                                           makeArg("v" + std::to_string(i))};
            p.Append(args);
        }
    }

    // Replay and count
    int replayCount = 0;
    {
        Persistence p(tmpPath);
        p.Load([&](const std::vector<std::string>& args) { replayCount++; });
    }

    assert(replayCount == 5 && "AOF load: expected 5 replayed commands");
    std::remove(tmpPath.c_str());
    std::cout << "PASS\n";
}

int main()
{
    std::cout << "=== Persistence Tests ===\n";
    test_aof_no_double_write_always_flush();
    test_aof_no_double_write_periodic_flush();
    test_aof_load_roundtrip();
    std::cout << "All Persistence tests passed!\n";
    return 0;
}
