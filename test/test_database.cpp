#include "../lib/redis/Database.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

void test_set_get_del()
{
    std::cout << "Database: SET/GET/DEL basics... ";
    Database db;

    // SET new key
    assert(db.set("name", "alice") == true);
    assert(db.size() == 1);

    // GET existing
    const std::string* val = db.get("name");
    assert(val != nullptr);
    assert(*val == "alice");

    // SET overwrite
    assert(db.set("name", "bob") == false);
    val = db.get("name");
    assert(*val == "bob");

    // GET missing
    assert(db.get("missing") == nullptr);

    // DEL existing
    assert(db.del("name") == true);
    assert(db.size() == 0);
    assert(db.get("name") == nullptr);

    // DEL missing
    assert(db.del("name") == false);

    std::cout << "PASS\n";
}

void test_many_keys()
{
    std::cout << "Database: many keys... ";
    Database db;

    for (int i = 0; i < 500; i++)
        db.set("key" + std::to_string(i), "val" + std::to_string(i));

    assert(db.size() == 500);

    for (int i = 0; i < 500; i++)
    {
        const std::string* val = db.get("key" + std::to_string(i));
        assert(val != nullptr);
        assert(*val == "val" + std::to_string(i));
    }

    // Delete odd keys
    for (int i = 1; i < 500; i += 2)
        assert(db.del("key" + std::to_string(i)));

    assert(db.size() == 250);

    // Verify even keys remain
    for (int i = 0; i < 500; i += 2)
    {
        const std::string* val = db.get("key" + std::to_string(i));
        assert(val != nullptr);
    }

    std::cout << "PASS\n";
}

void test_ttl_set_with_expiry()
{
    std::cout << "Database: SET with TTL... ";
    Database db;

    // Set with 200ms TTL
    db.set("temp", "data", 200);

    // Should be accessible immediately
    assert(db.get("temp") != nullptr);
    assert(*db.get("temp") == "data");

    // PTTL should be roughly 200ms
    int64_t ttl = db.pttl("temp");
    assert(ttl > 0 && ttl <= 200);

    // Wait for expiry
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // Should be gone (lazy expiration)
    assert(db.get("temp") == nullptr);
    assert(db.pttl("temp") == -2); // key doesn't exist

    std::cout << "PASS\n";
}

void test_expire_command()
{
    std::cout << "Database: EXPIRE on existing key... ";
    Database db;

    db.set("persistent", "value");
    assert(db.pttl("persistent") == -1); // no expiry

    // Set expiry
    assert(db.expire("persistent", 200) == true);
    int64_t ttl = db.pttl("persistent");
    assert(ttl > 0 && ttl <= 200);

    // EXPIRE on missing key
    assert(db.expire("missing", 100) == false);

    // Wait and verify
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    assert(db.get("persistent") == nullptr);

    std::cout << "PASS\n";
}

void test_persist()
{
    std::cout << "Database: PERSIST removes expiry... ";
    Database db;

    db.set("key", "val", 500);
    assert(db.pttl("key") > 0);

    // Remove expiry
    assert(db.persist("key") == true);
    assert(db.pttl("key") == -1);

    // Persist on no-expiry key returns false
    assert(db.persist("key") == false);

    // Persist on missing key
    assert(db.persist("missing") == false);

    // Key should survive well past original TTL
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    assert(db.get("key") != nullptr);

    std::cout << "PASS\n";
}

void test_overwrite_clears_ttl()
{
    std::cout << "Database: SET overwrite clears TTL... ";
    Database db;

    db.set("key", "v1", 100); // 100ms TTL
    assert(db.pttl("key") > 0);

    db.set("key", "v2");          // overwrite without TTL
    assert(db.pttl("key") == -1); // expiry removed

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    assert(db.get("key") != nullptr); // still alive

    std::cout << "PASS\n";
}

void test_exists()
{
    std::cout << "Database: EXISTS... ";
    Database db;

    db.set("a", "1");
    db.set("b", "2");

    assert(db.exists("a") == true);
    assert(db.exists("b") == true);
    assert(db.exists("c") == false);

    db.del("a");
    assert(db.exists("a") == false);

    // Expired key should not exist
    db.set("temp", "val", 50);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(db.exists("temp") == false);

    std::cout << "PASS\n";
}

