#include <iostream>
#include <vector>
#include <unordered_map>

/**
 * @brief LeetCode 1: Two Sum (两数之和)
 *
 * Problem Statement:
 * Given an array of integers `nums` and an integer `target`, return indices of the two numbers such that they add up to `target`.
 * You may assume that each input would have exactly one solution, and you may not use the same element twice.
 *
 * Algorithm Explanation:
 * We can solve this efficiently using a hash map (unordered_map in C++). The idea is to iterate through the array once. For each element `nums[i]`, we calculate its "complement" which is `target - nums[i]`.
 *
 * 1. Create an empty hash map to store numbers we've seen and their indices.
 * 2. Iterate through the `nums` array from `i = 0` to `n-1`.
 * 3. In each iteration, calculate `complement = target - nums[i]`.
 * 4. Check if the `complement` exists in the hash map.
 *    - If it exists, we have found our pair. The indices are the current index `i` and the index stored in the hash map for the complement. Return these two indices.
 *    - If it does not exist, add the current number `nums[i]` and its index `i` to the hash map.
 *
 * This approach ensures that we find the solution in a single pass.
 *
 * Complexity Analysis:
 * - Time Complexity: O(N), where N is the number of elements in the array. We traverse the list containing N elements only once. Each lookup and insertion in the hash map takes average O(1) time.
 * - Space Complexity: O(N), as in the worst case, we might need to store all N elements in the hash map.
 */
class Solution {
public:
    std::vector<int> twoSum(std::vector<int>& nums, int target) {
        std::unordered_map<int, int> num_to_index_map;
        for (int i = 0; i < nums.size(); ++i) {
            int complement = target - nums[i];
            if (num_to_index_map.count(complement)) {
                return {num_to_index_map[complement], i};
            }
            num_to_index_map[nums[i]] = i;
        }
        // Should not happen based on problem description (exactly one solution)
        return {};
    }
};

// Test function
int main() {
    Solution sol;
    std::vector<int> nums = {2, 7, 11, 15};
    int target = 9;

    std::cout << "--- LeetCode 1: Two Sum ---" << std::endl;
    std::cout << "Input: nums = [2, 7, 11, 15], target = 9" << std::endl;

    std::vector<int> result = sol.twoSum(nums, target);

    if (!result.empty()) {
        std::cout << "Output: [" << result[0] << ", " << result[1] << "]" << std::endl;
    } else {
        std::cout << "No solution found." << std::endl;
    }

    return 0;
}
