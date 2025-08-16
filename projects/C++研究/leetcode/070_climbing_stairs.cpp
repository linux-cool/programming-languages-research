#include <iostream>
#include <vector>

/**
 * @brief LeetCode 70: Climbing Stairs (爬楼梯)
 *
 * Problem Statement:
 * You are climbing a staircase. It takes `n` steps to reach the top.
 * Each time you can either climb 1 or 2 steps. In how many distinct ways can you climb to the top?
 *
 * Algorithm Explanation:
 * This is a classic dynamic programming problem. Let `dp[i]` be the number of distinct ways to reach the `i`-th step.
 *
 * To reach step `i`, you could have come from either step `i-1` (by taking one step) or step `i-2` (by taking two steps).
 * Therefore, the total number of ways to reach step `i` is the sum of the ways to reach step `i-1` and the ways to reach step `i-2`.
 * This gives us the recurrence relation: `dp[i] = dp[i-1] + dp[i-2]`.
 *
 * This is the same recurrence relation as the Fibonacci sequence.
 *
 * Base Cases:
 * - `dp[0] = 1` (conceptually, there's one way to be at the start - by not moving).
 * - `dp[1] = 1` (one way to reach the first step: take one 1-step).
 * - `dp[2] = 2` (two ways to reach the second step: 1+1 or 2).
 *
 * We can implement this using a bottom-up approach:
 * 1. Create a `dp` array of size `n+1`.
 * 2. Initialize the base cases: `dp[0] = 1`, `dp[1] = 1`.
 * 3. Iterate from `i = 2` to `n`, and for each `i`, calculate `dp[i] = dp[i-1] + dp[i-2]`.
 * 4. The final answer is `dp[n]`.
 *
 * Space Optimization:
 * Notice that to calculate `dp[i]`, we only need the values of `dp[i-1]` and `dp[i-2]`. We don't need the entire `dp` array. We can optimize the space by only storing the last two values.
 * Let `a` be the ways to reach `i-2` and `b` be the ways to reach `i-1`. Then the ways to reach `i` is `a + b`. We can update `a` and `b` in a loop.
 *
 * Complexity Analysis (Optimized):
 * - Time Complexity: O(N). We iterate from 2 to N once.
 * - Space Complexity: O(1). We only use a few variables to store the intermediate results.
 */
class Solution {
public:
    int climbStairs(int n) {
        if (n <= 1) {
            return 1;
        }

        // Space-optimized DP
        int prev2 = 1; // Represents dp[i-2], starts at ways to reach step 0
        int prev1 = 1; // Represents dp[i-1], starts at ways to reach step 1
        int current = 0;

        for (int i = 2; i <= n; ++i) {
            current = prev1 + prev2;
            prev2 = prev1;
            prev1 = current;
        }

        return prev1; // prev1 holds the final result for n
    }
};

// Test function
int main() {
    Solution sol;

    std::cout << "--- LeetCode 70: Climbing Stairs ---" << std::endl;

    int n1 = 2;
    std::cout << "Input: n = " << n1 << std::endl;
    std::cout << "Output: " << sol.climbStairs(n1) << std::endl; // Expected: 2

    int n2 = 3;
    std::cout << "Input: n = " << n2 << std::endl;
    std::cout << "Output: " << sol.climbStairs(n2) << std::endl; // Expected: 3
    
    int n3 = 5;
    std::cout << "Input: n = " << n3 << std::endl;
    std::cout << "Output: " << sol.climbStairs(n3) << std::endl; // Expected: 8

    return 0;
}
