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
    std::string input = "+OK\r\n";
    RespValue result;
    size_t bytesRead = 0;
    RespParser parser;
    RespStatus status = parser.decode(input.c_str(), input.length(), result, bytesRead);

    assert(status == RespStatus::Ok);
    assert(result.type == RespType::SimpleString);
    assert(result.toString() == "OK");
    assert(bytesRead == input.length());
    std::cout << "PASS\n";
}

void test_decode_error()
{
    std::cout << "RespParser: decode error... ";
    std::string input = "-Error message\r\n";
    RespValue result;
    size_t bytesRead = 0;
    RespParser parser;
    RespStatus status = parser.decode(input.c_str(), input.length(), result, bytesRead);

    assert(status == RespStatus::Ok);
    assert(result.type == RespType::Error);
    assert(result.toString() == "Error message");
    std::cout << "PASS\n";
}

void test_decode_integer()
{
    std::cout << "RespParser: decode integer... ";
    std::string input = ":1000\r\n";
    RespValue result;
    size_t bytesRead = 0;
    RespParser parser;
    RespStatus status = parser.decode(input.c_str(), input.length(), result, bytesRead);

    assert(status == RespStatus::Ok);
    assert(result.type == RespType::Integer);
    assert(std::get<int64_t>(result.value) == 1000);
    std::cout << "PASS\n";
}

void test_decode_bulk_string()
{
    std::cout << "RespParser: decode bulk string... ";
    std::string input = "$6\r\nfoobar\r\n";
    RespValue result;
    size_t bytesRead = 0;
    RespParser parser;
    RespStatus status = parser.decode(input.c_str(), input.length(), result, bytesRead);

    assert(status == RespStatus::Ok);
    assert(result.type == RespType::BulkString);
    assert(result.toString() == "foobar");
    std::cout << "PASS\n";
}

void test_decode_array()
{
    std::cout << "RespParser: decode array... ";
    std::string input = "*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
    RespValue result;
    size_t bytesRead = 0;
    RespParser parser;
    RespStatus status = parser.decode(input.c_str(), input.length(), result, bytesRead);

    assert(status == RespStatus::Ok);
    assert(result.type == RespType::Array);
    assert(result.getArray().size() == 2);
    assert(result.getArray()[0].toString() == "foo");
    assert(result.getArray()[1].toString() == "bar");
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
    assert(std::get<int64_t>(result.value) == -2);
    std::cout << "PASS\n";
}

void test_roundtrip()
{
    std::cout << "RespParser: encode->decode roundtrip... ";
    RespParser parser;

    // Build an array: ["PING"]
    RespValue cmd;
    cmd.type = RespType::Array;
    std::vector<RespValue> arr;
    RespValue elem;
    elem.type = RespType::BulkString;
    elem.value = std::string_view("PING");
    arr.push_back(elem);
    cmd.setArray(std::move(arr));

    std::string encoded = RespParser::encode(cmd);

    // Decode it back
    RespValue decoded;
    size_t bytesRead = 0;
    RespStatus status = parser.decode(encoded.c_str(), encoded.size(), decoded, bytesRead);

    assert(status == RespStatus::Ok);
    assert(decoded.type == RespType::Array);
    assert(decoded.getArray().size() == 1);
    assert(decoded.getArray()[0].toString() == "PING");
    std::cout << "PASS\n";
}

void test_zero_copy_anchor()
{
    std::cout << "RespParser: zero-copy anchor verification... ";
    SegmentedBuffer buffer;
    std::string input = "$6\r\nfoobar\r\n";
    buffer.append(input.data(), input.size());

    RespValue result;
    size_t bytesRead = 0;
    RespParser parser;

    // Parse using SegmentedBuffer to get the anchor
    RespStatus status = parser.decode(buffer, result, bytesRead);
    assert(status == RespStatus::Ok);
    assert(result.type == RespType::BulkString);
    assert(result.toString() == "foobar");

    // The result MUST hold an anchor to the buffer's segment
    assert(result.anchor != nullptr);

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
    test_decode_error();
    test_decode_integer();
    test_decode_bulk_string();
    test_decode_array();
    test_decode_incomplete();
    test_decode_negative_integer();
    test_roundtrip();
    test_zero_copy_anchor();
    std::cout << "All RespParser tests passed!\n";
    return 0;
}
