/*
 * DistFS 块管理器实现
 * 管理数据块的分配、回收和元数据
 */

#define _GNU_SOURCE
#include "distfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdbool.h>

/* 块分配位图 */
typedef struct block_bitmap {
    uint64_t *bits;
    uint64_t total_blocks;
    uint64_t free_blocks;
    pthread_mutex_t mutex;
} block_bitmap_t;

/* 块元数据 */
typedef struct block_metadata {
    uint64_t block_id;
    uint64_t file_id;
    uint64_t offset;
    uint32_t size;
    uint32_t checksum;
    time_t created_time;
    time_t modified_time;
    uint16_t ref_count;
    uint8_t status; // 0: free, 1: allocated, 2: dirty
} block_metadata_t;

/* 块管理器结构 */
struct distfs_block_manager {
    char data_dir[DISTFS_MAX_PATH_LEN];
    uint64_t block_size;
    uint64_t total_blocks;
    uint64_t free_blocks;
    
    /* 位图管理 */
    block_bitmap_t bitmap;
    
    /* 元数据管理 */
    block_metadata_t *metadata;
    char metadata_file[DISTFS_MAX_PATH_LEN];
    
    /* 同步控制 */
    pthread_mutex_t mutex;
    pthread_rwlock_t metadata_lock;
    
    /* 统计信息 */
    uint64_t allocations;
    uint64_t deallocations;
    uint64_t reads;
    uint64_t writes;
    
    bool initialized;
};

/* 全局块管理器实例 */
static distfs_block_manager_t *g_block_manager = NULL;

/* 设置位图中的位 */
static void set_bit(uint64_t *bitmap, uint64_t bit) {
    uint64_t word = bit / 64;
    uint64_t offset = bit % 64;
    bitmap[word] |= (1ULL << offset);
}

/* 清除位图中的位 */
static void clear_bit(uint64_t *bitmap, uint64_t bit) {
    uint64_t word = bit / 64;
    uint64_t offset = bit % 64;
    bitmap[word] &= ~(1ULL << offset);
}

/* 测试位图中的位 */
static bool test_bit(uint64_t *bitmap, uint64_t bit) {
    uint64_t word = bit / 64;
    uint64_t offset = bit % 64;
    return (bitmap[word] & (1ULL << offset)) != 0;
}

/* 查找第一个空闲位 */
static uint64_t find_first_free_bit(uint64_t *bitmap, uint64_t total_bits) {
    uint64_t words = (total_bits + 63) / 64;
    
    for (uint64_t i = 0; i < words; i++) {
        if (bitmap[i] != UINT64_MAX) {
            for (int j = 0; j < 64; j++) {
                uint64_t bit = i * 64 + j;
                if (bit >= total_bits) break;
                
                if (!test_bit(bitmap, bit)) {
                    return bit;
                }
            }
        }
    }
    
    return UINT64_MAX; // 没有找到空闲位
}

