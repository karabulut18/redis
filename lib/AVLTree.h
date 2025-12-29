#pragma once
#include <stdint.h>

struct AVLNode
{
    AVLNode* parent;
    AVLNode* left;
    AVLNode* right;
    uint32_t height;

    AVLNode();
    static uint32_t getHeight(const AVLNode* node);
    static void updateHeight(AVLNode* node);
    static AVLNode* rotateLeft(AVLNode* node);
    static AVLNode* rotateRight(AVLNode* node);

    static AVLNode* leftFix(AVLNode* node);
    static AVLNode* rightFix(AVLNode* node);
    static AVLNode* balance(AVLNode* node);
    static AVLNode* findMin(AVLNode* node);
    static AVLNode* deleteNode(AVLNode* node);
    static AVLNode* deleteNodeEasy(AVLNode* node);

    static void searchAndInsert(AVLNode** root, AVLNode* node, bool (*less)(AVLNode*, AVLNode*));
    static AVLNode* searchAndDelete(AVLNode** root, int32_t (*cmp)(AVLNode*, void*), void* key);
};
