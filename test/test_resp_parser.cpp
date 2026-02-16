#include "../lib/redis/RespParser.h"
#include <cassert>
#include <iostream>
#include <string>

void test_encode_simple_string()
{
    std::cout << "RespParser: encode simple string... ";
    RespValue val;
    val.type = RespType::SimpleString;
    val.value = std::string_view("OK");
    assert(RespParser::encode(val) == "+OK\r\n");
    std::cout << "PASS\n";
}

void test_encode_error()
{
    std::cout << "RespParser: encode error... ";
    RespValue val;
    val.type = RespType::Error;
    val.value = std::string_view("ERR unknown command");
    assert(RespParser::encode(val) == "-ERR unknown command\r\n");
    std::cout << "PASS\n";
}

void test_encode_integer()
{
    std::cout << "RespParser: encode integer... ";
    RespValue val;
    val.type = RespType::Integer;
    val.value = int64_t(42);
    assert(RespParser::encode(val) == ":42\r\n");
    std::cout << "PASS\n";
}

void test_encode_bulk_string()
{
    std::cout << "RespParser: encode bulk string... ";
    RespValue val;
    val.type = RespType::BulkString;
    val.value = std::string_view("hello");
    assert(RespParser::encode(val) == "$5\r\nhello\r\n");
    std::cout << "PASS\n";
}

void test_encode_null()
{
    std::cout << "RespParser: encode null... ";
    RespValue val;
    val.type = RespType::Null;
    assert(RespParser::encode(val) == "$-1\r\n");
    std::cout << "PASS\n";
}

void test_decode_simple_string()
{
    std::cout << "RespParser: decode simple string... ";
    RespParser parser;
    RespValue result;
    size_t bytesRead = 0;

    std::string input = "+PONG\r\n";
    RespStatus status = parser.decode(input.c_str(), input.size(), result, bytesRead);

    assert(status == RespStatus::Ok);
    assert(result.type == RespType::SimpleString);
    assert(result.getString() == "PONG");
    assert(bytesRead == input.size());
    std::cout << "PASS\n";
}

void test_decode_integer()
{
    std::cout << "RespParser: decode integer... ";
    RespParser parser;
    RespValue result;
    size_t bytesRead = 0;

    std::string input = ":1000\r\n";
    RespStatus status = parser.decode(input.c_str(), input.size(), result, bytesRead);

    assert(status == RespStatus::Ok);
    assert(result.type == RespType::Integer);
    assert(result.getInt() == 1000);
    std::cout << "PASS\n";
}

void test_decode_bulk_string()
{
    std::cout << "RespParser: decode bulk string... ";
    RespParser parser;
    RespValue result;
    size_t bytesRead = 0;

    std::string input = "$5\r\nhello\r\n";
    RespStatus status = parser.decode(input.c_str(), input.size(), result, bytesRead);

    assert(status == RespStatus::Ok);
    assert(result.type == RespType::BulkString);
    assert(result.getString() == "hello");
    std::cout << "PASS\n";
}

void test_decode_array()
{
    std::cout << "RespParser: decode array (SET command)... ";
    RespParser parser;
    RespValue result;
    size_t bytesRead = 0;

    // *3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n
    std::string input = "*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n";
    RespStatus status = parser.decode(input.c_str(), input.size(), result, bytesRead);

    assert(status == RespStatus::Ok);
    assert(result.type == RespType::Array);
    auto& arr = result.getArray();
    assert(arr.size() == 3);
    assert(arr[0].getString() == "SET");
    assert(arr[1].getString() == "key");
    assert(arr[2].getString() == "value");
    std::cout << "PASS\n";
}

void test_decode_incomplete()
{
    std::cout << "RespParser: decode incomplete message... ";
    RespParser parser;
    RespValue result;
    size_t bytesRead = 0;

    std::string input = "$5\r\nhel"; // incomplete bulk string
    RespStatus status = parser.decode(input.c_str(), input.size(), result, bytesRead);

    assert(status == RespStatus::Incomplete);
    std::cout << "PASS\n";
}

void test_decode_negative_integer()
{
    std::cout << "RespParser: decode negative integer... ";
    RespParser parser;
    RespValue result;
    size_t bytesRead = 0;

    std::string input = ":-2\r\n";
    RespStatus status = parser.decode(input.c_str(), input.size(), result, bytesRead);

    assert(status == RespStatus::Ok);
    assert(result.type == RespType::Integer);
    assert(result.getInt() == -2);
    std::cout << "PASS\n";
}

void test_roundtrip()
{
    std::cout << "RespParser: encode→decode roundtrip... ";
    RespParser parser;

    // Build an array: ["PING"]
    RespValue cmd;
    cmd.type = RespType::Array;
    std::vector<RespValue> arr;
    RespValue elem;
    elem.type = RespType::BulkString;
    elem.value = std::string_view("PING");
    arr.push_back(elem);
    cmd.value = arr;

    std::string encoded = RespParser::encode(cmd);

    // Decode it back
    RespValue decoded;
    size_t bytesRead = 0;
    RespStatus status = parser.decode(encoded.c_str(), encoded.size(), decoded, bytesRead);

    assert(status == RespStatus::Ok);
    assert(decoded.type == RespType::Array);
    assert(decoded.getArray().size() == 1);
    assert(decoded.getArray()[0].getString() == "PING");
    std::cout << "PASS\n";
}

int main()
{
    std::cout << "=== RespParser Tests ===\n";
    test_encode_simple_string();
    test_encode_error();
    test_encode_integer();
    test_encode_bulk_string();
    test_encode_null();
    test_decode_simple_string();
    test_decode_integer();
    test_decode_bulk_string();
    test_decode_array();
    test_decode_incomplete();
    test_decode_negative_integer();
    test_roundtrip();
    std::cout << "All RespParser tests passed!\n";
    return 0;
}
