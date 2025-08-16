/*
 * C语言内存池管理深度研究
 * 高效内存分配与碎片优化策略
 */
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>

// 研究1: 固定大小内存池实现
#define POOL_BLOCK_SIZE 64
#define POOL_MAX_BLOCKS 1024

typedef struct MemoryBlock {
    struct MemoryBlock* next;
    char data[POOL_BLOCK_SIZE - sizeof(struct MemoryBlock*)];
} MemoryBlock;

typedef struct MemoryPool {
    MemoryBlock* free_list;
    MemoryBlock* blocks;
    size_t block_size;
    size_t total_blocks;
    size_t used_blocks;
    size_t free_blocks;
} MemoryPool;

// 初始化内存池
MemoryPool* create_memory_pool(size_t block_size, size_t total_blocks) {
    MemoryPool* pool = (MemoryPool*)malloc(sizeof(MemoryPool));
    if (!pool) return NULL;
    
    pool->block_size = block_size;
    pool->total_blocks = total_blocks;
    pool->used_blocks = 0;
    pool->free_blocks = total_blocks;
    
    // 分配连续内存块
    pool->blocks = (MemoryBlock*)malloc(total_blocks * sizeof(MemoryBlock));
    if (!pool->blocks) {
        free(pool);
        return NULL;
    }
    
    // 初始化空闲链表
    pool->free_list = pool->blocks;
    for (size_t i = 0; i < total_blocks - 1; i++) {
        pool->blocks[i].next = &pool->blocks[i + 1];
    }
    pool->blocks[total_blocks - 1].next = NULL;
    
    return pool;
}

// 从内存池分配内存
void* pool_alloc(MemoryPool* pool) {
    if (!pool || !pool->free_list) {
        return NULL;
    }
    
    MemoryBlock* block = pool->free_list;
    pool->free_list = block->next;
    
    pool->used_blocks++;
    pool->free_blocks--;
    
    return block;
}

// 释放内存回内存池
void pool_free(MemoryPool* pool, void* ptr) {
    if (!pool || !ptr) return;
    
    MemoryBlock* block = (MemoryBlock*)ptr;
    block->next = pool->free_list;
    pool->free_list = block;
    
    pool->used_blocks--;
    pool->free_blocks++;
}

// 销毁内存池
void destroy_memory_pool(MemoryPool* pool) {
    if (pool) {
        free(pool->blocks);
        free(pool);
    }
}

// 研究2: 动态大小内存池（slab分配器）
#define SLAB_SIZE 4096
#define MIN_ALLOC_SIZE 16
#define MAX_ALLOC_SIZE 2048

typedef struct Slab {
    struct Slab* next;
    char* memory;
    size_t size;
    size_t used;
} Slab;

typedef struct SlabAllocator {
    Slab* slabs;
    size_t slab_size;
    size_t total_allocated;
    size_t total_used;
} SlabAllocator;

// 创建slab分配器
SlabAllocator* create_slab_allocator(size_t slab_size) {
    SlabAllocator* allocator = (SlabAllocator*)malloc(sizeof(SlabAllocator));
    if (!allocator) return NULL;
    
    allocator->slabs = NULL;
    allocator->slab_size = slab_size;
    allocator->total_allocated = 0;
    allocator->total_used = 0;
    
    return allocator;
}

// 分配新的slab
static Slab* allocate_slab(SlabAllocator* allocator) {
    Slab* slab = (Slab*)malloc(sizeof(Slab));
    if (!slab) return NULL;
    
    slab->memory = (char*)malloc(allocator->slab_size);
    if (!slab->memory) {
        free(slab);
        return NULL;
    }
    
    slab->size = allocator->slab_size;
    slab->used = 0;
    slab->next = allocator->slabs;
    allocator->slabs = slab;
    allocator->total_allocated += allocator->slab_size;
    
    return slab;
}

