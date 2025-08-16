#include <iostream>
#include <vector>

/**
 * @brief LeetCode 704: Binary Search (二分查找)
 *
 * Problem Statement:
 * Given an array of integers `nums` which is sorted in ascending order, and an integer `target`, write a function to search `target` in `nums`. If `target` exists, then return its index. Otherwise, return -1.
 * You must write an algorithm with O(log n) runtime complexity.
 *
 * Algorithm Explanation:
 * Binary search is an efficient algorithm for finding an item from a sorted list of items. It works by repeatedly dividing the search interval in half.
 *
 * 1. Initialize two pointers, `left` and `right`, to the beginning and end of the array, respectively. `left = 0`, `right = nums.size() - 1`.
 * 2. Loop as long as the search interval is valid (`left <= right`).
 * 3. Calculate the middle index of the current interval: `mid = left + (right - left) / 2`. This way of calculating `mid` avoids potential integer overflow compared to `(left + right) / 2`.
 * 4. Compare the element at the middle index, `nums[mid]`, with the `target`:
 *    a) If `nums[mid] == target`, the element is found. Return `mid`.
 *    b) If `nums[mid] < target`, the target must be in the right half of the interval (if it exists). So, discard the left half by moving the `left` pointer: `left = mid + 1`.
 *    c) If `nums[mid] > target`, the target must be in the left half of the interval. Discard the right half by moving the `right` pointer: `right = mid - 1`.
 * 5. If the loop finishes without finding the target, it means the target is not in the array. Return -1.
 *
 * This process halves the search space in each iteration, leading to a logarithmic time complexity.
 *
 * Complexity Analysis:
 * - Time Complexity: O(log N), where N is the number of elements in the array. With each comparison, we reduce the search space by half.
 * - Space Complexity: O(1). We only use a few variables (`left`, `right`, `mid`) to perform the search.
 */
class Solution {
public:
    int search(std::vector<int>& nums, int target) {
        int left = 0;
        int right = nums.size() - 1;

        while (left <= right) {
            // Avoid potential overflow with (left + right) / 2
            int mid = left + (right - left) / 2;

            if (nums[mid] == target) {
                return mid;
            } else if (nums[mid] < target) {
                left = mid + 1;
            } else {
                right = mid - 1;
            }
        }

        // Target was not found
        return -1;
    }
};

// Test function
int main() {
    Solution sol;

    std::cout << "--- LeetCode 704: Binary Search ---" << std::endl;

    std::vector<int> nums1 = {-1, 0, 3, 5, 9, 12};
    int target1 = 9;
    std::cout << "Input: nums = [-1, 0, 3, 5, 9, 12], target = 9" << std::endl;
    std::cout << "Output: " << sol.search(nums1, target1) << std::endl; // Expected: 4

    std::vector<int> nums2 = {-1, 0, 3, 5, 9, 12};
    int target2 = 2;
    std::cout << "Input: nums = [-1, 0, 3, 5, 9, 12], target = 2" << std::endl;
    std::cout << "Output: " << sol.search(nums2, target2) << std::endl; // Expected: -1

    return 0;
}
