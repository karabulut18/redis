#include "../lib/common/str_hash.h"
#include "../lib/redis/HashMap.h"
#include <cassert>
#include <iostream>
#include <string>

struct TestEntry
{
    HNode hashNode;
    std::string key;
    int value;

    TestEntry(const std::string& k, int v) : key(k), value(v)
    {
        hashNode.next = nullptr;
        hashNode.code = str_hash(key);
    }

    static TestEntry* fromHash(HNode* node)
    {
        return reinterpret_cast<TestEntry*>(reinterpret_cast<char*>(node) - offsetof(TestEntry, hashNode));
    }
};

struct TestLookup
{
    HNode hashNode;
    std::string key;

    explicit TestLookup(const std::string& k) : key(k)
    {
        hashNode.next = nullptr;
        hashNode.code = str_hash(k);
    }

    static bool cmp(HNode* entryNode, HNode* keyNode)
    {
        TestEntry* entry = TestEntry::fromHash(entryNode);
        TestLookup* lookup =
            reinterpret_cast<TestLookup*>(reinterpret_cast<char*>(keyNode) - offsetof(TestLookup, hashNode));
        return entry->key == lookup->key;
    }
};

void test_insert_and_lookup()
{
    std::cout << "HashMap: insert and lookup... ";
    HashMap map;

    TestEntry* a = new TestEntry("hello", 1);
    TestEntry* b = new TestEntry("world", 2);
    TestEntry* c = new TestEntry("foo", 3);

    map.insert(&a->hashNode);
    map.insert(&b->hashNode);
    map.insert(&c->hashNode);

    TestLookup lk("world");
    HNode* found = map.lookup(&lk.hashNode, TestLookup::cmp);
    assert(found != nullptr);
    assert(TestEntry::fromHash(found)->value == 2);

    TestLookup lk2("missing");
    assert(map.lookup(&lk2.hashNode, TestLookup::cmp) == nullptr);

    delete a;
    delete b;
    delete c;
    map.clear();
    std::cout << "PASS\n";
}

void test_remove()
{
    std::cout << "HashMap: remove... ";
    HashMap map;

    TestEntry* a = new TestEntry("alpha", 10);
    TestEntry* b = new TestEntry("beta", 20);
    map.insert(&a->hashNode);
    map.insert(&b->hashNode);

    TestLookup lk("alpha");
    HNode* removed = map.remove(&lk.hashNode, TestLookup::cmp);
    assert(removed != nullptr);
    assert(TestEntry::fromHash(removed)->value == 10);
    delete TestEntry::fromHash(removed);

    // Verify it's gone
    TestLookup lk2("alpha");
    assert(map.lookup(&lk2.hashNode, TestLookup::cmp) == nullptr);

    // beta is still there
    TestLookup lk3("beta");
    assert(map.lookup(&lk3.hashNode, TestLookup::cmp) != nullptr);

    delete b;
    map.clear();
    std::cout << "PASS\n";
}

void test_rehashing()
{
    std::cout << "HashMap: rehashing under load... ";
    HashMap map;

    const int N = 1000;
    TestEntry* entries[N];

    for (int i = 0; i < N; i++)
    {
        entries[i] = new TestEntry("key" + std::to_string(i), i);
        map.insert(&entries[i]->hashNode);
    }

    // Verify all entries are findable
    for (int i = 0; i < N; i++)
    {
        TestLookup lk("key" + std::to_string(i));
        HNode* found = map.lookup(&lk.hashNode, TestLookup::cmp);
        assert(found != nullptr);
        assert(TestEntry::fromHash(found)->value == i);
    }

    // Remove half
    for (int i = 0; i < N; i += 2)
    {
        TestLookup lk("key" + std::to_string(i));
        HNode* removed = map.remove(&lk.hashNode, TestLookup::cmp);
        assert(removed != nullptr);
        delete TestEntry::fromHash(removed);
    }

    // Verify remaining
    for (int i = 0; i < N; i++)
    {
        TestLookup lk("key" + std::to_string(i));
        HNode* found = map.lookup(&lk.hashNode, TestLookup::cmp);
        if (i % 2 == 0)
            assert(found == nullptr);
        else
            assert(found != nullptr);
    }

    // Cleanup remaining odd entries
    for (int i = 1; i < N; i += 2)
    {
        TestLookup lk("key" + std::to_string(i));
        HNode* removed = map.remove(&lk.hashNode, TestLookup::cmp);
        assert(removed != nullptr);
        delete TestEntry::fromHash(removed);
    }

    map.clear();
    std::cout << "PASS\n";
}

int main()
{
    std::cout << "=== HashMap Tests ===\n";
    test_insert_and_lookup();
    test_remove();
    test_rehashing();
    std::cout << "All HashMap tests passed!\n";
    return 0;
}
