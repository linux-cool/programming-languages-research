#include <iostream>
#include <string>
#include <unordered_map>
#include <algorithm>

/**
 * @brief LeetCode 3: Longest Substring Without Repeating Characters (无重复字符的最长子串)
 *
 * Problem Statement:
 * Given a string `s`, find the length of the longest substring without repeating characters.
 *
 * Algorithm Explanation:
 * This problem can be solved efficiently using the "sliding window" technique with a hash map. A sliding window is a conceptual sub-array or sub-string that "slides" over the main data structure.
 *
 * 1. Initialize two pointers, `left` and `right`, both at the beginning of the string. These pointers define our current window.
 * 2. Initialize a hash map `char_map` to store characters in the current window and their most recent indices.
 * 3. Initialize a variable `max_len` to 0 to keep track of the maximum length found so far.
 * 4. Iterate through the string with the `right` pointer from left to right.
 * 5. At each step, check if the character `s[right]` is already in our `char_map`.
 *    - If it is, it means we have a repeating character. We need to move our `left` pointer to the right of the last occurrence of this character to ensure the window is again free of repeats. We update `left = max(left, char_map[s[right]] + 1)`. The `max` is important to prevent `left` from moving backward if we find a character that was repeated but is outside our current window.
 *    - If it is not, the window is still valid.
 * 6. Update the character's last seen index in the hash map: `char_map[s[right]] = right`.
 * 7. Update the maximum length: `max_len = max(max_len, right - left + 1)`.
 * 8. Repeat until `right` reaches the end of the string.
 *
 * Complexity Analysis:
 * - Time Complexity: O(N), where N is the length of the string. Each character is visited by the `left` and `right` pointers at most once.
 * - Space Complexity: O(M), where M is the size of the character set. In the worst case, we store all unique characters in the hash map. For ASCII, this is O(128) or O(256), which is constant.
 */
class Solution {
public:
    int lengthOfLongestSubstring(std::string s) {
        std::unordered_map<char, int> char_map;
        int max_len = 0;
        int left = 0;

        for (int right = 0; right < s.length(); ++right) {
            char current_char = s[right];
            if (char_map.count(current_char)) {
                // Move the left pointer to the right of the last occurrence
                left = std::max(left, char_map[current_char] + 1);
            }
            // Update the character's last seen index
            char_map[current_char] = right;
            // Update the maximum length
            max_len = std::max(max_len, right - left + 1);
        }
        return max_len;
    }
};

// Test function
int main() {
    Solution sol;
    std::string s1 = "abcabcbb";
    std::string s2 = "bbbbb";
    std::string s3 = "pwwkew";

    std::cout << "--- LeetCode 3: Longest Substring Without Repeating Characters ---" << std::endl;
    
    std::cout << "Input: \"" << s1 << "\"" << std::endl;
    std::cout << "Output: " << sol.lengthOfLongestSubstring(s1) << std::endl; // Expected: 3

    std::cout << "Input: \"" << s2 << "\"" << std::endl;
    std::cout << "Output: " << sol.lengthOfLongestSubstring(s2) << std::endl; // Expected: 1

    std::cout << "Input: \"" << s3 << "\"" << std::endl;
    std::cout << "Output: " << sol.lengthOfLongestSubstring(s3) << std::endl; // Expected: 3

    return 0;
}
