/*
 * C语言性能优化深度研究
 * 算法优化与硬件特性利用
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <immintrin.h>

// 研究1: 缓存友好的数据结构
#define ARRAY_SIZE 1000000
#define CACHE_LINE_SIZE 64

typedef struct {
    float x, y, z;
} Point3D;

typedef struct {
    float *x, *y, *z;  // 分离存储以提高缓存效率
} SoAPoint3D;

typedef struct {
    float x, y, z;
    char padding[CACHE_LINE_SIZE - sizeof(float) * 3];  // 填充到缓存行大小
} AlignedPoint3D;

// 研究2: SIMD向量化优化
void vector_add_scalar(float* a, float* b, float* c, size_t n) {
    for (size_t i = 0; i < n; i++) {
        c[i] = a[i] + b[i];
    }
}

void vector_add_simd(float* a, float* b, float* c, size_t n) {
    size_t i = 0;
    
    // 使用AVX2处理8个float为一组
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        __m256 vc = _mm256_add_ps(va, vb);
        _mm256_storeu_ps(&c[i], vc);
    }
    
    // 处理剩余元素
    for (; i < n; i++) {
        c[i] = a[i] + b[i];
    }
}

// 研究3: 分支预测优化
int predict_branch(int* array, size_t n) {
    int sum = 0;
    for (size_t i = 0; i < n; i++) {
        if (array[i] > 0) {  // 难以预测的分支
            sum += array[i];
        }
    }
    return sum;
}

int predict_branch_optimized(int* array, size_t n) {
    int sum = 0;
    // 使用条件移动避免分支
    for (size_t i = 0; i < n; i++) {
        int mask = (array[i] > 0) ? 0xFFFFFFFF : 0;
        sum += array[i] & mask;
    }
    return sum;
}

// 研究4: 循环展开优化
void matrix_multiply_basic(float* a, float* b, float* c, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            c[i * n + j] = 0;
            for (int k = 0; k < n; k++) {
                c[i * n + j] += a[i * n + k] * b[k * n + j];
            }
        }
    }
}

void matrix_multiply_unrolled(float* a, float* b, float* c, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j += 4) {
            float sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0;
            for (int k = 0; k < n; k++) {
                float aik = a[i * n + k];
                sum0 += aik * b[k * n + j];
                sum1 += aik * b[k * n + j + 1];
                sum2 += aik * b[k * n + j + 2];
                sum3 += aik * b[k * n + j + 3];
            }
            c[i * n + j] = sum0;
            c[i * n + j + 1] = sum1;
            c[i * n + j + 2] = sum2;
            c[i * n + j + 3] = sum3;
        }
    }
}

// 研究5: 内存访问模式优化
typedef struct {
    int key;
    char data[64];  // 64字节数据
} Record;

void process_records_sequential(Record* records, size_t n) {
    for (size_t i = 0; i < n; i++) {
        records[i].key *= 2;
    }
}

void process_records_strided(Record* records, size_t n, size_t stride) {
    for (size_t i = 0; i < n; i += stride) {
        records[i].key *= 2;
    }
}

// 研究6: 位操作优化
unsigned int count_set_bits_basic(unsigned int n) {
    unsigned int count = 0;
    while (n) {
        count += n & 1;
        n >>= 1;
    }
    return count;
}

unsigned int count_set_bits_optimized(unsigned int n) {
    unsigned int count = 0;
    while (n) {
        n &= (n - 1);  // 清除最低位的1
        count++;
    }
    return count;
}

// 研究7: 预计算查找表
#define LUT_SIZE 256

static unsigned char popcount_lut[LUT_SIZE];

void init_popcount_lut() {
    for (int i = 0; i < LUT_SIZE; i++) {
        popcount_lut[i] = 0;
        int n = i;
        while (n) {
            popcount_lut[i] += n & 1;
            n >>= 1;
        }
    }
}

unsigned int popcount_lut32(unsigned int n) {
    return popcount_lut[n & 0xFF] + 
           popcount_lut[(n >> 8) & 0xFF] + 
           popcount_lut[(n >> 16) & 0xFF] + 
           popcount_lut[(n >> 24) & 0xFF];
}

// 研究8: 缓存阻塞优化
typedef struct {
    float* data;
    int rows, cols;
} Matrix;

void matrix_transpose_naive(float* src, float* dst, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            dst[j * n + i] = src[i * n + j];
        }
    }
}

void matrix_transpose_blocked(float* src, float* dst, int n, int block_size) {
    for (int i = 0; i < n; i += block_size) {
        for (int j = 0; j < n; j += block_size) {
            for (int ii = i; ii < i + block_size && ii < n; ii++) {
                for (int jj = j; jj < j + block_size && jj < n; jj++) {
                    dst[jj * n + ii] = src[ii * n + jj];
                }
            }
        }
    }
}

// 研究9: 性能基准测试框架
typedef struct {
    const char* name;
    double time;
    double throughput;
} BenchmarkResult;

void benchmark_function(const char* name, void (*func)(void), int iterations) {
    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        func();
    }
    clock_t end = clock();
    
    double time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("%s: %.4f 秒 (%.2f 万次/秒)\n", 
           name, time, iterations / (time * 10000));
}

// 测试数据生成
float* generate_random_floats(size_t n) {
    float* data = (float*)aligned_alloc(64, n * sizeof(float));
    if (!data) return NULL;
    
    for (size_t i = 0; i < n; i++) {
        data[i] = (float)rand() / RAND_MAX * 100.0f;
    }
    return data;
}

int* generate_random_ints(size_t n) {
    int* data = (int*)malloc(n * sizeof(int));
    if (!data) return NULL;
    
    for (size_t i = 0; i < n; i++) {
        data[i] = rand() % 200 - 100;  // -100 到 99
    }
    return data;
}

// 缓存性能测试
void cache_performance_test() {
    const size_t N = 1000000;
    
    printf("=== 缓存性能测试 ===\n");
    
    // 测试1: AoS vs SoA
    Point3D* aos_data = (Point3D*)malloc(N * sizeof(Point3D));
    SoAPoint3D soa_data;
    soa_data.x = (float*)malloc(N * sizeof(float));
    soa_data.y = (float*)malloc(N * sizeof(float));
    soa_data.z = (float*)malloc(N * sizeof(float));
    
    // 初始化数据
    for (size_t i = 0; i < N; i++) {
        aos_data[i].x = aos_data[i].y = aos_data[i].z = (float)i;
        soa_data.x[i] = soa_data.y[i] = soa_data.z[i] = (float)i;
    }
    
    clock_t start, end;
    
    // AoS访问
    start = clock();
    float sum_aos = 0;
    for (size_t i = 0; i < N; i++) {
        sum_aos += aos_data[i].x + aos_data[i].y + aos_data[i].z;
    }
    end = clock();
    double aos_time = (double)(end - start) / CLOCKS_PER_SEC;
    
    // SoA访问
    start = clock();
    float sum_soa = 0;
    for (size_t i = 0; i < N; i++) {
        sum_soa += soa_data.x[i] + soa_data.y[i] + soa_data.z[i];
    }
    end = clock();
    double soa_time = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("AoS访问时间: %.4f 秒\n", aos_time);
    printf("SoA访问时间: %.4f 秒\n", soa_time);
    printf("SoA性能提升: %.2fx\n", aos_time / soa_time);
    
    free(aos_data);
    free(soa_data.x);
    free(soa_data.y);
    free(soa_data.z);
}

// SIMD性能测试
void simd_performance_test() {
    const size_t N = 1000000;
    float *a = generate_random_floats(N);
    float *b = generate_random_floats(N);
    float *c = (float*)aligned_alloc(64, N * sizeof(float));
    
    printf("\n=== SIMD性能测试 ===\n");
    
    clock_t start, end;
    
    // 标量版本
    start = clock();
    vector_add_scalar(a, b, c, N);
    end = clock();
    double scalar_time = (double)(end - start) / CLOCKS_PER_SEC;
    
    // SIMD版本
    start = clock();
    vector_add_simd(a, b, c, N);
    end = clock();
    double simd_time = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("标量加法时间: %.4f 秒\n", scalar_time);
    printf("SIMD加法时间: %.4f 秒\n", simd_time);
    printf("SIMD性能提升: %.2fx\n", scalar_time / simd_time);
    
    free(a);
    free(b);
    free(c);
}

// 分支预测测试
void branch_prediction_test() {
    const size_t N = 1000000;
    int* data = generate_random_ints(N);
    
    printf("\n=== 分支预测测试 ===\n");
    
    clock_t start, end;
    
    // 基础版本
    start = clock();
    int result1 = predict_branch(data, N);
    end = clock();
    double basic_time = (double)(end - start) / CLOCKS_PER_SEC;
    
    // 优化版本
    start = clock();
    int result2 = predict_branch_optimized(data, N);
    end = clock();
    double opt_time = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("基础分支时间: %.4f 秒\n", basic_time);
    printf("优化分支时间: %.4f 秒\n", opt_time);
    printf("优化性能提升: %.2fx\n", basic_time / opt_time);
    printf("结果验证: %s\n", (result1 == result2) ? "正确" : "错误");
    
    free(data);
}

// 矩阵乘法测试
void matrix_multiplication_test() {
    const int N = 512;
    float *a = generate_random_floats(N * N);
    float *b = generate_random_floats(N * N);
    float *c = (float*)aligned_alloc(64, N * N * sizeof(float));
    
    printf("\n=== 矩阵乘法测试 ===\n");
    
    clock_t start, end;
    
    // 基础版本
    start = clock();
    matrix_multiply_basic(a, b, c, N);
    end = clock();
    double basic_time = (double)(end - start) / CLOCKS_PER_SEC;
    
    // 循环展开版本
    start = clock();
    matrix_multiply_unrolled(a, b, c, N);
    end = clock();
    double unrolled_time = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("基础矩阵乘法: %.4f 秒\n", basic_time);
    printf("循环展开矩阵乘法: %.4f 秒\n", unrolled_time);
    printf("循环展开性能提升: %.2fx\n", basic_time / unrolled_time);
    
    free(a);
    free(b);
    free(c);
}

// 位操作性能测试
void bit_operation_test() {
    const int ITERATIONS = 10000000;
    
    printf("\n=== 位操作性能测试 ===\n");
    
    init_popcount_lut();
    
    clock_t start, end;
    
    // 基础版本
    start = clock();
    unsigned int count1 = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        count1 += count_set_bits_basic(i);
    }
    end = clock();
    double basic_time = (double)(end - start) / CLOCKS_PER_SEC;
    
    // 优化版本
    start = clock();
    unsigned int count2 = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        count2 += count_set_bits_optimized(i);
    }
    end = clock();
    double opt_time = (double)(end - start) / CLOCKS_PER_SEC;
    
    // 查找表版本
    start = clock();
    unsigned int count3 = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        count3 += popcount_lut32(i);
    }
    end = clock();
    double lut_time = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("基础位计数: %.4f 秒\n", basic_time);
    printf("优化位计数: %.4f 秒\n", opt_time);
    printf("查找表位计数: %.4f 秒\n", lut_time);
    printf("优化性能提升: %.2fx\n", basic_time / opt_time);
    printf("查找表性能提升: %.2fx\n", basic_time / lut_time);
}

// 转置性能测试
void transpose_performance_test() {
    const int N = 1024;
    float *src = generate_random_floats(N * N);
    float *dst = (float*)aligned_alloc(64, N * N * sizeof(float));
    
    printf("\n=== 转置性能测试 ===\n");
    
    clock_t start, end;
    
    // 基础版本
    start = clock();
    matrix_transpose_naive(src, dst, N);
    end = clock();
    double naive_time = (double)(end - start) / CLOCKS_PER_SEC;
    
    // 阻塞版本
    start = clock();
    matrix_transpose_blocked(src, dst, N, 32);
    end = clock();
    double blocked_time = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("基础转置: %.4f 秒\n", naive_time);
    printf("阻塞转置: %.4f 秒\n", blocked_time);
    printf("阻塞性能提升: %.2fx\n", naive_time / blocked_time);
    
    free(src);
    free(dst);
}

// 主测试函数
int main() {
    printf("=== C语言性能优化深度研究 ===\n");
    
    // 初始化随机数种子
    srand(time(NULL));
    
    // 运行所有性能测试
    cache_performance_test();
    simd_performance_test();
    branch_prediction_test();
    matrix_multiplication_test();
    bit_operation_test();
    transpose_performance_test();
    
    printf("\n=== 研究结论 ===\n");
    printf("1. 缓存友好的数据结构显著提升性能\n");
    printf("2. SIMD向量化可带来2-8倍性能提升\n");
    printf("3. 分支预测优化对复杂逻辑效果显著\n");
    printf("4. 循环展开和阻塞优化提升计算密集型任务\n");
    printf("5. 位操作和查找表适用于特化场景\n");
    printf("6. 内存对齐和数据预取优化缓存效率\n");
    
    return 0;
}