// 从slab分配器分配内存
void* slab_alloc(SlabAllocator* allocator, size_t size) {
    if (!allocator || size == 0) return NULL;
    
    // 对齐到16字节
    size = (size + 15) & ~15;
    
    Slab* slab = allocator->slabs;
    while (slab) {
        if (slab->size - slab->used >= size) {
            void* ptr = slab->memory + slab->used;
            slab->used += size;
            allocator->total_used += size;
            return ptr;
        }
        slab = slab->next;
    }
    
    // 需要新的slab
    slab = allocate_slab(allocator);
    if (!slab) return NULL;
    
    void* ptr = slab->memory;
    slab->used = size;
    allocator->total_used += size;
    return ptr;
}

// 销毁slab分配器
void destroy_slab_allocator(SlabAllocator* allocator) {
    if (!allocator) return;
    
    Slab* slab = allocator->slabs;
    while (slab) {
        Slab* next = slab->next;
        free(slab->memory);
        free(slab);
        slab = next;
    }
    free(allocator);
}

// 研究3: 内存泄漏检测器
#define MAX_ALLOCS 10000

typedef struct AllocationInfo {
    void* ptr;
    size_t size;
    const char* file;
    int line;
    time_t timestamp;
} AllocationInfo;

typedef struct MemoryTracker {
    AllocationInfo allocations[MAX_ALLOCS];
    size_t count;
    size_t total_allocated;
    size_t total_freed;
    bool enabled;
} MemoryTracker;

static MemoryTracker global_tracker = {0};

// 启用内存跟踪
void enable_memory_tracking() {
    global_tracker.enabled = true;
    global_tracker.count = 0;
    global_tracker.total_allocated = 0;
    global_tracker.total_freed = 0;
}

// 添加分配记录
void track_allocation(void* ptr, size_t size, const char* file, int line) {
    if (!global_tracker.enabled || !ptr) return;
    
    if (global_tracker.count < MAX_ALLOCS) {
        AllocationInfo* info = &global_tracker.allocations[global_tracker.count++];
        info->ptr = ptr;
        info->size = size;
        info->file = file;
        info->line = line;
        info->timestamp = time(NULL);
        global_tracker.total_allocated += size;
    }
}

// 移除分配记录
void track_free(void* ptr) {
    if (!global_tracker.enabled || !ptr) return;
    
    for (size_t i = 0; i < global_tracker.count; i++) {
        if (global_tracker.allocations[i].ptr == ptr) {
            global_tracker.total_freed += global_tracker.allocations[i].size;
            global_tracker.allocations[i] = global_tracker.allocations[global_tracker.count - 1];
            global_tracker.count--;
            return;
        }
    }
}

// 打印内存泄漏报告
void print_memory_leaks() {
    if (!global_tracker.enabled) return;
    
    printf("\n=== 内存泄漏检测报告 ===\n");
    printf("总分配: %zu bytes\n", global_tracker.total_allocated);
    printf("总释放: %zu bytes\n", global_tracker.total_freed);
    printf("泄漏: %zu bytes\n", global_tracker.total_allocated - global_tracker.total_freed);
    
    if (global_tracker.count > 0) {
        printf("\n未释放的内存块:\n");
        for (size_t i = 0; i < global_tracker.count; i++) {
            AllocationInfo* info = &global_tracker.allocations[i];
            printf("  地址: %p, 大小: %zu, 文件: %s:%d, 时间: %s", 
                   info->ptr, info->size, info->file, info->line, ctime(&info->timestamp));
        }
    } else {
        printf("没有检测到内存泄漏！\n");
    }
}

// 包装的分配和释放函数
#define tracked_malloc(size) ({ \
    void* ptr = malloc(size); \
    track_allocation(ptr, size, __FILE__, __LINE__); \
    ptr; \
})

#define tracked_free(ptr) ({ \
    track_free(ptr); \
    free(ptr); \
})

// 研究4: 内存碎片分析与优化
#define MAX_FRAGMENT_BLOCKS 1000

typedef struct MemoryFragment {
    void* start;
    size_t size;
    bool is_free;
} MemoryFragment;

