#include "AVLTree.h"
#include <_types/_uint64_t.h>
#include <cassert>
#include <sys/_types/_int32_t.h>

void AVLNode::initNode(AVLNode* node)
{
    node->parent = node->left = node->right = nullptr;
    node->height = 1;
}

uint32_t AVLNode::getHeight(const AVLNode* node)
{
    return node ? node->height : 0;
}

void AVLNode::updateHeight(AVLNode* node)
{
    uint32_t leftHeight = getHeight(node->left);
    uint32_t rightHeight = getHeight(node->right);
    node->height = 1 + (leftHeight > rightHeight ? leftHeight : rightHeight);
}

AVLNode* AVLNode::rotateLeft(AVLNode* node)
{
    AVLNode* parent = node->parent;
    AVLNode* newNode = node->right;
    AVLNode* inner = newNode->left;

    node->right = inner;
    if (inner)
        inner->parent = node;

    newNode->parent = parent;

    newNode->left = node;
    node->parent = newNode;
    updateHeight(node);
    updateHeight(newNode);
    return newNode;
}

AVLNode* AVLNode::rotateRight(AVLNode* node)
{
    AVLNode* parent = node->parent;
    AVLNode* newNode = node->left;
    AVLNode* inner = newNode->right;

    node->left = inner;
    if (inner)
        inner->parent = node;

    newNode->parent = parent;

    newNode->right = node;
    node->parent = newNode;
    updateHeight(node);
    updateHeight(newNode);
    return newNode;
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

// Called on an updated node:
// - Propagate auxiliary data.
// - Fix imbalances.
// - Return the new root node.
AVLNode* AVLNode::balance(AVLNode* node)
{
    while (true)
    {
        AVLNode** from = &node;
        AVLNode* parent = node->parent;
        if (parent)
            from = parent->left == node ? &parent->left : &parent->right;

        updateHeight(node);

        uint32_t leftHeight = getHeight(node->left);
        uint32_t rightHeight = getHeight(node->right);
        if (leftHeight > rightHeight + 1)
            node = leftFix(node);
        else if (rightHeight > leftHeight + 1)
            node = rightFix(node);
        else
            break;

        if (!parent)
            return *from;

        node = parent;
    }
    return node;
};

// detach a node where 1 of its children is empty
AVLNode* AVLNode::deleteNodeEasy(AVLNode* node)
{
    assert(!node->left || !node->right);                    // at most 1 child
    AVLNode* child = node->left ? node->left : node->right; // can be NULL
    AVLNode* parent = node->parent;
    // update the child's parent pointer
    if (child)
        child->parent = parent; // can be NULL
    // attach the child to the grandparent
    if (!parent)
        return child; // removing the root node
    AVLNode** from = parent->left == node ? &parent->left : &parent->right;
    *from = child;
    return balance(parent);
}

// detach a node and returns the new root of the tree
AVLNode* AVLNode::deleteNode(AVLNode* node)
{
    // the easy case of 0 or 1 child
    if (!node->left || !node->right)
        return deleteNodeEasy(node);
    // find the successor
    AVLNode* victim = node->right;
    while (victim->left)
        victim = victim->left;
    // detach the successor
    AVLNode* root = deleteNodeEasy(victim);
    // swap with the successor
    *victim = *node; // left, right, parent
    if (victim->left)
        victim->left->parent = victim;
    if (victim->right)
        victim->right->parent = victim;
    // attach the successor to the parent, or update the root pointer
    AVLNode** from = &root;
    AVLNode* parent = node->parent;
    if (parent)
        from = parent->left == node ? &parent->left : &parent->right;
    *from = victim;
    return root;
}

void AVLTree::searchAndInsert(AVLNode** root, AVLNode* newNode, bool (*less)(AVLNode*, AVLNode*))
{
    AVLNode* parent = nullptr;
    AVLNode** from = root;

    for (AVLNode* node = *root; node;)
    {
        from = less(newNode, node) ? &node->left : &node->right;
        parent = node;
        node = *from;
    }
    *from = newNode;
    newNode->parent = parent;

    *root = AVLNode::balance(*root);
}

AVLNode* AVLTree::searchAndDelete(AVLNode** root, int32_t (*cmp)(AVLNode*, void*), void* key)
{
    for (AVLNode* node = *root; node;)
    {
        int32_t r = cmp(node, key);
        if (r < 0)
            node = node->left;
        else if (r > 0)
            node = node->right;
        else
        {
            *root = AVLNode::deleteNode(node);
            return node;
        }
    }
    return nullptr;
}

AVLNode* AVLNode::successor(AVLNode* node)
{
    // left most node in the right sub three
    if (node->right)
    {
        for (node = node->right; node->left; node = node->left)
        {
        };
        return node;
    }

    // the parent where this node is right most node in the left sub three
    while (AVLNode* parent = node->parent)
    {
        if (node == parent->left)
            return parent;

        node = parent;
    }
    return nullptr;
}

AVLNode* AVLNode::predecessor(AVLNode* node)
{
    // right most node in the left sub three
    if (node->left)
    {
        for (node = node->left; node->right; node = node->right)
        {
        };
        return node;
    }

    // the parent where this node is left most node in the right sub three
    while (AVLNode* parent = node->parent)
    {
        if (node == parent->right)
            return parent;

        node = parent;
    }
    return nullptr;
}

AVLNode* AVLNode::offset(AVLNode* node, int64_t offset)
{
    for (; offset > 0; offset--)
        node = successor(node);

    for (; offset < 0; offset++)
        node = predecessor(node);

    return node;
}
