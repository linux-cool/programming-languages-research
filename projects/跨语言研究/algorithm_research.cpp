#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>

// LeetCode 算法研究与解析

class Solution {
public:
    /**
     * @brief LeetCode 1. 两数之和 (Two Sum)
     * 
     * 问题描述:
     * 给定一个整数数组 nums 和一个整数目标值 target，请你在该数组中找出 和为目标值 target 的那两个整数，并返回它们的数组下标。
     * 
     * 解法思路:
     * 使用哈希表（unordered_map）来存储已经遍历过的数字及其索引。
     * 遍历数组，对于每个数字 `num`，计算需要的另一个数字 `complement = target - num`。
     * 在哈希表中查找 `complement` 是否存在。
     * - 如果存在，说明找到了配对，直接返回两个数字的索引。
     * - 如果不存在，将当前数字 `num` 及其索引存入哈希表，继续遍历。
     * 
     * 复杂度分析:
     * - 时间复杂度: O(N)，其中 N 是数组中元素的数量。我们只需要遍历数组一次。哈希表的插入和查找操作平均时间复杂度为 O(1)。
     * - 空间复杂度: O(N)，最坏情况下，我们需要将数组中的每个元素都存入哈希表。
     */
    std::vector<int> twoSum(std::vector<int>& nums, int target) {
        std::unordered_map<int, int> num_map;
        for (int i = 0; i < nums.size(); ++i) {
            int complement = target - nums[i];
            if (num_map.count(complement)) {
                return {num_map[complement], i};
            }
            num_map[nums[i]] = i;
        }
        return {}; // 未找到结果
    }

    /**
     * @brief LeetCode 3. 无重复字符的最长子串 (Longest Substring Without Repeating Characters)
     * 
     * 问题描述:
     * 给定一个字符串 s ，请你找出其中不含有重复字符的 最长子串 的长度。
     * 
     * 解法思路:
     * 使用滑动窗口（Sliding Window）和哈希表（unordered_map）来解决。
     * 维护一个窗口 [left, right]，表示当前正在检查的无重复字符的子串。
     * - right 指针向右移动，扩大窗口，将新字符加入哈希表（记录字符及其最新位置）。
     * - 如果遇到一个重复字符（该字符已在哈希表中且其位置在 left 指针或其后），说明当前窗口不再有效。
     * - 需要将 left 指针移动到重复字符上次出现位置的下一个位置，以确保窗口内没有重复字符。
     * - 在整个过程中，不断更新最长子串的长度。
     * 
     * 复杂度分析:
     * - 时间复杂度: O(N)，其中 N 是字符串的长度。每个字符最多被访问两次（一次被 right 指针访问，一次被 left 指针访问）。
     * - 空间复杂度: O(M)，其中 M 是字符集的大小。哈希表最多存储 M 个不同的字符。
     */
    int lengthOfLongestSubstring(std::string s) {
        std::unordered_map<char, int> char_map;
        int max_len = 0;
        for (int left = 0, right = 0; right < s.length(); ++right) {
            char current_char = s[right];
            if (char_map.count(current_char)) {
                // 更新 left 指针，确保窗口内无重复
                // max 的作用是防止 left 指针回退
                left = std::max(left, char_map[current_char] + 1);
            }
            char_map[current_char] = right;
            max_len = std::max(max_len, right - left + 1);
        }
        return max_len;
    }
};

void testTwoSum() {
    Solution sol;
    std::vector<int> nums = {2, 7, 11, 15};
    int target = 9;
    std::vector<int> result = sol.twoSum(nums, target);
    std::cout << "--- LeetCode 1. 两数之和 ---" << std::endl;
    std::cout << "输入: nums = [2, 7, 11, 15], target = 9" << std::endl;
    if (!result.empty()) {
        std::cout << "输出: [" << result[0] << ", " << result[1] << "]" << std::endl;
    } else {
        std::cout << "未找到解" << std::endl;
    }
    std::cout << std::endl;
}

void testLengthOfLongestSubstring() {
    Solution sol;
    std::string s = "abcabcbb";
    int result = sol.lengthOfLongestSubstring(s);
    std::cout << "--- LeetCode 3. 无重复字符的最长子串 ---" << std::endl;
    std::cout << "输入: s = \"abcabcbb\"" << std::endl;
    std::cout << "输出: " << result << std::endl;
    std::cout << std::endl;
}

int main() {
    std::cout << "=== C/C++ 算法研究与解析 (LeetCode) ===" << std::endl << std::endl;
    testTwoSum();
    testLengthOfLongestSubstring();
    return 0;
}
