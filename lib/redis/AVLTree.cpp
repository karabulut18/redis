#include "AVLTree.h"
#include <algorithm>
#include <cassert>

void AVLNode::init()
{
    parent = left = right = nullptr;
    height = 1;
    cnt = 1;
}

uint32_t AVLNode::getSize(const AVLNode* node)
{
    return node ? node->cnt : 0;
}

uint32_t AVLNode::getHeight(const AVLNode* node)
{
    return node ? node->height : 0;
}

void AVLNode::updateStats(AVLNode* node)
{
    node->height = 1 + std::max(getHeight(node->left), getHeight(node->right));
    node->cnt = 1 + getSize(node->left) + getSize(node->right);
}

AVLNode* AVLNode::rotateLeft(AVLNode* node)
{
    AVLNode* par = node->parent;
    AVLNode* newRoot = node->right;
    AVLNode* inner = newRoot->left;

    node->right = inner;
    if (inner)
        inner->parent = node;

    newRoot->parent = par;
    newRoot->left = node;
    node->parent = newRoot;

    updateStats(node);
    updateStats(newRoot);
    return newRoot;
}

AVLNode* AVLNode::rotateRight(AVLNode* node)
{
    AVLNode* par = node->parent;
    AVLNode* newRoot = node->left;
    AVLNode* inner = newRoot->right;

    node->left = inner;
    if (inner)
        inner->parent = node;

    newRoot->parent = par;
    newRoot->right = node;
    node->parent = newRoot;

    updateStats(node);
    updateStats(newRoot);
    return newRoot;
}

AVLNode* AVLNode::leftFix(AVLNode* node)
{
    if (getHeight(node->left->right) > getHeight(node->left->left))
        node->left = rotateLeft(node->left);
    return rotateRight(node);
}

AVLNode* AVLNode::rightFix(AVLNode* node)
{
    if (getHeight(node->right->left) > getHeight(node->right->right))
        node->right = rotateRight(node->right);
    return rotateLeft(node);
}

AVLNode* AVLNode::balance(AVLNode* node)
{
    while (true)
    {
        AVLNode** from = &node;
        AVLNode* par = node->parent;
        if (par)
            from = (par->left == node) ? &par->left : &par->right;

        updateStats(node);

        uint32_t lh = getHeight(node->left);
        uint32_t rh = getHeight(node->right);
        if (lh > rh + 1)
            *from = node = leftFix(node);
        else if (rh > lh + 1)
            *from = node = rightFix(node);

        if (!par)
            return node;

        node = par;
    }
}

AVLNode* AVLNode::deleteNodeEasy(AVLNode* node)
{
    assert(!node->left || !node->right);
    AVLNode* child = node->left ? node->left : node->right;
    AVLNode* par = node->parent;

    if (child)
        child->parent = par;

    if (!par)
        return child; // removing root

    AVLNode** from = (par->left == node) ? &par->left : &par->right;
    *from = child;
    return balance(par);
}

AVLNode* AVLNode::deleteNode(AVLNode* node)
{
    if (!node->left || !node->right)
        return deleteNodeEasy(node);

    // Find in-order successor
    AVLNode* victim = node->right;
    while (victim->left)
        victim = victim->left;

    AVLNode* root = deleteNodeEasy(victim);

    // Swap victim into node's position
    *victim = *node;
    if (victim->left)
        victim->left->parent = victim;
    if (victim->right)
        victim->right->parent = victim;

    AVLNode** from = &root;
    AVLNode* par = node->parent;
    if (par)
        from = (par->left == node) ? &par->left : &par->right;
    *from = victim;

    return root;
}

AVLNode* AVLNode::findMin(AVLNode* node)
{
    if (!node)
        return nullptr;
    while (node->left)
        node = node->left;
    return node;
}

AVLNode* AVLNode::successor(AVLNode* node)
{
    if (node->right)
    {
        for (node = node->right; node->left; node = node->left)
        {
        }
        return node;
    }

    while (AVLNode* par = node->parent)
    {
        if (node == par->left)
            return par;
        node = par;
    }
    return nullptr;
}

AVLNode* AVLNode::predecessor(AVLNode* node)
{
    if (node->left)
    {
        for (node = node->left; node->right; node = node->right)
        {
        }
        return node;
    }

    while (AVLNode* par = node->parent)
    {
        if (node == par->right)
            return par;
        node = par;
    }
    return nullptr;
}

AVLNode* AVLNode::offset(AVLNode* node, int64_t off)
{
    for (; off > 0 && node; off--)
        node = successor(node);

    for (; off < 0 && node; off++)
        node = predecessor(node);

    return node;
}

// --- AVLTree ---

void AVLTree::insert(AVLNode* node, const AVLLess& less)
{
    AVLNode* parent = nullptr;
    AVLNode** from = &_root;

    for (AVLNode* cur = _root; cur;)
    {
        from = less(node, cur) ? &cur->left : &cur->right;
        parent = cur;
        cur = *from;
    }
    *from = node;
    node->parent = parent;

    // Balance and maintain size from the inserted node to the root
    _root = AVLNode::balance(node);
}

AVLNode* AVLTree::remove(const AVLCmp& cmp, void* key)
{
    for (AVLNode* node = _root; node;)
    {
        int32_t r = cmp(node, key);
        if (r < 0)
            node = node->left;
        else if (r > 0)
            node = node->right;
        else
        {
            _root = AVLNode::deleteNode(node);
            return node;
        }
    }
    return nullptr;
}

uint32_t AVLNode::getRank(AVLNode* node)
{
    if (!node)
        return 0;

    uint32_t rank = getSize(node->left);
    AVLNode* cur = node;
    while (cur->parent)
    {
        if (cur == cur->parent->right)
        {
            rank += getSize(cur->parent->left) + 1;
        }
        cur = cur->parent;
    }
    return rank;
}
