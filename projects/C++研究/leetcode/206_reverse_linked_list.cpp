#include <iostream>
#include <vector>

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
 * @brief LeetCode 206: Reverse Linked List (反转链表)
 *
 * Problem Statement:
 * Given the head of a singly linked list, reverse the list, and return the reversed list's head.
 *
 * Algorithm Explanation (Iterative):
 * The iterative approach is the most common and efficient way to solve this. It involves rearranging pointers as we traverse the list.
 *
 * We need three pointers to keep track of the nodes:
 * - `prev`: Points to the previous node. Initially `nullptr` because the new tail's `next` will be null.
 * - `current`: Points to the current node we are processing. Initially the `head` of the list.
 * - `next_temp`: Temporarily stores the next node in the original list before we change the `current` node's `next` pointer.
 *
 * The process is as follows:
 * 1. Initialize `prev = nullptr` and `current = head`.
 * 2. Loop as long as `current` is not `nullptr`.
 * 3. Inside the loop:
 *    a) Store the next node: `next_temp = current->next`.
 *    b) Reverse the `current` node's pointer: `current->next = prev`. This is the core reversal step.
 *    c) Move `prev` one step forward: `prev = current`.
 *    d) Move `current` one step forward: `current = next_temp`.
 * 4. When the loop finishes, `current` will be `nullptr`, and `prev` will be pointing to the new head of the reversed list. Return `prev`.
 *
 * This process effectively reverses the direction of the `next` pointers for each node one by one.
 *
 * Complexity Analysis:
 * - Time Complexity: O(N), where N is the number of nodes in the list. We traverse the list exactly once.
 * - Space Complexity: O(1). We only use a few pointers, so the space is constant regardless of the list size.
 */
class Solution {
public:
    ListNode* reverseList(ListNode* head) {
        ListNode* prev = nullptr;
        ListNode* current = head;

        while (current != nullptr) {
            ListNode* next_temp = current->next; // Store the next node
            current->next = prev;               // Reverse the link
            prev = current;                     // Move prev to current
            current = next_temp;                // Move to the next node in the original list
        }

        return prev; // `prev` is now the new head
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

    std::cout << "--- LeetCode 206: Reverse Linked List ---" << std::endl;

    // Create a linked list: 1 -> 2 -> 3 -> 4 -> 5
    ListNode* head = createList({1, 2, 3, 4, 5});

    std::cout << "Original List: ";
    printList(head);

    ListNode* reversedHead = sol.reverseList(head);

    std::cout << "Reversed List: ";
    printList(reversedHead);

    // Note: Memory for the list nodes should be freed in a real application.
    return 0;
}
