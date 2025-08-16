/*
 * C语言性能基准测试深度研究
 * 多维度性能测试与优化分析
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>

// 高精度计时器
#define GET_TIME(start) gettimeofday(&start, NULL)
#define TIME_DIFF(start, end) ((end.tv_sec - start.tv_sec) * 1000000.0 + (end.tv_usec - start.tv_usec))

// 研究1: CPU性能基准测试
void benchmark_cpu_arithmetic() {
    printf("=== CPU算术性能测试 ===\n");
    
    struct timeval start, end;
    const int iterations = 10000000;
    
    // 整数加法测试
    GET_TIME(start);
    volatile int sum = 0;
    for (int i = 0; i < iterations; i++) {
        sum += i;
    }
    GET_TIME(end);
    double int_add_time = TIME_DIFF(start, end);
    
    // 浮点加法测试
    GET_TIME(start);
    volatile double fsum = 0.0;
    for (int i = 0; i < iterations; i++) {
        fsum += (double)i;
    }
    GET_TIME(end);
    double float_add_time = TIME_DIFF(start, end);
    
    // 乘法测试
    GET_TIME(start);
    volatile double product = 1.0;
    for (int i = 1; i < iterations; i++) {
        product *= 1.000001;
    }
    GET_TIME(end);
    double multiply_time = TIME_DIFF(start, end);
    
    // 除法测试
    GET_TIME(start);
    volatile double quotient = 1.0;
    for (int i = 1; i < iterations; i++) {
        quotient /= 1.000001;
    }
    GET_TIME(end);
    double divide_time = TIME_DIFF(start, end);
    
    printf("整数加法: %.2f μs, %.2f Mops/sec\n", int_add_time, iterations / int_add_time);
    printf("浮点加法: %.2f μs, %.2f Mops/sec\n", float_add_time, iterations / float_add_time);
    printf("浮点乘法: %.2f μs, %.2f Mops/sec\n", multiply_time, iterations / multiply_time);
    printf("浮点除法: %.2f μs, %.2f Mops/sec\n", divide_time, iterations / divide_time);
}

// 研究2: 内存带宽测试
void benchmark_memory_bandwidth() {
    printf("\n=== 内存带宽测试 ===\n");
    
    struct timeval start, end;
    const size_t data_size = 100 * 1024 * 1024; // 100MB
    const size_t block_size = 64;
    
    char *buffer = (char *)malloc(data_size);
    if (!buffer) {
        printf("内存分配失败\n");
        return;
    }
    
    // 顺序读取测试
    GET_TIME(start);
    volatile char sum = 0;
    for (size_t i = 0; i < data_size; i += block_size) {
        sum += buffer[i];
    }
    GET_TIME(end);
    double sequential_read_time = TIME_DIFF(start, end);
    
    // 随机读取测试
    GET_TIME(start);
    sum = 0;
    for (size_t i = 0; i < data_size / block_size; i++) {
        size_t index = (size_t)(rand() % (data_size / block_size)) * block_size;
        sum += buffer[index];
    }
    GET_TIME(end);
    double random_read_time = TIME_DIFF(start, end);
    
    // 顺序写入测试
    GET_TIME(start);
    for (size_t i = 0; i < data_size; i += block_size) {
        buffer[i] = (char)(i % 256);
    }
    GET_TIME(end);
    double sequential_write_time = TIME_DIFF(start, end);
    
    double sequential_read_bw = (data_size / sequential_read_time) * 1000000.0 / (1024 * 1024);
    double random_read_bw = (data_size / random_read_time) * 1000000.0 / (1024 * 1024);
    double sequential_write_bw = (data_size / sequential_write_time) * 1000000.0 / (1024 * 1024);
    
    printf("顺序读取: %.2f MB/s\n", sequential_read_bw);
    printf("随机读取: %.2f MB/s\n", random_read_bw);
    printf("顺序写入: %.2f MB/s\n", sequential_write_bw);
    
    free(buffer);
}

// 研究3: 缓存性能测试
void benchmark_cache_performance() {
    printf("\n=== 缓存性能测试 ===\n");
    
    struct timeval start, end;
    const int array_size = 64 * 1024 * 1024; // 64MB
    int *array = (int *)malloc(array_size * sizeof(int));
    if (!array) {
        printf("内存分配失败\n");
        return;
    }
    
    // 初始化数组
    for (int i = 0; i < array_size; i++) {
        array[i] = i;
    }
    
    // L1缓存测试 (1KB)
    const int l1_size = 1024 / sizeof(int);
    GET_TIME(start);
    volatile int sum = 0;
    for (int k = 0; k < 1000; k++) {
        for (int i = 0; i < l1_size; i++) {
            sum += array[i];
        }
    }
    GET_TIME(end);
    double l1_time = TIME_DIFF(start, end);
    
    // L2缓存测试 (256KB)
    const int l2_size = 256 * 1024 / sizeof(int);
    GET_TIME(start);
    sum = 0;
    for (int k = 0; k < 100; k++) {
        for (int i = 0; i < l2_size; i++) {
            sum += array[i];
        }
    }
    GET_TIME(end);
    double l2_time = TIME_DIFF(start, end);
    
    // L3缓存测试 (8MB)
    const int l3_size = 8 * 1024 * 1024 / sizeof(int);
    GET_TIME(start);
    sum = 0;
    for (int k = 0; k < 10; k++) {
        for (int i = 0; i < l3_size; i++) {
            sum += array[i];
        }
    }
    GET_TIME(end);
    double l3_time = TIME_DIFF(start, end);
    
    // 主内存测试
    GET_TIME(start);
    sum = 0;
    for (int i = 0; i < array_size; i++) {
        sum += array[i];
    }
    GET_TIME(end);
    double memory_time = TIME_DIFF(start, end);
    
    printf("L1缓存: %.2f μs\n", l1_time);
    printf("L2缓存: %.2f μs\n", l2_time);
    printf("L3缓存: %.2f μs\n", l3_time);
    printf("主内存: %.2f μs\n", memory_time);
    
    free(array);
}

// 研究4: 分支预测测试
void benchmark_branch_prediction() {
    printf("\n=== 分支预测性能测试 ===\n");
    
    struct timeval start, end;
    const int size = 1000000;
    int *data = (int *)malloc(size * sizeof(int));
    if (!data) {
        printf("内存分配失败\n");
        return;
    }
    
    // 生成随机数据
    for (int i = 0; i < size; i++) {
        data[i] = rand() % 100;
    }
    
    // 随机分支（难以预测）
    GET_TIME(start);
    volatile int sum = 0;
    for (int i = 0; i < size; i++) {
        if (data[i] < 50) {
            sum += data[i];
        } else {
            sum -= data[i];
        }
    }
    GET_TIME(end);
    double random_branch_time = TIME_DIFF(start, end);
    
    // 可预测分支
    GET_TIME(start);
    sum = 0;
    for (int i = 0; i < size; i++) {
        if (i < size / 2) {
            sum += data[i];
        } else {
            sum -= data[i];
        }
    }
    GET_TIME(end);
    double predictable_branch_time = TIME_DIFF(start, end);
    
    printf("随机分支: %.2f μs\n", random_branch_time);
    printf("可预测分支: %.2f μs\n", predictable_branch_time);
    printf("性能提升: %.2fx\n", random_branch_time / predictable_branch_time);
    
    free(data);
}

// 研究5: 多线程性能测试
typedef struct {
    int *data;
    int start;
    int end;
    int result;
} ThreadData;

void *thread_worker(void *arg) {
    ThreadData *tdata = (ThreadData *)arg;
    int sum = 0;
    for (int i = tdata->start; i < tdata->end; i++) {
        sum += tdata->data[i];
    }
    tdata->result = sum;
    return NULL;
}

void benchmark_multithreading() {
    printf("\n=== 多线程性能测试 ===\n");
    
    struct timeval start, end;
    const int num_threads = 4;
    const int data_size = 10000000;
    
    int *data = (int *)malloc(data_size * sizeof(int));
    if (!data) {
        printf("内存分配失败\n");
        return;
    }
    
    for (int i = 0; i < data_size; i++) {
        data[i] = rand() % 100;
    }
    
    // 单线程版本
    GET_TIME(start);
    int total_sum = 0;
    for (int i = 0; i < data_size; i++) {
        total_sum += data[i];
    }
    GET_TIME(end);
    double single_thread_time = TIME_DIFF(start, end);
    
    // 多线程版本
    pthread_t threads[num_threads];
    ThreadData thread_data[num_threads];
    
    GET_TIME(start);
    int chunk_size = data_size / num_threads;
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].data = data;
        thread_data[i].start = i * chunk_size;
        thread_data[i].end = (i == num_threads - 1) ? data_size : (i + 1) * chunk_size;
        thread_data[i].result = 0;
        pthread_create(&threads[i], NULL, thread_worker, &thread_data[i]);
    }
    
    total_sum = 0;
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        total_sum += thread_data[i].result;
    }
    GET_TIME(end);
    double multi_thread_time = TIME_DIFF(start, end);
    
    printf("单线程: %.2f ms\n", single_thread_time / 1000);
    printf("多线程: %.2f ms\n", multi_thread_time / 1000);
    printf("加速比: %.2fx\n", single_thread_time / multi_thread_time);
    
    free(data);
}

// 研究6: 内存分配性能测试
void benchmark_memory_allocation() {
    printf("\n=== 内存分配性能测试 ===\n");
    
    struct timeval start, end;
    const int num_allocations = 100000;
    const int block_size = 1024;
    
    // malloc/free测试
    GET_TIME(start);
    void **pointers = malloc(num_allocations * sizeof(void *));
    for (int i = 0; i < num_allocations; i++) {
        pointers[i] = malloc(block_size);
    }
    for (int i = 0; i < num_allocations; i++) {
        free(pointers[i]);
    }
    free(pointers);
    GET_TIME(end);
    double malloc_time = TIME_DIFF(start, end);
    
    // 内存池测试（简化版）
    GET_TIME(start);
    char *pool = malloc(num_allocations * block_size);
    int *offsets = malloc(num_allocations * sizeof(int));
    for (int i = 0; i < num_allocations; i++) {
        offsets[i] = i * block_size;
    }
    free(offsets);
    free(pool);
    GET_TIME(end);
    double pool_time = TIME_DIFF(start, end);
    
    printf("malloc/free: %.2f ms\n", malloc_time / 1000);
    printf("内存池: %.2f ms\n", pool_time / 1000);
    printf("性能提升: %.2fx\n", malloc_time / pool_time);
}

// 研究7: 排序算法性能测试
int compare_ints(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);
}

void benchmark_sorting_algorithms() {
    printf("\n=== 排序算法性能测试 ===\n");
    
    struct timeval start, end;
    const int array_size = 100000;
    
    // 快速排序
    int *quick_data = (int *)malloc(array_size * sizeof(int));
    for (int i = 0; i < array_size; i++) {
        quick_data[i] = rand();
    }
    
    GET_TIME(start);
    qsort(quick_data, array_size, sizeof(int), compare_ints);
    GET_TIME(end);
    double quicksort_time = TIME_DIFF(start, end);
    
    // 归并排序（简化版冒泡排序）
    int *bubble_data = (int *)malloc(array_size * sizeof(int));
    for (int i = 0; i < array_size; i++) {
        bubble_data[i] = rand();
    }
    
    GET_TIME(start);
    for (int i = 0; i < array_size - 1; i++) {
        for (int j = 0; j < array_size - i - 1; j++) {
            if (bubble_data[j] > bubble_data[j + 1]) {
                int temp = bubble_data[j];
                bubble_data[j] = bubble_data[j + 1];
                bubble_data[j + 1] = temp;
            }
        }
    }
    GET_TIME(end);
    double bubblesort_time = TIME_DIFF(start, end);
    
    printf("快速排序: %.2f ms\n", quicksort_time / 1000);
    printf("冒泡排序: %.2f ms\n", bubblesort_time / 1000);
    printf("性能差异: %.2fx\n", bubblesort_time / quicksort_time);
    
    free(quick_data);
    free(bubble_data);
}

// 研究8: 系统调用开销测试
void benchmark_system_calls() {
    printf("\n=== 系统调用开销测试 ===\n");
    
    struct timeval start, end;
    const int num_calls = 100000;
    
    // getpid测试
    GET_TIME(start);
    for (int i = 0; i < num_calls; i++) {
        getpid();
    }
    GET_TIME(end);
    double getpid_time = TIME_DIFF(start, end);
    
    // time测试
    GET_TIME(start);
    for (int i = 0; i < num_calls; i++) {
        time(NULL);
    }
    GET_TIME(end);
    double time_time = TIME_DIFF(start, end);
    
    // malloc/free测试
    GET_TIME(start);
    for (int i = 0; i < 10000; i++) {
        void *ptr = malloc(1024);
        free(ptr);
    }
    GET_TIME(end);
    double malloc_time = TIME_DIFF(start, end);
    
    printf("getpid: %.2f ns/call\n", getpid_time * 1000 / num_calls);
    printf("time: %.2f ns/call\n", time_time * 1000 / num_calls);
    printf("malloc/free: %.2f μs/call\n", malloc_time / 10000);
}

// 研究9: 浮点运算精度测试
void benchmark_floating_point_precision() {
    printf("\n=== 浮点运算精度测试 ===\n");
    
    struct timeval start, end;
    const int iterations = 1000000;
    
    // 单精度浮点测试
    GET_TIME(start);
    volatile float fsum = 0.0f;
    for (int i = 0; i < iterations; i++) {
        fsum += 0.1f;
    }
    GET_TIME(end);
    double float_time = TIME_DIFF(start, end);
    
    // 双精度浮点测试
    GET_TIME(start);
    volatile double dsum = 0.0;
    for (int i = 0; i < iterations; i++) {
        dsum += 0.1;
    }
    GET_TIME(end);
    double double_time = TIME_DIFF(start, end);
    
    printf("单精度浮点: %.2f ms, 误差: %.8f\n", float_time / 1000, fabsf(fsum - iterations * 0.1f));
    printf("双精度浮点: %.2f ms, 误差: %.15f\n", double_time / 1000, fabs(dsum - iterations * 0.1));
    printf("性能差异: %.2fx\n", double_time / float_time);
}

// 性能结果结构体
typedef struct {
    const char *name;
    double time_us;
    double throughput;
    const char *unit;
} PerformanceResult;

// 主性能测试函数
int main() {
    printf("=== C语言性能基准测试深度研究 ===\n");
    printf("测试环境: Linux x86_64\n");
    printf("编译器: GCC with -O2 optimization\n\n");
    
    // 初始化随机种子
    srand(time(NULL));
    
    // 执行所有测试
    benchmark_cpu_arithmetic();
    benchmark_memory_bandwidth();
    benchmark_cache_performance();
    benchmark_branch_prediction();
    benchmark_multithreading();
    benchmark_memory_allocation();
    benchmark_sorting_algorithms();
    benchmark_system_calls();
    benchmark_floating_point_precision();
    
    printf("\n=== 性能优化建议 ===\n");
    printf("1. 优化内存访问模式，提高缓存命中率\n");
    printf("2. 减少分支预测失败，使用条件移动指令\n");
    printf("3. 合理使用多线程并行计算\n");
    printf("4. 减少系统调用和内存分配开销\n");
    printf("5. 选择合适的数据结构和算法\n");
    printf("6. 避免伪共享，使用缓存行对齐\n");
    printf("7. 预取数据，减少内存延迟\n");
    printf("8. 使用编译器优化选项 (-O2, -O3)\n");
    printf("9. 考虑CPU架构特性进行优化\n");
    printf("10. 使用性能分析工具定位瓶颈\n");
    
    printf("\n=== 测试完成 ===\n");
    
    return 0;
}