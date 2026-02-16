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

int main()
{
    std::cout << "=== Database Tests ===\n";
    test_set_get_del();
    test_many_keys();
    test_ttl_set_with_expiry();
    test_expire_command();
    test_persist();
    test_overwrite_clears_ttl();
    std::cout << "All Database tests passed!\n";
    return 0;
}
