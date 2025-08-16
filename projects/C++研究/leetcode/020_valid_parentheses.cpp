#include <iostream>
#include <string>
#include <stack>
#include <unordered_map>

/**
 * @brief LeetCode 20: Valid Parentheses (有效的括号)
 *
 * Problem Statement:
 * Given a string `s` containing just the characters '(', ')', '{', '}', '[' and ']', determine if the input string is valid.
 * An input string is valid if:
 * 1. Open brackets must be closed by the same type of brackets.
 * 2. Open brackets must be closed in the correct order.
 *
 * Algorithm Explanation:
 * This problem is a perfect use case for a stack data structure. The Last-In, First-Out (LIFO) nature of a stack helps ensure that brackets are closed in the correct order.
 *
 * 1. Create a stack to store the opening brackets.
 * 2. Create a hash map to store the mappings between closing and opening brackets (e.g., `)` maps to `(`, `}` maps to `{`). This makes checking for pairs easy.
 * 3. Iterate through the input string `s` character by character.
 * 4. For each character:
 *    a) If it's an opening bracket ('(', '{', '['), push it onto the stack.
 *    b) If it's a closing bracket (')', '}', ']'):
 *       i) Check if the stack is empty. If it is, we have a closing bracket without a corresponding opener. The string is invalid.
 *       ii) If the stack is not empty, pop the top element. This element should be the corresponding opening bracket.
 *       iii) Check if the popped bracket matches the current closing bracket (using our hash map). If they don't match, the string is invalid.
 * 5. After the loop finishes, check if the stack is empty.
 *    - If it is empty, it means every opening bracket had a corresponding closing bracket in the correct order. The string is valid.
 *    - If it's not empty, it means there are unclosed opening brackets. The string is invalid.
 *
 * Complexity Analysis:
 * - Time Complexity: O(N), where N is the length of the string. We iterate through the string once. Stack operations (push, pop, top) take O(1) time.
 * - Space Complexity: O(N). In the worst case, if the string consists of only opening brackets (e.g., "((((("), we would push all N characters onto the stack.
 */
class Solution {
public:
    bool isValid(std::string s) {
        std::stack<char> bracket_stack;
        std::unordered_map<char, char> bracket_map = {
            {')', '('},
            {'}', '{'},
            {']', '['}
        };

        for (char c : s) {
            // If it's a closing bracket
            if (bracket_map.count(c)) {
                if (bracket_stack.empty() || bracket_stack.top() != bracket_map[c]) {
                    return false; // No matching opening bracket
                }
                bracket_stack.pop();
            } else {
                // It's an opening bracket
                bracket_stack.push(c);
            }
        }

        // If stack is empty, all brackets were matched correctly
        return bracket_stack.empty();
    }
};

// Test function
int main() {
    Solution sol;

    std::cout << "--- LeetCode 20: Valid Parentheses ---" << std::endl;

    std::string s1 = "()";
    std::cout << "Input: \"" << s1 << "\"" << std::endl;
    std::cout << "Output: " << (sol.isValid(s1) ? "true" : "false") << std::endl; // Expected: true

    std::string s2 = "()[]{}";
    std::cout << "Input: \"" << s2 << "\"" << std::endl;
    std::cout << "Output: " << (sol.isValid(s2) ? "true" : "false") << std::endl; // Expected: true

    std::string s3 = "(]";
    std::cout << "Input: \"" << s3 << "\"" << std::endl;
    std::cout << "Output: " << (sol.isValid(s3) ? "true" : "false") << std::endl; // Expected: false
    
    std::string s4 = "([)]";
    std::cout << "Input: \"" << s4 << "\"" << std::endl;
    std::cout << "Output: " << (sol.isValid(s4) ? "true" : "false") << std::endl; // Expected: false

    std::string s5 = "{[]}";
    std::cout << "Input: \"" << s5 << "\"" << std::endl;
    std::cout << "Output: " << (sol.isValid(s5) ? "true" : "false") << std::endl; // Expected: true

    return 0;
}
