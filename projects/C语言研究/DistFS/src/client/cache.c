/*
 * DistFS 客户端缓存管理
 * 提供高效的数据和元数据缓存功能
 */

#define _GNU_SOURCE
#include "distfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>

/* 缓存项结构 */
typedef struct cache_entry {
    char key[256];
    void *data;
    size_t size;
    time_t created_time;
    time_t last_access_time;
    uint32_t access_count;
    bool dirty;
    struct cache_entry *next;
    struct cache_entry *prev;
} cache_entry_t;

/* 缓存结构 */
struct distfs_cache {
    cache_entry_t **hash_table;
    int hash_table_size;
    
    /* LRU链表 */
    cache_entry_t *head;
    cache_entry_t *tail;
    
    /* 配置参数 */
    size_t max_size;
    size_t current_size;
    int max_entries;
    int current_entries;
    int ttl; // 生存时间(秒)
    
    /* 统计信息 */
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
    uint64_t insertions;
    
    pthread_mutex_t mutex;
};

/* 哈希函数 */
static uint32_t hash_key(const char *key) {
    uint32_t hash = 5381;
    int c;
    
    while ((c = *key++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    return hash;
}

/* 创建缓存项 */
static cache_entry_t* create_cache_entry(const char *key, const void *data, size_t size) {
    cache_entry_t *entry = calloc(1, sizeof(cache_entry_t));
    if (!entry) return NULL;
    
    strncpy(entry->key, key, sizeof(entry->key) - 1);
    
    entry->data = malloc(size);
    if (!entry->data) {
        free(entry);
        return NULL;
    }
    
    memcpy(entry->data, data, size);
    entry->size = size;
    entry->created_time = time(NULL);
    entry->last_access_time = entry->created_time;
    entry->access_count = 1;
    entry->dirty = false;
    
    return entry;
}

/* 销毁缓存项 */
static void destroy_cache_entry(cache_entry_t *entry) {
    if (!entry) return;
    
    if (entry->data) {
        free(entry->data);
    }
    
    free(entry);
}

/* 从LRU链表中移除项 */
static void remove_from_lru(distfs_cache_t *cache, cache_entry_t *entry) {
    if (entry->prev) {
        entry->prev->next = entry->next;
    } else {
        cache->head = entry->next;
    }
    
    if (entry->next) {
        entry->next->prev = entry->prev;
    } else {
        cache->tail = entry->prev;
    }
    
    entry->prev = NULL;
    entry->next = NULL;
}

/* 添加到LRU链表头部 */
static void add_to_lru_head(distfs_cache_t *cache, cache_entry_t *entry) {
    entry->next = cache->head;
    entry->prev = NULL;
    
    if (cache->head) {
        cache->head->prev = entry;
    } else {
        cache->tail = entry;
    }
    
    cache->head = entry;
}

/* 移动到LRU链表头部 */
static void move_to_lru_head(distfs_cache_t *cache, cache_entry_t *entry) {
    if (cache->head == entry) {
        return; // 已经在头部
    }
    
    remove_from_lru(cache, entry);
    add_to_lru_head(cache, entry);
}

/* 从哈希表中查找项 */
static cache_entry_t* find_in_hash_table(distfs_cache_t *cache, const char *key) {
    uint32_t hash = hash_key(key) % cache->hash_table_size;
    cache_entry_t *entry = cache->hash_table[hash];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

/* 添加到哈希表 */
static void add_to_hash_table(distfs_cache_t *cache, cache_entry_t *entry) {
    uint32_t hash = hash_key(entry->key) % cache->hash_table_size;
    
    entry->next = cache->hash_table[hash];
    if (cache->hash_table[hash]) {
        cache->hash_table[hash]->prev = entry;
    }
    cache->hash_table[hash] = entry;
}

/* 从哈希表中移除 */
static void remove_from_hash_table(distfs_cache_t *cache, cache_entry_t *entry) {
    uint32_t hash = hash_key(entry->key) % cache->hash_table_size;
    
    if (cache->hash_table[hash] == entry) {
        cache->hash_table[hash] = entry->next;
    }
    
    if (entry->prev) {
        entry->prev->next = entry->next;
    }
    
    if (entry->next) {
        entry->next->prev = entry->prev;
    }
}

/* 淘汰最少使用的项 */
static void evict_lru_entry(distfs_cache_t *cache) {
    if (!cache->tail) return;
    
    cache_entry_t *entry = cache->tail;
    
    remove_from_lru(cache, entry);
    remove_from_hash_table(cache, entry);
    
    cache->current_size -= entry->size;
    cache->current_entries--;
    cache->evictions++;
    
    destroy_cache_entry(entry);
}

/* 检查并清理过期项 */
static void cleanup_expired_entries(distfs_cache_t *cache) {
    if (cache->ttl <= 0) return;
    
    time_t now = time(NULL);
    cache_entry_t *entry = cache->tail;
    
    while (entry) {
        cache_entry_t *prev = entry->prev;
        
        if (now - entry->created_time > cache->ttl) {
            remove_from_lru(cache, entry);
            remove_from_hash_table(cache, entry);
            
            cache->current_size -= entry->size;
            cache->current_entries--;
            
            destroy_cache_entry(entry);
        }
        
        entry = prev;
    }
}

/* 创建缓存 */
distfs_cache_t* distfs_cache_create(size_t max_size, int max_entries, int ttl) {
    if (max_size == 0 || max_entries <= 0) {
        return NULL;
    }
    
    distfs_cache_t *cache = calloc(1, sizeof(distfs_cache_t));
    if (!cache) {
        return NULL;
    }
    
    cache->hash_table_size = max_entries * 2; // 减少哈希冲突
    cache->hash_table = calloc(cache->hash_table_size, sizeof(cache_entry_t*));
    if (!cache->hash_table) {
        free(cache);
        return NULL;
    }
    
    cache->max_size = max_size;
    cache->max_entries = max_entries;
    cache->ttl = ttl;
    
    if (pthread_mutex_init(&cache->mutex, NULL) != 0) {
        free(cache->hash_table);
        free(cache);
        return NULL;
    }
    
    return cache;
}

/* 插入缓存项 */
int distfs_cache_put(distfs_cache_t *cache, const char *key, const void *data, size_t size) {
    if (!cache || !key || !data || size == 0) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&cache->mutex);
    
    // 检查是否已存在
    cache_entry_t *existing = find_in_hash_table(cache, key);
    if (existing) {
        // 更新现有项
        void *new_data = realloc(existing->data, size);
        if (!new_data) {
            pthread_mutex_unlock(&cache->mutex);
            return DISTFS_ERROR_NO_MEMORY;
        }
        
        cache->current_size -= existing->size;
        existing->data = new_data;
        memcpy(existing->data, data, size);
        existing->size = size;
        existing->last_access_time = time(NULL);
        existing->access_count++;
        cache->current_size += size;
        
        move_to_lru_head(cache, existing);
        
        pthread_mutex_unlock(&cache->mutex);
        return DISTFS_SUCCESS;
    }
    
    // 创建新项
    cache_entry_t *entry = create_cache_entry(key, data, size);
    if (!entry) {
        pthread_mutex_unlock(&cache->mutex);
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    // 检查容量限制
    while ((cache->current_size + size > cache->max_size || 
            cache->current_entries >= cache->max_entries) && 
           cache->tail) {
        evict_lru_entry(cache);
    }
    
    // 添加新项
    add_to_hash_table(cache, entry);
    add_to_lru_head(cache, entry);
    
    cache->current_size += size;
    cache->current_entries++;
    cache->insertions++;
    
    pthread_mutex_unlock(&cache->mutex);
    
    return DISTFS_SUCCESS;
}

/* 获取缓存项 */
int distfs_cache_get(distfs_cache_t *cache, const char *key, void **data, size_t *size) {
    if (!cache || !key || !data || !size) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&cache->mutex);
    
    // 清理过期项
    cleanup_expired_entries(cache);
    
    cache_entry_t *entry = find_in_hash_table(cache, key);
    if (!entry) {
        cache->misses++;
        pthread_mutex_unlock(&cache->mutex);
        return DISTFS_ERROR_NOT_FOUND;
    }
    
    // 检查是否过期
    if (cache->ttl > 0 && time(NULL) - entry->created_time > cache->ttl) {
        remove_from_lru(cache, entry);
        remove_from_hash_table(cache, entry);
        
        cache->current_size -= entry->size;
        cache->current_entries--;
        
        destroy_cache_entry(entry);
        
        cache->misses++;
        pthread_mutex_unlock(&cache->mutex);
        return DISTFS_ERROR_NOT_FOUND;
    }
    
    // 复制数据
    *data = malloc(entry->size);
    if (!*data) {
        pthread_mutex_unlock(&cache->mutex);
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    memcpy(*data, entry->data, entry->size);
    *size = entry->size;
    
    // 更新访问信息
    entry->last_access_time = time(NULL);
    entry->access_count++;
    move_to_lru_head(cache, entry);
    
    cache->hits++;
    
    pthread_mutex_unlock(&cache->mutex);
    
    return DISTFS_SUCCESS;
}

/* 删除缓存项 */
int distfs_cache_remove(distfs_cache_t *cache, const char *key) {
    if (!cache || !key) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&cache->mutex);
    
    cache_entry_t *entry = find_in_hash_table(cache, key);
    if (!entry) {
        pthread_mutex_unlock(&cache->mutex);
        return DISTFS_ERROR_NOT_FOUND;
    }
    
    remove_from_lru(cache, entry);
    remove_from_hash_table(cache, entry);
    
    cache->current_size -= entry->size;
    cache->current_entries--;
    
    destroy_cache_entry(entry);
    
    pthread_mutex_unlock(&cache->mutex);
    
    return DISTFS_SUCCESS;
}

/* 清空缓存 */
void distfs_cache_clear(distfs_cache_t *cache) {
    if (!cache) return;
    
    pthread_mutex_lock(&cache->mutex);
    
    cache_entry_t *entry = cache->head;
    while (entry) {
        cache_entry_t *next = entry->next;
        destroy_cache_entry(entry);
        entry = next;
    }
    
    memset(cache->hash_table, 0, cache->hash_table_size * sizeof(cache_entry_t*));
    
    cache->head = NULL;
    cache->tail = NULL;
    cache->current_size = 0;
    cache->current_entries = 0;
    
    pthread_mutex_unlock(&cache->mutex);
}

/* 获取缓存统计信息 */
int distfs_cache_get_stats(distfs_cache_t *cache, distfs_cache_stats_t *stats) {
    if (!cache || !stats) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&cache->mutex);
    
    stats->max_size = cache->max_size;
    stats->current_size = cache->current_size;
    stats->max_entries = cache->max_entries;
    stats->current_entries = cache->current_entries;
    stats->hits = cache->hits;
    stats->misses = cache->misses;
    stats->evictions = cache->evictions;
    stats->insertions = cache->insertions;
    
    if (cache->hits + cache->misses > 0) {
        stats->hit_rate = (double)cache->hits / (cache->hits + cache->misses);
    } else {
        stats->hit_rate = 0.0;
    }
    
    pthread_mutex_unlock(&cache->mutex);
    
    return DISTFS_SUCCESS;
}

/* 销毁缓存 */
void distfs_cache_destroy(distfs_cache_t *cache) {
    if (!cache) return;
    
    distfs_cache_clear(cache);
    
    if (cache->hash_table) {
        free(cache->hash_table);
    }
    
    pthread_mutex_destroy(&cache->mutex);
    free(cache);
}
