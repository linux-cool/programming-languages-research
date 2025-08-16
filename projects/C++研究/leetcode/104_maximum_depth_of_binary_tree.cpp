#include <iostream>
#include <algorithm>

/**
 * Definition for a binary tree node.
 */
struct TreeNode {
    int val;
    TreeNode *left;
    TreeNode *right;
    TreeNode() : val(0), left(nullptr), right(nullptr) {}
    TreeNode(int x) : val(x), left(nullptr), right(nullptr) {}
    TreeNode(int x, TreeNode *left, TreeNode *right) : val(x), left(left), right(right) {}
};

/**
 * @brief LeetCode 104: Maximum Depth of Binary Tree (二叉树的最大深度)
 *
 * Problem Statement:
 * Given the root of a binary tree, return its maximum depth.
 * A binary tree's maximum depth is the number of nodes along the longest path from the root node down to the farthest leaf node.
 *
 * Algorithm Explanation:
 * This is a classic problem that can be solved elegantly with recursion, leveraging the tree's inherent recursive structure.
 *
 * The depth of a tree is defined as:
 * - 0 if the tree is empty (i.e., the root is null).
 * - 1 + the maximum of the depths of its left and right subtrees, otherwise.
 *
 * The recursive approach works as follows:
 * 1. Base Case: If the current node (`root`) is `nullptr`, it means we've reached the end of a path. The depth of an empty tree is 0, so we return 0.
 * 2. Recursive Step: If the node is not null, we recursively call the function on its left child to find the maximum depth of the left subtree (`left_depth`).
 * 3. Similarly, we call the function on its right child to find the maximum depth of the right subtree (`right_depth`).
 * 4. The depth of the tree rooted at the current node is `1` (for the current node itself) plus the maximum of the depths of its two subtrees. So, we return `1 + std::max(left_depth, right_depth)`.
 *
 * This process continues until all paths have been explored down to the leaves, and the depths bubble up to the initial call.
 *
 * Complexity Analysis:
 * - Time Complexity: O(N), where N is the number of nodes in the tree. We must visit every node exactly once to determine the depth.
 * - Space Complexity: O(H), where H is the height of the tree. This is the space used by the recursion call stack. In the worst case (a completely unbalanced tree, like a linked list), the height is N, so the space complexity is O(N). In the best case (a perfectly balanced tree), the height is log(N), so the space complexity is O(log N).
 */
class Solution {
public:
    int maxDepth(TreeNode* root) {
        // Base case: if the node is null, the depth is 0.
        if (root == nullptr) {
            return 0;
        }
        
        // Recursively find the depth of the left and right subtrees.
        int left_depth = maxDepth(root->left);
        int right_depth = maxDepth(root->right);
        
        // The depth of the tree is 1 (for the current node) + the max of the subtree depths.
        return 1 + std::max(left_depth, right_depth);
    }
};

// Test function
int main() {
    Solution sol;

    std::cout << "--- LeetCode 104: Maximum Depth of Binary Tree ---" << std::endl;

    // Construct a sample binary tree:
    //     3
    //    / \
    //   9  20
    //     /  \
    //    15   7
    TreeNode* root = new TreeNode(3);
    root->left = new TreeNode(9);
    root->right = new TreeNode(20);
    root->right->left = new TreeNode(15);
    root->right->right = new TreeNode(7);

    int depth = sol.maxDepth(root);

    std::cout << "The maximum depth of the tree is: " << depth << std::endl; // Expected: 3

    // Note: Memory for the tree nodes should be freed in a real application.
    // This is omitted for simplicity.

    return 0;
}
