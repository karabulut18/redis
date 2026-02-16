#include "../lib/redis/ZSet.h"
#include <cassert>
#include <iostream>
#include <vector>

void test_insert_and_lookup()
{
    std::cout << "ZSet: insert and lookup... ";
    ZSet zset;

    // New insert
    bool isNew = zset.insert("alice", 10.0);
    assert(isNew);
    assert(zset.size() == 1);

    // Update existing
    isNew = zset.insert("alice", 20.0);
    assert(!isNew);
    assert(zset.size() == 1);

    // Lookup
    ZNode* node = zset.lookUp("alice");
    assert(node != nullptr);
    assert(node->score == 20.0);
    assert(node->name == "alice");

    // Lookup missing
    assert(zset.lookUp("bob") == nullptr);

    std::cout << "PASS\n";
}

void test_sorting_order()
{
    std::cout << "ZSet: sorting order... ";
    ZSet zset;

    zset.insert("b", 20.0);
    zset.insert("a", 10.0);
    zset.insert("c", 30.0);
    zset.insert("d", 20.0); // Same score as b, should be sorted by name: a, b, d, c

    // Iterate via tree
    AVLNode* root = zset.tree().root();
    assert(root != nullptr);

    // Find min
    AVLNode* min = root;
    while (min->left)
        min = min->left;

    std::vector<std::string> names;
    std::vector<double> scores;

    AVLNode* cur = min;
    while (cur)
    {
        ZNode* znode = ZNode::fromTree(cur);
        names.push_back(znode->name);
        scores.push_back(znode->score);
        cur = AVLNode::successor(cur);
    }

    assert(names.size() == 4);
    assert(names[0] == "a" && scores[0] == 10.0);
    assert(names[1] == "b" && scores[1] == 20.0);
    assert(names[2] == "d" && scores[2] == 20.0); // Lexicographical check for ties
    assert(names[3] == "c" && scores[3] == 30.0);

    std::cout << "PASS\n";
}

void test_update_reposition()
{
    std::cout << "ZSet: update repositioning... ";
    ZSet zset;

    zset.insert("a", 10.0);
    zset.insert("b", 20.0);
    zset.insert("c", 30.0);

    // Update 'a' to be largest
    zset.insert("a", 40.0);

    // Check order: b(20), c(30), a(40)
    AVLNode* min = zset.tree().root();
    while (min->left)
        min = min->left;
    ZNode* first = ZNode::fromTree(min);
    assert(first->name == "b");

    // Check lookup
    ZNode* node = zset.lookUp("a");
    assert(node->score == 40.0);

    std::cout << "PASS\n";
}

void test_remove()
{
    std::cout << "ZSet: remove... ";
    ZSet zset;

    zset.insert("a", 10.0);
    zset.insert("b", 20.0);

    ZNode* node = zset.lookUp("a");
    assert(node != nullptr);

    zset.remove(node);
    assert(zset.size() == 1);
    assert(zset.lookUp("a") == nullptr);
    assert(zset.lookUp("b") != nullptr);

    std::cout << "PASS\n";
}

void test_seek_ge()
{
    std::cout << "ZSet: seek greater or equal... ";
    ZSet zset;

    zset.insert("a", 10.0);
    zset.insert("b", 20.0);
    zset.insert("c", 30.0);

    // Exact match
    ZNode* n = zset.seekGe(20.0, "b");
    assert(n != nullptr && n->name == "b");

    // Value in between
    n = zset.seekGe(15.0, "");
    assert(n != nullptr && n->name == "b"); // First >= 15 is 20(b)

    // Value larger than max
    n = zset.seekGe(40.0, "");
    assert(n == nullptr);

    // Value smaller than min
    n = zset.seekGe(5.0, "");
    assert(n != nullptr && n->name == "a");

    std::cout << "PASS\n";
}

int main()
{
    std::cout << "=== ZSet Tests ===\n";
    test_insert_and_lookup();
    test_sorting_order();
    test_update_reposition();
    test_remove();
    test_seek_ge();
    std::cout << "All ZSet tests passed!\n";
    return 0;
}
