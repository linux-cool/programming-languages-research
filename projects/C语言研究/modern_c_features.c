/*
 * 现代C语言特性深度研究 (C11/C18/C23)
 * 探索C语言的现代化编程模式和新特性
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <threads.h>
#include <stdnoreturn.h>
#include <stdalign.h>
#include <assert.h>
#include <string.h>
#include <time.h>

// 研究1: C11静态断言和对齐
_Static_assert(sizeof(int) >= 4, "int must be at least 4 bytes");

typedef struct {
    alignas(64) char data[64];  // 64字节对齐
    int value;
} AlignedStruct;

// 研究2: C11泛型选择 (_Generic)
#define max(a, b) _Generic((a), \
    int: max_int, \
    float: max_float, \
    double: max_double, \
    default: max_int \
)(a, b)

int max_int(int a, int b) {
    return a > b ? a : b;
}

float max_float(float a, float b) {
    return a > b ? a : b;
}

double max_double(double a, double b) {
    return a > b ? a : b;
}

// 研究3: C99/C11柔性数组成员
typedef struct {
    size_t count;
    int data[];  // 柔性数组成员
} FlexibleArray;

FlexibleArray* create_flexible_array(size_t count) {
    FlexibleArray* arr = malloc(sizeof(FlexibleArray) + count * sizeof(int));
    if (arr) {
        arr->count = count;
        for (size_t i = 0; i < count; i++) {
            arr->data[i] = (int)i;
        }
    }
    return arr;
}

// 研究4: C99指定初始化器
typedef struct {
    int x, y, z;
    char name[32];
    bool active;
} Point3D;

void test_designated_initializers() {
    // 指定初始化器
    Point3D p1 = {
        .x = 10,
        .y = 20,
        .z = 30,
        .name = "Point1",
        .active = true
    };
    
    // 数组指定初始化
    int arr[10] = {
        [0] = 1,
        [5] = 6,
        [9] = 10
    };
    
    printf("Point: (%d, %d, %d) name=%s active=%s\n", 
           p1.x, p1.y, p1.z, p1.name, p1.active ? "true" : "false");
    
    printf("Array elements: ");
    for (int i = 0; i < 10; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
}

// 研究5: C11原子操作
typedef struct {
    atomic_int counter;
    atomic_bool running;
    atomic_uintptr_t data_ptr;
} AtomicCounter;

int atomic_thread_func(void* arg) {
    AtomicCounter* counter = (AtomicCounter*)arg;
    
    while (atomic_load(&counter->running)) {
        atomic_fetch_add(&counter->counter, 1);
        thrd_sleep(&(struct timespec){.tv_nsec = 1000000}, NULL); // 1ms
    }
    
    return 0;
}

void test_atomic_operations() {
    printf("\n=== 原子操作测试 ===\n");
    
    AtomicCounter counter = {
        .counter = ATOMIC_VAR_INIT(0),
        .running = ATOMIC_VAR_INIT(true),
        .data_ptr = ATOMIC_VAR_INIT(0)
    };
    
    thrd_t threads[4];
    
    // 创建多个线程
    for (int i = 0; i < 4; i++) {
        thrd_create(&threads[i], atomic_thread_func, &counter);
    }
    
    // 运行一段时间
    thrd_sleep(&(struct timespec){.tv_sec = 1}, NULL);
    
    // 停止所有线程
    atomic_store(&counter.running, false);
    
    // 等待线程结束
    for (int i = 0; i < 4; i++) {
        thrd_join(threads[i], NULL);
    }
    
    printf("最终计数器值: %d\n", atomic_load(&counter.counter));
}

// 研究6: C11线程本地存储
_Thread_local int thread_local_counter = 0;

int thread_local_func(void* arg) {
    int thread_id = *(int*)arg;
    
    for (int i = 0; i < 1000; i++) {
        thread_local_counter++;
    }
    
    printf("线程 %d 的本地计数器: %d\n", thread_id, thread_local_counter);
    return 0;
}

void test_thread_local_storage() {
    printf("\n=== 线程本地存储测试 ===\n");
    
    thrd_t threads[3];
    int thread_ids[3] = {1, 2, 3};
    
    for (int i = 0; i < 3; i++) {
        thrd_create(&threads[i], thread_local_func, &thread_ids[i]);
    }
    
    for (int i = 0; i < 3; i++) {
        thrd_join(threads[i], NULL);
    }
    
    printf("主线程的本地计数器: %d\n", thread_local_counter);
}

// 研究7: C11内存模型和内存序
typedef struct {
    atomic_int data;
    atomic_bool ready;
} SharedData;

int producer_thread(void* arg) {
    SharedData* shared = (SharedData*)arg;
    
    // 写入数据
    atomic_store_explicit(&shared->data, 42, memory_order_relaxed);
    
    // 设置就绪标志 (release语义)
    atomic_store_explicit(&shared->ready, true, memory_order_release);
    
    return 0;
}

int consumer_thread(void* arg) {
    SharedData* shared = (SharedData*)arg;
    
    // 等待就绪标志 (acquire语义)
    while (!atomic_load_explicit(&shared->ready, memory_order_acquire)) {
        thrd_yield();
    }
    
    // 读取数据
    int value = atomic_load_explicit(&shared->data, memory_order_relaxed);
    printf("消费者读取到数据: %d\n", value);
    
    return 0;
}

void test_memory_ordering() {
    printf("\n=== 内存序测试 ===\n");
    
    SharedData shared = {
        .data = ATOMIC_VAR_INIT(0),
        .ready = ATOMIC_VAR_INIT(false)
    };
    
    thrd_t producer, consumer;
    
    thrd_create(&consumer, consumer_thread, &shared);
    thrd_create(&producer, producer_thread, &shared);
    
    thrd_join(producer, NULL);
    thrd_join(consumer, NULL);
}

// 研究8: C11 noreturn函数
_Noreturn void fatal_error(const char* message) {
    fprintf(stderr, "致命错误: %s\n", message);
    exit(EXIT_FAILURE);
}

// 研究9: C23新特性预览 (如果编译器支持)
#if __STDC_VERSION__ >= 202311L
// C23的typeof操作符
#define SWAP(a, b) do { \
    typeof(a) temp = (a); \
    (a) = (b); \
    (b) = temp; \
} while(0)

// C23的二进制字面量
void test_c23_features() {
    printf("\n=== C23特性测试 ===\n");
    
    int binary_num = 0b1010101;  // 二进制字面量
    printf("二进制 0b1010101 = %d\n", binary_num);
    
    int a = 10, b = 20;
    printf("交换前: a=%d, b=%d\n", a, b);
    SWAP(a, b);
    printf("交换后: a=%d, b=%d\n", a, b);
}
#endif

// 研究10: 复合字面量和复合初始化
typedef struct {
    int x, y;
} Point2D;

void process_point(Point2D p) {
    printf("处理点: (%d, %d)\n", p.x, p.y);
}

void test_compound_literals() {
    printf("\n=== 复合字面量测试 ===\n");
    
    // 复合字面量
    process_point((Point2D){.x = 5, .y = 10});
    
    // 数组复合字面量
    int* arr = (int[]){1, 2, 3, 4, 5};
    printf("数组元素: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
}

// 研究11: VLA (可变长度数组)
void test_vla(int n) {
    printf("\n=== VLA测试 (n=%d) ===\n", n);
    
    int matrix[n][n];  // VLA
    
    // 初始化矩阵
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            matrix[i][j] = i * n + j;
        }
    }
    
    // 打印矩阵
    printf("VLA矩阵:\n");
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            printf("%3d ", matrix[i][j]);
        }
        printf("\n");
    }
}

// 主测试函数
int main() {
    printf("=== 现代C语言特性深度研究 ===\n");
    
    // 测试1: 对齐和静态断言
    printf("\n[测试1] 对齐和静态断言:\n");
    AlignedStruct aligned;
    printf("AlignedStruct对齐: %zu字节\n", alignof(AlignedStruct));
    printf("data字段对齐: %zu字节\n", alignof(aligned.data));
    
    // 测试2: 泛型选择
    printf("\n[测试2] 泛型选择:\n");
    printf("max(10, 20) = %d\n", max(10, 20));
    printf("max(3.14f, 2.71f) = %.2f\n", max(3.14f, 2.71f));
    printf("max(1.414, 1.732) = %.3f\n", max(1.414, 1.732));
    
    // 测试3: 柔性数组
    printf("\n[测试3] 柔性数组:\n");
    FlexibleArray* flex_arr = create_flexible_array(5);
    if (flex_arr) {
        printf("柔性数组: ");
        for (size_t i = 0; i < flex_arr->count; i++) {
            printf("%d ", flex_arr->data[i]);
        }
        printf("\n");
        free(flex_arr);
    }
    
    // 测试4: 指定初始化器
    test_designated_initializers();
    
    // 测试5: 原子操作
    test_atomic_operations();
    
    // 测试6: 线程本地存储
    test_thread_local_storage();
    
    // 测试7: 内存序
    test_memory_ordering();
    
    // 测试8: 复合字面量
    test_compound_literals();
    
    // 测试9: VLA
    test_vla(4);
    
#if __STDC_VERSION__ >= 202311L
    // 测试10: C23特性
    test_c23_features();
#endif
    
    printf("\n=== 研究结论 ===\n");
    printf("1. C11引入了强大的并发和原子操作支持\n");
    printf("2. 泛型选择提供了类型安全的多态性\n");
    printf("3. 静态断言和对齐控制增强了代码可靠性\n");
    printf("4. 线程本地存储简化了多线程编程\n");
    printf("5. 内存模型提供了精确的并发控制\n");
    printf("6. 现代C语言特性显著提升了开发效率\n");
    
    return 0;
}
