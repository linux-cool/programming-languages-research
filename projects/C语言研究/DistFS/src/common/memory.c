/*
 * DistFS 内存管理模块
 * 提供高性能内存池和内存统计功能
 */

#define _GNU_SOURCE
#include "distfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <unistd.h>

/* 内存块头部结构 */
typedef struct memory_block {
    size_t size;
    uint32_t magic;
    struct memory_block *next;
    struct memory_block *prev;
} memory_block_t;

/* 内存池结构 */
typedef struct memory_pool {
    size_t block_size;
    size_t total_blocks;
    size_t free_blocks;
    memory_block_t *free_list;
    void *memory_region;
    size_t region_size;
    pthread_mutex_t mutex;
    struct memory_pool *next;
} memory_pool_t;

/* 内存统计结构 */
typedef struct memory_stats {
    uint64_t total_allocated;
    uint64_t total_freed;
    uint64_t current_usage;
    uint64_t peak_usage;
    uint64_t allocation_count;
    uint64_t free_count;
    uint64_t pool_hits;
    uint64_t pool_misses;
} memory_stats_t;

/* 内存管理器结构 */
struct distfs_memory_manager {
    memory_pool_t *pools;
    memory_stats_t stats;
    pthread_mutex_t global_mutex;
    bool initialized;
    size_t page_size;
};

/* 全局内存管理器 */
static distfs_memory_manager_t *g_memory_manager = NULL;

/* 内存块魔数 */
#define MEMORY_MAGIC 0xDEADBEEF
#define MEMORY_FREED_MAGIC 0xFEEDFACE

/* 默认内存池大小 */
static const size_t default_pool_sizes[] = {
    32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536
};
#define DEFAULT_POOL_COUNT (sizeof(default_pool_sizes) / sizeof(default_pool_sizes[0]))

