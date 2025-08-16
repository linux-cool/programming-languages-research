#include <iostream>
#include <string>
#include <cctype>
#include <algorithm>

/**
 * @brief LeetCode 125: Valid Palindrome (验证回文串)
 *
 * Problem Statement:
 * A phrase is a palindrome if, after converting all uppercase letters into lowercase letters and removing all non-alphanumeric characters, it reads the same forward and backward. Alphanumeric characters include letters and numbers.
 * Given a string `s`, return `true` if it is a palindrome, or `false` otherwise.
 *
 * Algorithm Explanation:
 * The problem can be solved using the "two pointers" technique.
 *
 * 1. Initialize two pointers, `left` at the beginning of the string (`0`) and `right` at the end of the string (`s.length() - 1`).
 * 2. Loop as long as `left` is less than `right`.
 * 3. In the loop, move the `left` pointer to the right until it points to an alphanumeric character.
 * 4. Similarly, move the `right` pointer to the left until it points to an alphanumeric character.
 * 5. Now, both pointers are on alphanumeric characters. Compare them after converting to lowercase.
 *    - If `tolower(s[left])` is not equal to `tolower(s[right])`, the string is not a palindrome. Return `false`.
 *    - If they are equal, the characters match. Move both pointers towards the center: `left++`, `right--`.
 * 6. If the loop completes without returning `false`, it means all corresponding alphanumeric characters matched. The string is a palindrome. Return `true`.
 *
 * This approach efficiently checks the string by skipping irrelevant characters and comparing only the essential ones.
 *
 * Complexity Analysis:
 * - Time Complexity: O(N), where N is the length of the string. In the worst case, each pointer traverses the entire string once.
 * - Space Complexity: O(1). We only use a few variables for the pointers, requiring no extra space proportional to the input size.
 */
class Solution {
public:
    bool isPalindrome(std::string s) {
        int left = 0;
        int right = s.length() - 1;

        while (left < right) {
            // Move left pointer to the next alphanumeric character
            while (left < right && !std::isalnum(s[left])) {
                left++;
            }
            // Move right pointer to the previous alphanumeric character
            while (left < right && !std::isalnum(s[right])) {
                right--;
            }

            // Compare the characters (case-insensitive)
            if (std::tolower(s[left]) != std::tolower(s[right])) {
                return false;
            }

            // Move pointers towards the center
            left++;
            right--;
        }

        return true;
    }
};

// Test function
int main() {
    Solution sol;

    std::cout << "--- LeetCode 125: Valid Palindrome ---" << std::endl;

    std::string s1 = "A man, a plan, a canal: Panama";
    std::cout << "Input: \"" << s1 << "\"" << std::endl;
    std::cout << "Output: " << (sol.isPalindrome(s1) ? "true" : "false") << "\n"; // Expected: true

    std::string s2 = "race a car";
    std::cout << "Input: \"" << s2 << "\"" << std::endl;
    std::cout << "Output: " << (sol.isPalindrome(s2) ? "true" : "false") << "\n"; // Expected: false

    std::string s3 = " ";
    std::cout << "Input: \"" << s3 << "\"" << std::endl;
    std::cout << "Output: " << (sol.isPalindrome(s3) ? "true" : "false") << "\n"; // Expected: true

    return 0;
}