/* 初始化位图 */
static int init_bitmap(block_bitmap_t *bitmap, uint64_t total_blocks) {
    uint64_t words = (total_blocks + 63) / 64;
    
    bitmap->bits = calloc(words, sizeof(uint64_t));
    if (!bitmap->bits) {
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    bitmap->total_blocks = total_blocks;
    bitmap->free_blocks = total_blocks;
    
    if (pthread_mutex_init(&bitmap->mutex, NULL) != 0) {
        free(bitmap->bits);
        return DISTFS_ERROR_SYSTEM_ERROR;
    }
    
    return DISTFS_SUCCESS;
}

/* 清理位图 */
static void cleanup_bitmap(block_bitmap_t *bitmap) {
    if (bitmap->bits) {
        free(bitmap->bits);
        bitmap->bits = NULL;
    }
    pthread_mutex_destroy(&bitmap->mutex);
}

/* 保存元数据到文件 */
static int save_metadata(distfs_block_manager_t *manager) {
    FILE *fp = fopen(manager->metadata_file, "wb");
    if (!fp) {
        DISTFS_LOG_ERROR("Failed to open metadata file %s: %s", 
                        manager->metadata_file, strerror(errno));
        return DISTFS_ERROR_FILE_OPEN_FAILED;
    }
    
    // 写入头部信息
    struct {
        uint64_t magic;
        uint64_t version;
        uint64_t block_size;
        uint64_t total_blocks;
        uint64_t free_blocks;
    } header = {
        .magic = 0x44495354424C4B53, // "DISTBLKS"
        .version = 1,
        .block_size = manager->block_size,
        .total_blocks = manager->total_blocks,
        .free_blocks = manager->free_blocks
    };
    
    if (fwrite(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return DISTFS_ERROR_SYSTEM_ERROR;
    }
    
    // 写入位图
    uint64_t bitmap_words = (manager->total_blocks + 63) / 64;
    if (fwrite(manager->bitmap.bits, sizeof(uint64_t), bitmap_words, fp) != bitmap_words) {
        fclose(fp);
        return DISTFS_ERROR_SYSTEM_ERROR;
    }
    
    // 写入元数据
    if (fwrite(manager->metadata, sizeof(block_metadata_t), manager->total_blocks, fp) != manager->total_blocks) {
        fclose(fp);
        return DISTFS_ERROR_SYSTEM_ERROR;
    }
    
    fclose(fp);
    return DISTFS_SUCCESS;
}

/* 从文件加载元数据 */
static int load_metadata(distfs_block_manager_t *manager) {
    FILE *fp = fopen(manager->metadata_file, "rb");
    if (!fp) {
        if (errno == ENOENT) {
            // 文件不存在，创建新的元数据
            return DISTFS_SUCCESS;
        }
        DISTFS_LOG_ERROR("Failed to open metadata file %s: %s", 
                        manager->metadata_file, strerror(errno));
        return DISTFS_ERROR_FILE_OPEN_FAILED;
    }
    
    // 读取头部信息
    struct {
        uint64_t magic;
        uint64_t version;
        uint64_t block_size;
        uint64_t total_blocks;
        uint64_t free_blocks;
    } header;
    
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return DISTFS_ERROR_SYSTEM_ERROR;
    }
    
    // 验证魔数和版本
    if (header.magic != 0x44495354424C4B53 || header.version != 1) {
        fclose(fp);
        DISTFS_LOG_ERROR("Invalid metadata file format");
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    // 验证块大小和总块数
    if (header.block_size != manager->block_size || header.total_blocks != manager->total_blocks) {
        fclose(fp);
        DISTFS_LOG_ERROR("Metadata file parameters mismatch");
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    manager->free_blocks = header.free_blocks;
    
    // 读取位图
    uint64_t bitmap_words = (manager->total_blocks + 63) / 64;
    if (fread(manager->bitmap.bits, sizeof(uint64_t), bitmap_words, fp) != bitmap_words) {
        fclose(fp);
        return DISTFS_ERROR_SYSTEM_ERROR;
    }
    
    manager->bitmap.free_blocks = header.free_blocks;
    
    // 读取元数据
    if (fread(manager->metadata, sizeof(block_metadata_t), manager->total_blocks, fp) != manager->total_blocks) {
        fclose(fp);
        return DISTFS_ERROR_SYSTEM_ERROR;
    }
    
    fclose(fp);
    
    DISTFS_LOG_INFO("Loaded metadata: %lu total blocks, %lu free blocks", 
                   manager->total_blocks, manager->free_blocks);
    
    return DISTFS_SUCCESS;
}

/* 分配数据块 */
uint64_t distfs_block_allocate(distfs_block_manager_t *manager) {
    if (!manager || !manager->initialized) {
        return 0;
    }
    
    pthread_mutex_lock(&manager->bitmap.mutex);
    
    if (manager->bitmap.free_blocks == 0) {
        pthread_mutex_unlock(&manager->bitmap.mutex);
        return 0; // 没有空闲块
    }
    
    uint64_t block_id = find_first_free_bit(manager->bitmap.bits, manager->bitmap.total_blocks);
    if (block_id == UINT64_MAX) {
        pthread_mutex_unlock(&manager->bitmap.mutex);
        return 0;
    }
    
    // 标记块为已分配
    set_bit(manager->bitmap.bits, block_id);
    manager->bitmap.free_blocks--;
    manager->free_blocks--;
    
    pthread_mutex_unlock(&manager->bitmap.mutex);
    
    // 更新元数据
    pthread_rwlock_wrlock(&manager->metadata_lock);
    
    block_metadata_t *meta = &manager->metadata[block_id];
    meta->block_id = block_id;
    meta->status = 1; // allocated
    meta->created_time = time(NULL);
    meta->modified_time = meta->created_time;
    meta->ref_count = 1;
    
    pthread_rwlock_unlock(&manager->metadata_lock);
    
    // 更新统计信息
    pthread_mutex_lock(&manager->mutex);
    manager->allocations++;
    pthread_mutex_unlock(&manager->mutex);
    
    DISTFS_LOG_DEBUG("Allocated block %lu", block_id);

    return block_id;
}

/* 释放数据块 */
int distfs_block_free(distfs_block_manager_t *manager, uint64_t block_id) {
    if (!manager || !manager->initialized || block_id >= manager->total_blocks) {
        return DISTFS_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&manager->bitmap.mutex);

    if (!test_bit(manager->bitmap.bits, block_id)) {
        pthread_mutex_unlock(&manager->bitmap.mutex);
        return DISTFS_ERROR_INVALID_PARAM; // 块未分配
    }

    // 标记块为空闲
    clear_bit(manager->bitmap.bits, block_id);
    manager->bitmap.free_blocks++;
    manager->free_blocks++;

    pthread_mutex_unlock(&manager->bitmap.mutex);

    // 更新元数据
    pthread_rwlock_wrlock(&manager->metadata_lock);

    block_metadata_t *meta = &manager->metadata[block_id];
    memset(meta, 0, sizeof(block_metadata_t));
    meta->block_id = block_id;
    meta->status = 0; // free

    pthread_rwlock_unlock(&manager->metadata_lock);

    // 更新统计信息
    pthread_mutex_lock(&manager->mutex);
    manager->deallocations++;
    pthread_mutex_unlock(&manager->mutex);

    DISTFS_LOG_DEBUG("Freed block %lu", block_id);

    return DISTFS_SUCCESS;
}

/* 获取块元数据 */
int distfs_block_get_metadata(distfs_block_manager_t *manager, uint64_t block_id,
                             block_metadata_t *metadata) {
    if (!manager || !manager->initialized || !metadata || block_id >= manager->total_blocks) {
        return DISTFS_ERROR_INVALID_PARAM;
    }

    pthread_rwlock_rdlock(&manager->metadata_lock);

    *metadata = manager->metadata[block_id];

    pthread_rwlock_unlock(&manager->metadata_lock);

    return DISTFS_SUCCESS;
}

/* 设置块元数据 */
int distfs_block_set_metadata(distfs_block_manager_t *manager, uint64_t block_id,
                             const block_metadata_t *metadata) {
    if (!manager || !manager->initialized || !metadata || block_id >= manager->total_blocks) {
        return DISTFS_ERROR_INVALID_PARAM;
    }

    pthread_rwlock_wrlock(&manager->metadata_lock);

    manager->metadata[block_id] = *metadata;
    manager->metadata[block_id].modified_time = time(NULL);

    pthread_rwlock_unlock(&manager->metadata_lock);

    return DISTFS_SUCCESS;
}

/* 创建块管理器 */
distfs_block_manager_t* distfs_block_manager_create(const char *data_dir,
                                                   uint64_t block_size,
                                                   uint64_t total_blocks) {
    if (!data_dir || block_size == 0 || total_blocks == 0) {
        return NULL;
    }

    if (g_block_manager) {
        return NULL; // 单例模式
    }

    g_block_manager = calloc(1, sizeof(distfs_block_manager_t));
    if (!g_block_manager) {
        return NULL;
    }

    strncpy(g_block_manager->data_dir, data_dir, sizeof(g_block_manager->data_dir) - 1);
    g_block_manager->block_size = block_size;
    g_block_manager->total_blocks = total_blocks;
    g_block_manager->free_blocks = total_blocks;

    snprintf(g_block_manager->metadata_file, sizeof(g_block_manager->metadata_file),
             "%s/block_metadata.dat", data_dir);

    // 初始化互斥锁和读写锁
    if (pthread_mutex_init(&g_block_manager->mutex, NULL) != 0 ||
        pthread_rwlock_init(&g_block_manager->metadata_lock, NULL) != 0) {
        free(g_block_manager);
        g_block_manager = NULL;
        return NULL;
    }

    // 初始化位图
    if (init_bitmap(&g_block_manager->bitmap, total_blocks) != DISTFS_SUCCESS) {
        pthread_mutex_destroy(&g_block_manager->mutex);
        pthread_rwlock_destroy(&g_block_manager->metadata_lock);
        free(g_block_manager);
        g_block_manager = NULL;
        return NULL;
    }

    // 分配元数据数组
    g_block_manager->metadata = calloc(total_blocks, sizeof(block_metadata_t));
    if (!g_block_manager->metadata) {
        cleanup_bitmap(&g_block_manager->bitmap);
        pthread_mutex_destroy(&g_block_manager->mutex);
        pthread_rwlock_destroy(&g_block_manager->metadata_lock);
        free(g_block_manager);
        g_block_manager = NULL;
        return NULL;
    }

    // 初始化元数据
    for (uint64_t i = 0; i < total_blocks; i++) {
        g_block_manager->metadata[i].block_id = i;
        g_block_manager->metadata[i].status = 0; // free
    }

    // 加载现有元数据
    if (load_metadata(g_block_manager) != DISTFS_SUCCESS) {
        DISTFS_LOG_WARN("Failed to load existing metadata, starting fresh");
    }

    g_block_manager->initialized = true;

    DISTFS_LOG_INFO("Block manager created: %lu blocks, block size %lu",
                   total_blocks, block_size);

    return g_block_manager;
}

/* 销毁块管理器 */
void distfs_block_manager_destroy(distfs_block_manager_t *manager) {
    if (!manager) return;

    // 保存元数据
    if (manager->initialized) {
        save_metadata(manager);
    }

    // 清理位图
    cleanup_bitmap(&manager->bitmap);

    // 清理元数据
    if (manager->metadata) {
        free(manager->metadata);
    }

    pthread_mutex_destroy(&manager->mutex);
    pthread_rwlock_destroy(&manager->metadata_lock);

    if (manager == g_block_manager) {
        g_block_manager = NULL;
    }

    free(manager);
}

/* 同步元数据到磁盘 */
int distfs_block_manager_sync(distfs_block_manager_t *manager) {
    if (!manager || !manager->initialized) {
        return DISTFS_ERROR_INVALID_PARAM;
    }

    return save_metadata(manager);
}

/* 获取块管理器统计信息 */
int distfs_block_manager_get_stats(distfs_block_manager_t *manager,
                                  distfs_block_stats_t *stats) {
    if (!manager || !manager->initialized || !stats) {
        return DISTFS_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&manager->mutex);

    stats->total_blocks = manager->total_blocks;
    stats->free_blocks = manager->free_blocks;
    stats->used_blocks = manager->total_blocks - manager->free_blocks;
    stats->block_size = manager->block_size;
    stats->allocations = manager->allocations;
    stats->deallocations = manager->deallocations;
    stats->reads = manager->reads;
    stats->writes = manager->writes;

    pthread_mutex_unlock(&manager->mutex);

    return DISTFS_SUCCESS;
}

/* 检查块是否已分配 */
bool distfs_block_is_allocated(distfs_block_manager_t *manager, uint64_t block_id) {
    if (!manager || !manager->initialized || block_id >= manager->total_blocks) {
        return false;
    }

    pthread_mutex_lock(&manager->bitmap.mutex);
    bool allocated = test_bit(manager->bitmap.bits, block_id);
    pthread_mutex_unlock(&manager->bitmap.mutex);

    return allocated;
}

/* 获取空闲块数量 */
uint64_t distfs_block_get_free_count(distfs_block_manager_t *manager) {
    if (!manager || !manager->initialized) {
        return 0;
    }

    pthread_mutex_lock(&manager->bitmap.mutex);
    uint64_t free_blocks = manager->bitmap.free_blocks;
    pthread_mutex_unlock(&manager->bitmap.mutex);

    return free_blocks;
}

/* 批量分配块 */
int distfs_block_allocate_batch(distfs_block_manager_t *manager,
                               uint64_t count, uint64_t *block_ids) {
    if (!manager || !manager->initialized || count == 0 || !block_ids) {
        return DISTFS_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&manager->bitmap.mutex);

    if (manager->bitmap.free_blocks < count) {
        pthread_mutex_unlock(&manager->bitmap.mutex);
        return DISTFS_ERROR_STORAGE_FULL;
    }

    uint64_t allocated = 0;
    uint64_t start_bit = 0;

    while (allocated < count && start_bit < manager->bitmap.total_blocks) {
        uint64_t block_id = find_first_free_bit(manager->bitmap.bits + start_bit / 64,
                                               manager->bitmap.total_blocks - start_bit);
        if (block_id == UINT64_MAX) {
            break;
        }

        block_id += start_bit;
        set_bit(manager->bitmap.bits, block_id);
        block_ids[allocated] = block_id;
        allocated++;
        start_bit = block_id + 1;
    }

    if (allocated < count) {
        // 回滚已分配的块
        for (uint64_t i = 0; i < allocated; i++) {
            clear_bit(manager->bitmap.bits, block_ids[i]);
        }
        pthread_mutex_unlock(&manager->bitmap.mutex);
        return DISTFS_ERROR_STORAGE_FULL;
    }

    manager->bitmap.free_blocks -= count;
    manager->free_blocks -= count;

    pthread_mutex_unlock(&manager->bitmap.mutex);

    // 更新元数据
    pthread_rwlock_wrlock(&manager->metadata_lock);

    time_t now = time(NULL);
    for (uint64_t i = 0; i < count; i++) {
        block_metadata_t *meta = &manager->metadata[block_ids[i]];
        meta->block_id = block_ids[i];
        meta->status = 1; // allocated
        meta->created_time = now;
        meta->modified_time = now;
        meta->ref_count = 1;
    }

    pthread_rwlock_unlock(&manager->metadata_lock);

    // 更新统计信息
    pthread_mutex_lock(&manager->mutex);
    manager->allocations += count;
    pthread_mutex_unlock(&manager->mutex);

    DISTFS_LOG_DEBUG("Allocated %lu blocks in batch", count);

    return DISTFS_SUCCESS;
}