/* 获取对齐后的大小 */
static size_t align_size(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

/* 创建内存池 */
static memory_pool_t* create_memory_pool(size_t block_size, size_t block_count) {
    memory_pool_t *pool = malloc(sizeof(memory_pool_t));
    if (!pool) return NULL;
    
    memset(pool, 0, sizeof(memory_pool_t));
    
    pool->block_size = align_size(block_size + sizeof(memory_block_t), 16);
    pool->total_blocks = block_count;
    pool->free_blocks = block_count;
    pool->region_size = pool->block_size * block_count;
    
    // 分配内存区域
    pool->memory_region = mmap(NULL, pool->region_size, 
                              PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pool->memory_region == MAP_FAILED) {
        free(pool);
        return NULL;
    }
    
    // 初始化互斥锁
    if (pthread_mutex_init(&pool->mutex, NULL) != 0) {
        munmap(pool->memory_region, pool->region_size);
        free(pool);
        return NULL;
    }
    
    // 初始化空闲链表
    char *ptr = (char*)pool->memory_region;
    memory_block_t *prev = NULL;
    
    for (size_t i = 0; i < block_count; i++) {
        memory_block_t *block = (memory_block_t*)ptr;
        block->size = block_size;
        block->magic = MEMORY_FREED_MAGIC;
        block->next = NULL;
        block->prev = prev;
        
        if (prev) {
            prev->next = block;
        } else {
            pool->free_list = block;
        }
        
        prev = block;
        ptr += pool->block_size;
    }
    
    return pool;
}

/* 销毁内存池 */
static void destroy_memory_pool(memory_pool_t *pool) {
    if (!pool) return;
    
    pthread_mutex_destroy(&pool->mutex);
    munmap(pool->memory_region, pool->region_size);
    free(pool);
}

/* 从内存池分配内存 */
static void* pool_alloc(memory_pool_t *pool, size_t size) {
    if (!pool || size > pool->block_size - sizeof(memory_block_t)) {
        return NULL;
    }
    
    pthread_mutex_lock(&pool->mutex);
    
    if (pool->free_blocks == 0 || !pool->free_list) {
        pthread_mutex_unlock(&pool->mutex);
        return NULL;
    }
    
    memory_block_t *block = pool->free_list;
    pool->free_list = block->next;
    if (pool->free_list) {
        pool->free_list->prev = NULL;
    }
    
    pool->free_blocks--;
    
    block->size = size;
    block->magic = MEMORY_MAGIC;
    block->next = NULL;
    block->prev = NULL;
    
    pthread_mutex_unlock(&pool->mutex);
    
    return (char*)block + sizeof(memory_block_t);
}

/* 释放内存到内存池 */
static bool pool_free(memory_pool_t *pool, void *ptr) {
    if (!pool || !ptr) return false;
    
    memory_block_t *block = (memory_block_t*)((char*)ptr - sizeof(memory_block_t));
    
    // 检查内存块是否属于此池
    if ((char*)block < (char*)pool->memory_region ||
        (char*)block >= (char*)pool->memory_region + pool->region_size) {
        return false;
    }
    
    // 检查魔数
    if (block->magic != MEMORY_MAGIC) {
        return false;
    }
    
    pthread_mutex_lock(&pool->mutex);
    
    // 标记为已释放
    block->magic = MEMORY_FREED_MAGIC;
    block->next = pool->free_list;
    block->prev = NULL;
    
    if (pool->free_list) {
        pool->free_list->prev = block;
    }
    pool->free_list = block;
    pool->free_blocks++;
    
    pthread_mutex_unlock(&pool->mutex);
    
    return true;
}

/* 初始化内存管理器 */
int distfs_memory_init(void) {
    if (g_memory_manager) {
        return DISTFS_ERROR_ALREADY_INITIALIZED;
    }
    
    g_memory_manager = malloc(sizeof(distfs_memory_manager_t));
    if (!g_memory_manager) {
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    memset(g_memory_manager, 0, sizeof(distfs_memory_manager_t));
    
    g_memory_manager->page_size = getpagesize();
    
    if (pthread_mutex_init(&g_memory_manager->global_mutex, NULL) != 0) {
        free(g_memory_manager);
        g_memory_manager = NULL;
        return DISTFS_ERROR_SYSTEM_ERROR;
    }
    
    // 创建默认内存池
    memory_pool_t *prev_pool = NULL;
    for (size_t i = 0; i < DEFAULT_POOL_COUNT; i++) {
        size_t block_count = 1024; // 每个池1024个块
        memory_pool_t *pool = create_memory_pool(default_pool_sizes[i], block_count);
        if (!pool) {
            distfs_memory_cleanup();
            return DISTFS_ERROR_NO_MEMORY;
        }
        
        if (prev_pool) {
            prev_pool->next = pool;
        } else {
            g_memory_manager->pools = pool;
        }
        prev_pool = pool;
    }
    
    g_memory_manager->initialized = true;
    return DISTFS_SUCCESS;
}

/* 清理内存管理器 */
void distfs_memory_cleanup(void) {
    if (!g_memory_manager) return;
    
    pthread_mutex_lock(&g_memory_manager->global_mutex);
    
    memory_pool_t *pool = g_memory_manager->pools;
    while (pool) {
        memory_pool_t *next = pool->next;
        destroy_memory_pool(pool);
        pool = next;
    }
    
    g_memory_manager->initialized = false;
    
    pthread_mutex_unlock(&g_memory_manager->global_mutex);
    pthread_mutex_destroy(&g_memory_manager->global_mutex);
    
    free(g_memory_manager);
    g_memory_manager = NULL;
}

/* 分配内存 */
void* distfs_malloc(size_t size) {
    if (!g_memory_manager || !g_memory_manager->initialized || size == 0) {
        return NULL;
    }
    
    pthread_mutex_lock(&g_memory_manager->global_mutex);
    
    // 尝试从内存池分配
    memory_pool_t *pool = g_memory_manager->pools;
    while (pool) {
        if (size <= pool->block_size - sizeof(memory_block_t)) {
            void *ptr = pool_alloc(pool, size);
            if (ptr) {
                g_memory_manager->stats.pool_hits++;
                g_memory_manager->stats.total_allocated += size;
                g_memory_manager->stats.current_usage += size;
                g_memory_manager->stats.allocation_count++;
                
                if (g_memory_manager->stats.current_usage > g_memory_manager->stats.peak_usage) {
                    g_memory_manager->stats.peak_usage = g_memory_manager->stats.current_usage;
                }
                
                pthread_mutex_unlock(&g_memory_manager->global_mutex);
                return ptr;
            }
        }
        pool = pool->next;
    }
    
    // 内存池分配失败，使用系统malloc
    g_memory_manager->stats.pool_misses++;
    
    size_t alloc_size = size + sizeof(memory_block_t);
    memory_block_t *block = malloc(alloc_size);
    if (!block) {
        pthread_mutex_unlock(&g_memory_manager->global_mutex);
        return NULL;
    }
    
    block->size = size;
    block->magic = MEMORY_MAGIC;
    block->next = NULL;
    block->prev = NULL;
    
    g_memory_manager->stats.total_allocated += size;
    g_memory_manager->stats.current_usage += size;
    g_memory_manager->stats.allocation_count++;
    
    if (g_memory_manager->stats.current_usage > g_memory_manager->stats.peak_usage) {
        g_memory_manager->stats.peak_usage = g_memory_manager->stats.current_usage;
    }
    
    pthread_mutex_unlock(&g_memory_manager->global_mutex);
    
    return (char*)block + sizeof(memory_block_t);
}

/* 释放内存 */
void distfs_free(void *ptr) {
    if (!ptr || !g_memory_manager || !g_memory_manager->initialized) {
        return;
    }
    
    memory_block_t *block = (memory_block_t*)((char*)ptr - sizeof(memory_block_t));
    
    // 检查魔数
    if (block->magic != MEMORY_MAGIC) {
        return;
    }
    
    pthread_mutex_lock(&g_memory_manager->global_mutex);
    
    // 尝试释放到内存池
    memory_pool_t *pool = g_memory_manager->pools;
    while (pool) {
        if (pool_free(pool, ptr)) {
            g_memory_manager->stats.total_freed += block->size;
            g_memory_manager->stats.current_usage -= block->size;
            g_memory_manager->stats.free_count++;
            pthread_mutex_unlock(&g_memory_manager->global_mutex);
            return;
        }
        pool = pool->next;
    }
    
    // 不属于内存池，使用系统free
    g_memory_manager->stats.total_freed += block->size;
    g_memory_manager->stats.current_usage -= block->size;
    g_memory_manager->stats.free_count++;
    
    block->magic = MEMORY_FREED_MAGIC;
    
    pthread_mutex_unlock(&g_memory_manager->global_mutex);
    
    free(block);
}

/* 重新分配内存 */
void* distfs_realloc(void *ptr, size_t new_size) {
    if (!ptr) {
        return distfs_malloc(new_size);
    }
    
    if (new_size == 0) {
        distfs_free(ptr);
        return NULL;
    }
    
    memory_block_t *block = (memory_block_t*)((char*)ptr - sizeof(memory_block_t));
    if (block->magic != MEMORY_MAGIC) {
        return NULL;
    }
    
    size_t old_size = block->size;
    if (new_size <= old_size) {
        return ptr; // 不需要重新分配
    }
    
    void *new_ptr = distfs_malloc(new_size);
    if (!new_ptr) {
        return NULL;
    }
    
    memcpy(new_ptr, ptr, old_size);
    distfs_free(ptr);
    
    return new_ptr;
}

/* 分配并清零内存 */
void* distfs_calloc(size_t count, size_t size) {
    size_t total_size = count * size;
    if (total_size / count != size) {
        return NULL; // 溢出检查
    }
    
    void *ptr = distfs_malloc(total_size);
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    
    return ptr;
}

/* 获取内存统计信息 */
int distfs_memory_get_stats(distfs_memory_stats_t *stats) {
    if (!stats || !g_memory_manager) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_memory_manager->global_mutex);
    
    stats->total_allocated = g_memory_manager->stats.total_allocated;
    stats->total_freed = g_memory_manager->stats.total_freed;
    stats->current_usage = g_memory_manager->stats.current_usage;
    stats->peak_usage = g_memory_manager->stats.peak_usage;
    stats->allocation_count = g_memory_manager->stats.allocation_count;
    stats->free_count = g_memory_manager->stats.free_count;
    stats->pool_hits = g_memory_manager->stats.pool_hits;
    stats->pool_misses = g_memory_manager->stats.pool_misses;
    
    pthread_mutex_unlock(&g_memory_manager->global_mutex);
    
    return DISTFS_SUCCESS;
}
