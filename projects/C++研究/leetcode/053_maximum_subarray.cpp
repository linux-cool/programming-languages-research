#include <iostream>
#include <vector>
#include <algorithm>

/**
 * @brief LeetCode 53: Maximum Subarray (最大子数组和)
 *
 * Problem Statement:
 * Given an integer array `nums`, find the contiguous subarray (containing at least one number) which has the largest sum and return its sum.
 *
 * Algorithm Explanation (Kadane's Algorithm):
 * This is a classic problem that can be solved efficiently using Kadane's Algorithm, which is a dynamic programming approach.
 *
 * The core idea is to iterate through the array and, at each position `i`, determine the maximum subarray sum that *ends* at that position.
 *
 * Let `current_max` be the maximum subarray sum ending at the current position.
 * Let `global_max` be the maximum subarray sum found so far anywhere in the array.
 *
 * 1. Initialize `current_max` and `global_max` to the first element of the array.
 * 2. Iterate through the array starting from the second element (`i = 1`).
 * 3. For each element `nums[i]`, we have two choices for the maximum subarray ending at `i`:
 *    a) Start a new subarray at `nums[i]`. The sum would be just `nums[i]`.
 *    b) Extend the previous maximum subarray by adding `nums[i]` to it. The sum would be `current_max + nums[i]`.
 *    We choose the larger of these two options: `current_max = max(nums[i], current_max + nums[i])`.
 * 4. After updating `current_max`, we compare it with `global_max` and update `global_max` if `current_max` is larger: `global_max = max(global_max, current_max)`.
 * 5. After the loop finishes, `global_max` will hold the maximum subarray sum for the entire array.
 *
 * This algorithm works because it discards any subarray with a negative sum. If `current_max` becomes negative, it's better to start a new subarray from the next element, which is exactly what `max(nums[i], current_max + nums[i])` does.
 *
 * Complexity Analysis:
 * - Time Complexity: O(N), where N is the number of elements in the array. We iterate through the array only once.
 * - Space Complexity: O(1). We only use a few variables to keep track of the current and global maximums.
 */
class Solution {
public:
    int maxSubArray(std::vector<int>& nums) {
        if (nums.empty()) {
            return 0;
        }

        int current_max = nums[0];
        int global_max = nums[0];

        for (size_t i = 1; i < nums.size(); ++i) {
            // Decide whether to extend the previous subarray or start a new one
            current_max = std::max(nums[i], current_max + nums[i]);
            // Update the overall maximum sum found so far
            global_max = std::max(global_max, current_max);
        }

        return global_max;
    }
};

// Test function
int main() {
    Solution sol;

    std::cout << "--- LeetCode 53: Maximum Subarray ---" << std::endl;

    std::vector<int> nums1 = {-2, 1, -3, 4, -1, 2, 1, -5, 4};
    std::cout << "Input: [-2, 1, -3, 4, -1, 2, 1, -5, 4]" << std::endl;
    std::cout << "Output: " << sol.maxSubArray(nums1) << std::endl; // Expected: 6 (from [4, -1, 2, 1])

    std::vector<int> nums2 = {1};
    std::cout << "Input: [1]" << std::endl;
    std::cout << "Output: " << sol.maxSubArray(nums2) << std::endl; // Expected: 1

    std::vector<int> nums3 = {5, 4, -1, 7, 8};
    std::cout << "Input: [5, 4, -1, 7, 8]" << std::endl;
    std::cout << "Output: " << sol.maxSubArray(nums3) << std::endl; // Expected: 23 (the whole array)

    return 0;
}
