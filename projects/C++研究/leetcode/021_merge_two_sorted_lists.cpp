#include <iostream>

/**
 * Definition for singly-linked list.
 */
struct ListNode {
    int val;
    ListNode *next;
    ListNode() : val(0), next(nullptr) {}
    ListNode(int x) : val(x), next(nullptr) {}
    ListNode(int x, ListNode *next) : val(x), next(next) {}
};

/**
 * @brief LeetCode 21: Merge Two Sorted Lists (合并两个有序链表)
 *
 * Problem Statement:
 * Merge two sorted linked lists and return it as a new sorted list. The new list should be made by splicing together the nodes of the first two lists.
 *
 * Algorithm Explanation:
 * We can solve this iteratively. The core idea is to maintain a pointer to the current node of the merged list and decide which node to append next from either `l1` or `l2`.
 *
 * 1. Create a "dummy" or "sentinel" node. This node acts as a placeholder for the head of our merged list, simplifying the code by avoiding edge cases for an empty list.
 * 2. Create a `current` pointer, initially pointing to the dummy node. This pointer will be used to build the new list.
 * 3. While both `l1` and `l2` are not null, compare their values:
 *    - If `l1->val` is smaller than or equal to `l2->val`, append the `l1` node to our merged list (`current->next = l1`) and advance the `l1` pointer (`l1 = l1->next`).
 *    - Otherwise, append the `l2` node (`current->next = l2`) and advance the `l2` pointer (`l2 = l2->next`).
 *    - In either case, move the `current` pointer forward (`current = current->next`).
 * 4. After the loop, one of the lists might still have remaining nodes. Append the rest of the non-null list to the end of our merged list.
 * 5. The merged list starts at `dummy->next`. Return this node.
 *
 * Complexity Analysis:
 * - Time Complexity: O(M + N), where M and N are the lengths of the two linked lists. We iterate through each node of both lists exactly once.
 * - Space Complexity: O(1). We are rearranging the existing nodes in-place, so we only use a few extra pointers. The new list reuses the nodes from the input lists.
 */
class Solution {
public:
    ListNode* mergeTwoLists(ListNode* l1, ListNode* l2) {
        ListNode dummy(0);
        ListNode* current = &dummy;

        while (l1 != nullptr && l2 != nullptr) {
            if (l1->val <= l2->val) {
                current->next = l1;
                l1 = l1->next;
            } else {
                current->next = l2;
                l2 = l2->next;
            }
            current = current->next;
        }

        // Append the remaining nodes
        if (l1 != nullptr) {
            current->next = l1;
        } else {
            current->next = l2;
        }

        return dummy.next;
    }
};

// Helper function to print a linked list
void printList(ListNode* head) {
    while (head != nullptr) {
        std::cout << head->val << " -> ";
        head = head->next;
    }
    std::cout << "nullptr" << std::endl;
}

// Helper function to create a linked list from a vector
ListNode* createList(const std::vector<int>& vals) {
    if (vals.empty()) return nullptr;
    ListNode* head = new ListNode(vals[0]);
    ListNode* current = head;
    for (size_t i = 1; i < vals.size(); ++i) {
        current->next = new ListNode(vals[i]);
        current = current->next;
    }
    return head;
}

// Test function
int main() {
    Solution sol;

    std::cout << "--- LeetCode 21: Merge Two Sorted Lists ---" << std::endl;

    // Create two sorted lists
    ListNode* l1 = createList({1, 2, 4});
    ListNode* l2 = createList({1, 3, 4});

    std::cout << "List 1: ";
    printList(l1);
    std::cout << "List 2: ";
    printList(l2);

    ListNode* mergedList = sol.mergeTwoLists(l1, l2);

    std::cout << "Merged List: ";
    printList(mergedList);

    // Note: In a real application, you would need to free the allocated memory for the lists.
    // This is omitted here for simplicity.

    return 0;
}