typedef struct FragmentAnalyzer {
    MemoryFragment fragments[MAX_FRAGMENT_BLOCKS];
    size_t count;
    size_t total_memory;
} FragmentAnalyzer;

// 初始化碎片分析器
void init_fragment_analyzer(FragmentAnalyzer* analyzer, size_t total_memory) {
    analyzer->count = 0;
    analyzer->total_memory = total_memory;
}

// 添加内存块到分析器
void add_fragment(FragmentAnalyzer* analyzer, void* start, size_t size, bool is_free) {
    if (analyzer->count < MAX_FRAGMENT_BLOCKS) {
        analyzer->fragments[analyzer->count].start = start;
        analyzer->fragments[analyzer->count].size = size;
        analyzer->fragments[analyzer->count].is_free = is_free;
        analyzer->count++;
    }
}

// 计算内存碎片率
double calculate_fragmentation_rate(FragmentAnalyzer* analyzer) {
    if (analyzer->count == 0) return 0.0;
    
    size_t total_free = 0;
    size_t largest_free = 0;
    
    for (size_t i = 0; i < analyzer->count; i++) {
        if (analyzer->fragments[i].is_free) {
            total_free += analyzer->fragments[i].size;
            if (analyzer->fragments[i].size > largest_free) {
                largest_free = analyzer->fragments[i].size;
            }
        }
    }
    
    if (total_free == 0) return 0.0;
    
    return 1.0 - (double)largest_free / total_free;
}

// 打印碎片分析报告
void print_fragment_analysis(FragmentAnalyzer* analyzer) {
    printf("\n=== 内存碎片分析报告 ===\n");
    printf("总内存块数: %zu\n", analyzer->count);
    printf("总内存大小: %zu bytes\n", analyzer->total_memory);
    
    size_t total_free = 0;
    size_t largest_free = 0;
    void* largest_free_start = NULL;
    
    for (size_t i = 0; i < analyzer->count; i++) {
        MemoryFragment* frag = &analyzer->fragments[i];
        if (frag->is_free) {
            total_free += frag->size;
            if (frag->size > largest_free) {
                largest_free = frag->size;
                largest_free_start = frag->start;
            }
        }
    }
    
    double fragmentation_rate = calculate_fragmentation_rate(analyzer);
    
    printf("空闲内存总数: %zu bytes\n", total_free);
    printf("最大空闲块: %zu bytes at %p\n", largest_free, largest_free_start);
    printf("碎片率: %.2f%%\n", fragmentation_rate * 100);
}

// 研究5: 内存池性能基准测试
void benchmark_memory_pools() {
    const int ITERATIONS = 100000;
    clock_t start, end;
    
    printf("\n=== 内存池性能基准测试 ===\n");
    
    // 测试1: 标准malloc/free
    start = clock();
    for (int i = 0; i < ITERATIONS; i++) {
        void* ptr = malloc(64);
        if (ptr) free(ptr);
    }
    end = clock();
    double malloc_time = (double)(end - start) / CLOCKS_PER_SEC;
    
    // 测试2: 固定大小内存池
    MemoryPool* pool = create_memory_pool(64, ITERATIONS);
    start = clock();
    for (int i = 0; i < ITERATIONS; i++) {
        void* ptr = pool_alloc(pool);
        if (ptr) pool_free(pool, ptr);
    }
    end = clock();
    double pool_time = (double)(end - start) / CLOCKS_PER_SEC;
    destroy_memory_pool(pool);
    
    // 测试3: slab分配器
    SlabAllocator* slab = create_slab_allocator(4096);
    start = clock();
    for (int i = 0; i < ITERATIONS; i++) {
        void* ptr = slab_alloc(slab, 64);
        if (ptr) {} // slab分配器不支持单独释放
    }
    end = clock();
    double slab_time = (double)(end - start) / CLOCKS_PER_SEC;
    destroy_slab_allocator(slab);
    
    printf("标准malloc/free: %.4f 秒\n", malloc_time);
    printf("固定大小内存池: %.4f 秒\n", pool_time);
    printf("slab分配器: %.4f 秒\n", slab_time);
    printf("性能提升: %.2fx\n", malloc_time / pool_time);
}

