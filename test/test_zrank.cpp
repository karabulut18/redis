#include "../lib/redis/ZSet.h"
#include <cassert>
#include <iostream>
#include <string>

void test_zrank_basic()
{
    std::cout << "ZSet: ZRANK basic functionality... ";
    ZSet zset;
    zset.insert("a", 10.0);
    zset.insert("b", 20.0);
    zset.insert("c", 30.0);

    assert(zset.getRank("a") == 0);
    assert(zset.getRank("b") == 1);
    assert(zset.getRank("c") == 2);
    assert(zset.getRank("nonexistent") == -1);
    std::cout << "PASS\n";
}

void test_zrank_rebalancing()
{
    std::cout << "ZSet: ZRANK with rebalancing (sequential inserts)... ";
    ZSet zset;
    // Sequential inserts will trigger many rotations/rebalances in AVL
    for (int i = 0; i < 100; ++i)
    {
        zset.insert("member" + std::to_string(i), static_cast<double>(i));
    }

    assert(zset.size() == 100);

    for (int i = 0; i < 100; ++i)
    {
        std::string name = "member" + std::to_string(i);
        int64_t rank = zset.getRank(name);
        if (rank != i)
        {
            std::cerr << "Fail at " << i << " got " << rank << std::endl;
            assert(false);
        }
    }
    std::cout << "PASS\n";
}

void test_zrank_updates()
{
    std::cout << "ZSet: ZRANK after score updates... ";
    ZSet zset;
    zset.insert("a", 10.0); // Rank 0
    zset.insert("b", 20.0); // Rank 1
    zset.insert("c", 30.0); // Rank 2

    // Move 'a' to the end
    zset.insert("a", 40.0);
    assert(zset.getRank("b") == 0);
    assert(zset.getRank("c") == 1);
    assert(zset.getRank("a") == 2);

    // Remove 'c'
    ZNode* nodeC = zset.lookUp("c");
    zset.remove(nodeC);
    assert(zset.getRank("b") == 0);
    assert(zset.getRank("a") == 1);
    assert(zset.getRank("c") == -1);

    std::cout << "PASS\n";
}

int main()
{
    std::cout << "=== ZRANK Unit Tests ===\n";
    test_zrank_basic();
    test_zrank_rebalancing();
    test_zrank_updates();
    std::cout << "All ZRANK unit tests passed!\n";
    return 0;
}
