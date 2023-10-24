#ifndef TREENODE_H
#define TREENODE_H

struct TreeNode {
    char* name;
    struct TreeNode** child;
    int num_child;
    int type;
    union{
        char* string_val;
        int int_val;
        float float_val;
    };
};

struct TreeNode* createNode(const char* name, int num_child, ...);
void addChild(struct TreeNode* parent, struct TreeNode* child);
void printParseTree(struct TreeNode* node, int level);

#endif