// 研究6: 内存对齐优化
void* aligned_alloc_custom(size_t size, size_t alignment) {
    if (alignment < sizeof(void*)) {
        alignment = sizeof(void*);
    }
    
    // 分配额外空间用于对齐和存储原始指针
    size_t total_size = size + alignment + sizeof(void*);
    void* raw_ptr = malloc(total_size);
    if (!raw_ptr) return NULL;
    
    // 计算对齐地址
    uintptr_t raw_addr = (uintptr_t)raw_ptr + sizeof(void*);
    uintptr_t aligned_addr = (raw_addr + alignment - 1) & ~(alignment - 1);
    
    // 存储原始指针
    void** original_ptr = (void**)(aligned_addr - sizeof(void*));
    *original_ptr = raw_ptr;
    
    return (void*)aligned_addr;
}

void aligned_free_custom(void* aligned_ptr) {
    if (aligned_ptr) {
        void** original_ptr = (void**)((uintptr_t)aligned_ptr - sizeof(void*));
        free(*original_ptr);
    }
}

// 主测试函数
int main() {
    printf("=== C语言内存池管理深度研究 ===\n");
    
    // 测试1: 固定大小内存池
    printf("\n[测试1] 固定大小内存池:\n");
    MemoryPool* pool = create_memory_pool(64, 100);
    if (pool) {
        printf("内存池创建成功\n");
        printf("块大小: %zu bytes\n", pool->block_size);
        printf("总块数: %zu\n", pool->total_blocks);
        printf("空闲块数: %zu\n", pool->free_blocks);
        
        // 分配测试
        void* ptr1 = pool_alloc(pool);
        void* ptr2 = pool_alloc(pool);
        printf("分配后空闲块数: %zu\n", pool->free_blocks);
        
        pool_free(pool, ptr1);
        pool_free(pool, ptr2);
        printf("释放后空闲块数: %zu\n", pool->free_blocks);
        
        destroy_memory_pool(pool);
    }
    
    // 测试2: 内存泄漏检测
    printf("\n[测试2] 内存泄漏检测:\n");
    enable_memory_tracking();
    
    int* leak1 = (int*)tracked_malloc(100);
    int* ok1 = (int*)tracked_malloc(200);
    tracked_free(ok1); // 正确释放
    // leak1 有意不释放用于测试
    
    print_memory_leaks();
    
    // 测试3: 内存碎片分析
    printf("\n[测试3] 内存碎片分析:\n");
    FragmentAnalyzer analyzer;
    init_fragment_analyzer(&analyzer, 1024);
    
    // 模拟内存分配和释放模式
    add_fragment(&analyzer, (void*)0x1000, 256, false);
    add_fragment(&analyzer, (void*)0x1100, 128, true);
    add_fragment(&analyzer, (void*)0x1180, 256, false);
    add_fragment(&analyzer, (void*)0x1280, 256, true);
    add_fragment(&analyzer, (void*)0x1380, 128, false);
    
    print_fragment_analysis(&analyzer);
    
    // 测试4: 性能基准测试
    benchmark_memory_pools();
    
    // 测试5: 内存对齐测试
    printf("\n[测试5] 内存对齐优化:\n");
    void* aligned = aligned_alloc_custom(128, 64);
    printf("对齐分配地址: %p\n", aligned);
    printf("地址对齐: %s\n", ((uintptr_t)aligned % 64 == 0) ? "成功" : "失败");
    aligned_free_custom(aligned);
    
    printf("\n=== 研究结论 ===\n");
    printf("1. 内存池显著减少分配开销和内存碎片\n");
    printf("2. 固定大小内存池适用于对象池场景\n");
    printf("3. slab分配器适合不同大小的内存需求\n");
    printf("4. 内存泄漏检测工具提高程序可靠性\n");
    printf("5. 内存对齐优化提升缓存性能\n");
    
    return 0;
}