void test_keys()
{
    std::cout << "Database: KEYS with patterns... ";
    Database db;

    db.set("user:1", "alice");
    db.set("user:2", "bob");
    db.set("user:3", "charlie");
    db.set("session:abc", "data");
    db.set("session:def", "data");

    // Match all
    auto all = db.keys("*");
    assert(all.size() == 5);

    // Prefix match
    auto users = db.keys("user:*");
    assert(users.size() == 3);

    // Suffix match
    auto abc = db.keys("*abc");
    assert(abc.size() == 1);
    assert(abc[0] == "session:abc");

    // Exact match
    auto exact = db.keys("user:2");
    assert(exact.size() == 1);

    // No match
    auto none = db.keys("missing*");
    assert(none.size() == 0);

    std::cout << "PASS\n";
}

void test_rename()
{
    std::cout << "Database: RENAME... ";
    Database db;

    db.set("old", "value");
    assert(db.rename("old", "new") == true);
    assert(db.exists("old") == false);
    assert(db.exists("new") == true);
    assert(*db.get("new") == "value");

    // Rename to existing key (overwrites)
    db.set("x", "10");
    db.set("y", "20");
    assert(db.rename("x", "y") == true);
    assert(*db.get("y") == "10");
    assert(db.exists("x") == false);

    // Rename missing key
    assert(db.rename("does_not_exist", "z") == false);

    std::cout << "PASS\n";
}

void test_zsets()
{
    std::cout << "Database: ZSets... ";
    Database db;

    // ZADD
    assert(db.zadd("zkey", 10.0, "m1") == true);
    assert(db.zadd("zkey", 20.0, "m2") == true);
    assert(db.zadd("zkey", 15.0, "m1") == false); // updated existing
    assert(db.zcard("zkey") == 2);

    // ZSCORE
    auto score = db.zscore("zkey", "m1");
    assert(score.has_value());
    assert(*score == 15.0);
    assert(!db.zscore("zkey", "m3").has_value());

    // ZRANGE
    auto range = db.zrange("zkey", 0, -1);
    assert(range.size() == 2);
    assert(range[0].member == "m1");
    assert(range[0].score == 15.0);
    assert(range[1].member == "m2");
    assert(range[1].score == 20.0);

    // ZRANGEBYSCORE
    auto rangeByScore = db.zrangebyscore("zkey", 18.0, 25.0);
    assert(rangeByScore.size() == 1);
    assert(rangeByScore[0].member == "m2");

    // ZREM
    assert(db.zrem("zkey", "m1") == true);
    assert(db.zcard("zkey") == 1);
    assert(db.zrem("zkey", "m1") == false);

    // TYPE
    assert(db.getType("zkey") == EntryType::ZSET);
    db.set("skey", "val");
    assert(db.getType("skey") == EntryType::STRING);

    // Overwrite ZSET with STRING (SET command)
    db.set("zkey", "converted");
    assert(db.getType("zkey") == EntryType::STRING);
    assert(*db.get("zkey") == "converted");

    // Empty ZSET should be deleted
    db.zadd("zempty", 1.0, "m1");
    assert(db.exists("zempty"));
    db.zrem("zempty", "m1");
    assert(!db.exists("zempty"));

    std::cout << "PASS\n";
}

void test_hashes()
{
    std::cout << "Database: Hashes... ";
    Database db;

    // HSET
    assert(db.hset("hkey", "f1", "v1") == 1);     // New
    assert(db.hset("hkey", "f2", "v2") == 1);     // New
    assert(db.hset("hkey", "f1", "v1_new") == 0); // Update
    assert(db.hlen("hkey") == 2);

    // HGET
    auto val = db.hget("hkey", "f1");
    assert(val.has_value());
    assert(*val == "v1_new");
    assert(!db.hget("hkey", "f3").has_value());

    // HDEL
    assert(db.hdel("hkey", "f1") == 1);
    assert(db.hlen("hkey") == 1);
    assert(db.hdel("hkey", "f1") == 0);

    // HGETALL
    auto all = db.hgetall("hkey");
    assert(all.size() == 1);
    assert(all[0].field == "f2");
    assert(all[0].value == "v2");

    // TYPE
    assert(db.getType("hkey") == EntryType::HASH);

    // Empty hash cleanup
    db.hdel("hkey", "f2");
    assert(!db.exists("hkey"));

    std::cout << "PASS\n";
}

int main()
{
    std::cout << "=== Database Tests ===\n";
    test_set_get_del();
    test_many_keys();
    test_ttl_set_with_expiry();
    test_expire_command();
    test_persist();
    test_overwrite_clears_ttl();
    test_exists();
    test_keys();
    test_rename();
    test_zsets();
    test_hashes();
    std::cout << "All Database tests passed!\n";
    return 0;
}
