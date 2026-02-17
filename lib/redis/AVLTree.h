#pragma once

#include <cstdint>
#include <functional>

struct AVLNode
{
    AVLNode* parent = nullptr;
    AVLNode* left = nullptr;
    AVLNode* right = nullptr;
    uint32_t height = 0;
    uint32_t cnt = 0; // Subtree size

    void init();

    static uint32_t getHeight(const AVLNode* node);
    static uint32_t getSize(const AVLNode* node);
    static void updateStats(AVLNode* node);

    static AVLNode* rotateLeft(AVLNode* node);
    static AVLNode* rotateRight(AVLNode* node);

    static AVLNode* leftFix(AVLNode* node);
    static AVLNode* rightFix(AVLNode* node);
    static AVLNode* balance(AVLNode* node);

    static AVLNode* findMin(AVLNode* node);
    static AVLNode* deleteNode(AVLNode* node);
    static AVLNode* deleteNodeEasy(AVLNode* node);

    static AVLNode* successor(AVLNode* node);
    static AVLNode* predecessor(AVLNode* node);
    static AVLNode* offset(AVLNode* node, int64_t offset);
    static uint32_t getRank(AVLNode* node);
};

// Comparator types
using AVLLess = std::function<bool(AVLNode*, AVLNode*)>;
using AVLCmp = std::function<int32_t(AVLNode*, void*)>;

class AVLTree
{
public:
    AVLTree() = default;

    void insert(AVLNode* node, const AVLLess& less);
    AVLNode* remove(const AVLCmp& cmp, void* key);

    AVLNode* root() const
    {
        return _root;
    }
    void setRoot(AVLNode* node)
    {
        _root = node;
    }

private:
    AVLNode* _root = nullptr;
};