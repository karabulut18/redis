#include "../lib/redis/AVLTree.h"
#include <cassert>
#include <iostream>
#include <string>

struct TestNode
{
    AVLNode avl;
    int key;

    explicit TestNode(int k) : key(k)
    {
        avl.init();
    }

    static TestNode* fromAVL(AVLNode* node)
    {
        if (!node)
            return nullptr;
        return reinterpret_cast<TestNode*>(reinterpret_cast<char*>(node) - offsetof(TestNode, avl));
    }
};

static bool testLess(AVLNode* a, AVLNode* b)
{
    return TestNode::fromAVL(a)->key < TestNode::fromAVL(b)->key;
}

void test_insert_and_traversal()
{
    std::cout << "AVLTree: insert and in-order traversal... ";
    AVLTree tree;

    int keys[] = {5, 3, 7, 1, 4, 6, 8, 2};
    TestNode* nodes[8];

    for (int i = 0; i < 8; i++)
    {
        nodes[i] = new TestNode(keys[i]);
        tree.insert(&nodes[i]->avl, testLess);
    }

    // Find minimum (should be 1)
    AVLNode* min = tree.root();
    while (min->left)
        min = min->left;
    assert(TestNode::fromAVL(min)->key == 1);

    // In-order traversal via successor
    int expected[] = {1, 2, 3, 4, 5, 6, 7, 8};
    AVLNode* cur = min;
    for (int i = 0; i < 8; i++)
    {
        assert(cur != nullptr);
        assert(TestNode::fromAVL(cur)->key == expected[i]);
        cur = AVLNode::successor(cur);
    }
    assert(cur == nullptr); // past the end

    for (int i = 0; i < 8; i++)
        delete nodes[i];
    std::cout << "PASS\n";
}

void test_delete()
{
    std::cout << "AVLTree: delete nodes... ";
    AVLTree tree;

    TestNode* nodes[5];
    for (int i = 0; i < 5; i++)
    {
        nodes[i] = new TestNode(i * 10); // 0, 10, 20, 30, 40
        tree.insert(&nodes[i]->avl, testLess);
    }

    // Delete middle node (20)
    tree.setRoot(AVLNode::deleteNode(&nodes[2]->avl));
    delete nodes[2];

    // Verify remaining: 0, 10, 30, 40
    AVLNode* min = tree.root();
    while (min->left)
        min = min->left;

    int expected[] = {0, 10, 30, 40};
    AVLNode* cur = min;
    for (int i = 0; i < 4; i++)
    {
        assert(cur != nullptr);
        assert(TestNode::fromAVL(cur)->key == expected[i]);
        cur = AVLNode::successor(cur);
    }

    delete nodes[0];
    delete nodes[1];
    delete nodes[3];
    delete nodes[4];
    std::cout << "PASS\n";
}

void test_balance()
{
    std::cout << "AVLTree: balance under sequential insert... ";
    AVLTree tree;

    // Insert 1..100 in order — worst case for BST but AVL should balance
    const int N = 100;
    TestNode* nodes[N];
    for (int i = 0; i < N; i++)
    {
        nodes[i] = new TestNode(i);
        tree.insert(&nodes[i]->avl, testLess);
    }

    // Height should be O(log n), specifically <= 1.44 * log2(100) ≈ 9.6
    assert(tree.root() != nullptr);
    assert(AVLNode::getHeight(tree.root()) <= 10);

    // Verify offset works
    AVLNode* min = tree.root();
    while (min->left)
        min = min->left;
    assert(TestNode::fromAVL(min)->key == 0);

    AVLNode* tenth = AVLNode::offset(min, 10);
    assert(TestNode::fromAVL(tenth)->key == 10);

    for (int i = 0; i < N; i++)
        delete nodes[i];
    std::cout << "PASS\n";
}

int main()
{
    std::cout << "=== AVLTree Tests ===\n";
    test_insert_and_traversal();
    test_delete();
    test_balance();
    std::cout << "All AVLTree tests passed!\n";
    return 0;
}
