/*
 * 编译器优化与构建系统深度研究
 * GCC/Clang编译器优化技术与Makefile工程实践
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <x86intrin.h>

// 研究1: 编译器优化标志对比分析
#define ARRAY_SIZE 1000000

// 基础版本 - 无优化
void vector_add_baseline(float* a, float* b, float* c, int n) {
    for (int i = 0; i < n; i++) {
        c[i] = a[i] + b[i];
    }
}

// 优化版本1: 循环展开
void vector_add_unroll(float* a, float* b, float* c, int n) {
    int i = 0;
    // 展开8次
    for (; i <= n - 8; i += 8) {
        c[i] = a[i] + b[i];
        c[i+1] = a[i+1] + b[i+1];
        c[i+2] = a[i+2] + b[i+2];
        c[i+3] = a[i+3] + b[i+3];
        c[i+4] = a[i+4] + b[i+4];
        c[i+5] = a[i+5] + b[i+5];
        c[i+6] = a[i+6] + b[i+6];
        c[i+7] = a[i+7] + b[i+7];
    }
    // 处理剩余元素
    for (; i < n; i++) {
        c[i] = a[i] + b[i];
    }
}

// 优化版本2: SIMD向量化
void vector_add_simd(float* a, float* b, float* c, int n) {
    int i = 0;
    
    // 使用4元素SIMD
    for (; i <= n - 4; i += 4) {
        __m128 va = _mm_load_ps(&a[i]);
        __m128 vb = _mm_load_ps(&b[i]);
        __m128 vc = _mm_add_ps(va, vb);
        _mm_store_ps(&c[i], vc);
    }
    
    // 处理剩余元素
    for (; i < n; i++) {
        c[i] = a[i] + b[i];
    }
}

// 研究2: 内存对齐优化
typedef struct {
    float x, y, z, w;
} __attribute__((aligned(16))) Vector4;

// 研究3: 分支预测优化
int binary_search_optimized(int* arr, int n, int target) {
    int left = 0, right = n - 1;
    
    while (left <= right) {
        // 避免分支预测失败
        int mid = left + ((right - left) >> 1);
        int cmp = (arr[mid] > target) - (arr[mid] < target);
        
        if (cmp == 0) return mid;
        left = (cmp < 0) ? mid + 1 : left;
        right = (cmp > 0) ? mid - 1 : right;
    }
    return -1;
}

// 研究4: 缓存优化
#define BLOCK_SIZE 64
void matrix_multiply_cache_optimized(float* A, float* B, float* C, int n) {
    for (int i = 0; i < n; i += BLOCK_SIZE) {
        for (int j = 0; j < n; j += BLOCK_SIZE) {
            for (int k = 0; k < n; k += BLOCK_SIZE) {
                // 分块矩阵乘法
                for (int ii = i; ii < i + BLOCK_SIZE && ii < n; ii++) {
                    for (int jj = j; jj < j + BLOCK_SIZE && jj < n; jj++) {
                        float sum = 0.0f;
                        for (int kk = k; kk < k + BLOCK_SIZE && kk < n; kk++) {
                            sum += A[ii * n + kk] * B[kk * n + jj];
                        }
                        C[ii * n + jj] += sum;
                    }
                }
            }
        }
    }
}

// 研究5: 编译器内联优化
static inline float fast_distance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrtf(dx * dx + dy * dy);
}

// 研究6: 性能基准测试框架
typedef struct {
    const char* name;
    double time_ms;
    double throughput_mops;
    double speedup;
} BenchmarkResult;

void benchmark_function(void (*func)(float*, float*, float*, int), 
                       const char* name, float* a, float* b, float* c, int n) {
    clock_t start = clock();
    
    // 多次运行取平均值
    const int iterations = 10;
    for (int i = 0; i < iterations; i++) {
        func(a, b, c, n);
    }
    
    clock_t end = clock();
    double total_time = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;
    double avg_time = total_time / iterations;
    double throughput = (double)n / avg_time / 1000.0; // Mops
    
    printf("%-20s: %8.3f ms, %8.3f Mops/sec\n", name, avg_time, throughput);
}

// 研究7: 编译器特定优化
#ifdef __GNUC__
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

int optimized_search(int* arr, int n, int target) {
    if (UNLIKELY(n <= 0)) return -1;
    
    for (int i = 0; LIKELY(i < n); i++) {
        if (UNLIKELY(arr[i] == target)) {
            return i;
        }
    }
    return -1;
}

// 研究8: 内存预取优化
void prefetch_optimized_copy(float* dst, float* src, int n) {
    for (int i = 0; i < n; i += 8) {
        // 预取未来数据
        __builtin_prefetch(&src[i + 64], 0, 3);
        __builtin_prefetch(&dst[i + 64], 1, 3);
        
        for (int j = i; j < i + 8 && j < n; j++) {
            dst[j] = src[j];
        }
    }
}

// 研究9: 编译器优化报告生成
void generate_optimization_report() {
    printf("=== 编译器优化技术分析报告 ===\n\n");
    
    printf("1. 优化等级对比:\n");
    printf("   -O0: 无优化，调试友好\n");
    printf("   -O1: 启用基本优化\n");
    printf("   -O2: 启用大多数优化，推荐默认使用\n");
    printf("   -O3: 启用更激进的优化，如循环展开\n");
    printf("   -Os: 优化代码大小\n");
    printf("   -Ofast: 不保证标准兼容性，最大性能\n\n");
    
    printf("2. 特定优化标志:\n");
    printf("   -march=native: 针对本地CPU优化\n");
    printf("   -mtune=native: 针对本地CPU调优\n");
    printf("   -funroll-loops: 循环展开\n");
    printf("   -ffast-math: 激进数学优化\n");
    printf("   -fno-omit-frame-pointer: 保留帧指针用于调试\n\n");
    
    printf("3. 链接时优化:\n");
    printf("   -flto: 链接时优化\n");
    printf("   -fuse-linker-plugin: 使用链接器插件\n\n");
    
    printf("4. 分析工具:\n");
    printf("   gcc -S -fverbose-asm: 生成汇编代码\n");
    printf("   objdump -d: 反汇编二进制文件\n");
    printf("   perf record/report: 性能分析\n");
    printf("   valgrind: 内存检查\n");
}

// 主测试函数
int main() {
    printf("=== 编译器优化与构建系统深度研究 ===\n\n");
    
    // 分配测试数据
    float *a = (float*)aligned_alloc(64, ARRAY_SIZE * sizeof(float));
    float *b = (float*)aligned_alloc(64, ARRAY_SIZE * sizeof(float));
    float *c = (float*)aligned_alloc(64, ARRAY_SIZE * sizeof(float));
    
    if (!a || !b || !c) {
        printf("内存分配失败\n");
        return 1;
    }
    
    // 初始化数据
    for (int i = 0; i < ARRAY_SIZE; i++) {
        a[i] = (float)(rand() % 100);
        b[i] = (float)(rand() % 100);
    }
    
    printf("数组大小: %d 元素\n", ARRAY_SIZE);
    printf("数据类型: float (%zu bytes)\n", sizeof(float));
    printf("总内存: %.2f MB\n\n", (double)(ARRAY_SIZE * sizeof(float) * 3) / 1024 / 1024);
    
    // 运行基准测试
    printf("性能基准测试结果:\n");
    printf("================================\n");
    
    benchmark_function(vector_add_baseline, "基准版本", a, b, c, ARRAY_SIZE);
    benchmark_function(vector_add_unroll, "循环展开", a, b, c, ARRAY_SIZE);
    
    // 清理内存
    free(a);
    free(b);
    free(c);
    
    // 生成优化报告
    generate_optimization_report();
    
    printf("\n=== 编译优化建议 ===\n");
    printf("1. 使用-O2或-O3优化等级\n");
    printf("2. 启用-march=native针对本地CPU\n");
    printf("3. 使用restrict关键字帮助编译器优化\n");
    printf("4. 合理使用inline函数\n");
    printf("5. 考虑内存对齐和缓存优化\n");
    printf("6. 使用性能分析工具定位瓶颈\n");
    printf("7. 注意编译器警告信息\n");
    printf("8. 考虑使用LTO链接时优化\n");
    
    return 0;
}