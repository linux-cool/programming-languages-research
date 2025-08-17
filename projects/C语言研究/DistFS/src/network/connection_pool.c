/*
 * DistFS 连接池管理
 * 提供高效的连接复用和管理功能
 */

#define _GNU_SOURCE
#include "distfs.h"
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>

/* 连接池中的连接项 */
typedef struct pool_connection {
    distfs_connection_t *conn;
    char hostname[256];
    uint16_t port;
    time_t created_time;
    time_t last_used_time;
    bool in_use;
    uint32_t use_count;
    struct pool_connection *next;
} pool_connection_t;

/* 连接池结构 */
struct distfs_connection_pool {
    pool_connection_t *connections;
    int max_connections;
    int current_connections;
    int max_idle_time;
    int max_lifetime;
    
    /* 同步控制 */
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    
    /* 清理线程 */
    pthread_t cleanup_thread;
    bool running;
    
    /* 统计信息 */
    uint64_t total_created;
    uint64_t total_destroyed;
    uint64_t total_requests;
    uint64_t cache_hits;
    uint64_t cache_misses;
};

/* 全局连接池实例 */
static distfs_connection_pool_t *g_connection_pool = NULL;

/* 生成连接键 */
static void generate_connection_key(const char *hostname, uint16_t port, char *key, size_t key_size) {
    snprintf(key, key_size, "%s:%d", hostname, port);
}

/* 查找可用连接 */
static pool_connection_t* find_available_connection(distfs_connection_pool_t *pool,
                                                   const char *hostname, uint16_t port) {
    pool_connection_t *current = pool->connections;
    
    while (current) {
        if (!current->in_use && 
            strcmp(current->hostname, hostname) == 0 && 
            current->port == port) {
            
            // 检查连接是否仍然有效
            time_t now = time(NULL);
            if (now - current->created_time > pool->max_lifetime ||
                now - current->last_used_time > pool->max_idle_time) {
                // 连接过期，需要重新创建
                return NULL;
            }
            
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

/* 创建新的池连接 */
static pool_connection_t* create_pool_connection(const char *hostname, uint16_t port) {
    pool_connection_t *pool_conn = calloc(1, sizeof(pool_connection_t));
    if (!pool_conn) {
        return NULL;
    }
    
    // 创建实际连接
    pool_conn->conn = distfs_connection_create(hostname, port);
    if (!pool_conn->conn) {
        free(pool_conn);
        return NULL;
    }
    
    // 连接到服务器
    if (distfs_connection_connect(pool_conn->conn) != DISTFS_SUCCESS) {
        distfs_connection_destroy(pool_conn->conn);
        free(pool_conn);
        return NULL;
    }
    
    strncpy(pool_conn->hostname, hostname, sizeof(pool_conn->hostname) - 1);
    pool_conn->port = port;
    pool_conn->created_time = time(NULL);
    pool_conn->last_used_time = pool_conn->created_time;
    pool_conn->in_use = false;
    pool_conn->use_count = 0;
    
    return pool_conn;
}

/* 销毁池连接 */
static void destroy_pool_connection(pool_connection_t *pool_conn) {
    if (!pool_conn) return;
    
    if (pool_conn->conn) {
        distfs_connection_destroy(pool_conn->conn);
    }
    
    free(pool_conn);
}

/* 清理过期连接 */
static void cleanup_expired_connections(distfs_connection_pool_t *pool) {
    pthread_mutex_lock(&pool->mutex);
    
    time_t now = time(NULL);
    pool_connection_t *prev = NULL;
    pool_connection_t *current = pool->connections;
    
    while (current) {
        pool_connection_t *next = current->next;
        
        bool should_remove = false;
        
        if (!current->in_use) {
            // 检查是否过期
            if (now - current->created_time > pool->max_lifetime ||
                now - current->last_used_time > pool->max_idle_time) {
                should_remove = true;
            }
        }
        
        if (should_remove) {
            // 从链表中移除
            if (prev) {
                prev->next = next;
            } else {
                pool->connections = next;
            }
            
            destroy_pool_connection(current);
            pool->current_connections--;
            pool->total_destroyed++;
            
            DISTFS_LOG_DEBUG("Removed expired connection to %s:%d", 
                           current->hostname, current->port);
        } else {
            prev = current;
        }
        
        current = next;
    }
    
    pthread_mutex_unlock(&pool->mutex);
}

/* 清理线程函数 */
static void* cleanup_thread_func(void *arg) {
    distfs_connection_pool_t *pool = (distfs_connection_pool_t*)arg;
    
    while (pool->running) {
        cleanup_expired_connections(pool);
        sleep(60); // 每分钟清理一次
    }
    
    return NULL;
}

/* 创建连接池 */
distfs_connection_pool_t* distfs_connection_pool_create(int max_connections) {
    if (max_connections <= 0) {
        return NULL;
    }
    
    if (g_connection_pool) {
        return NULL; // 单例模式
    }
    
    g_connection_pool = calloc(1, sizeof(distfs_connection_pool_t));
    if (!g_connection_pool) {
        return NULL;
    }
    
    g_connection_pool->max_connections = max_connections;
    g_connection_pool->max_idle_time = 300; // 5分钟
    g_connection_pool->max_lifetime = 3600; // 1小时
    
    // 初始化互斥锁和条件变量
    if (pthread_mutex_init(&g_connection_pool->mutex, NULL) != 0 ||
        pthread_cond_init(&g_connection_pool->cond, NULL) != 0) {
        free(g_connection_pool);
        g_connection_pool = NULL;
        return NULL;
    }
    
    g_connection_pool->running = true;
    
    // 启动清理线程
    if (pthread_create(&g_connection_pool->cleanup_thread, NULL, 
                      cleanup_thread_func, g_connection_pool) != 0) {
        pthread_mutex_destroy(&g_connection_pool->mutex);
        pthread_cond_destroy(&g_connection_pool->cond);
        free(g_connection_pool);
        g_connection_pool = NULL;
        return NULL;
    }
    
    DISTFS_LOG_INFO("Connection pool created with max %d connections", max_connections);
    
    return g_connection_pool;
}

/* 从连接池获取连接 */
distfs_connection_t* distfs_connection_pool_get(distfs_connection_pool_t *pool,
                                               const char *hostname, uint16_t port) {
    if (!pool || !hostname || port == 0) {
        return NULL;
    }
    
    pthread_mutex_lock(&pool->mutex);
    
    pool->total_requests++;
    
    // 查找可用连接
    pool_connection_t *pool_conn = find_available_connection(pool, hostname, port);
    
    if (pool_conn) {
        // 找到可用连接
        pool_conn->in_use = true;
        pool_conn->last_used_time = time(NULL);
        pool_conn->use_count++;
        pool->cache_hits++;
        
        pthread_mutex_unlock(&pool->mutex);
        
        DISTFS_LOG_DEBUG("Reusing connection to %s:%d", hostname, port);
        return pool_conn->conn;
    }
    
    // 检查连接数限制
    if (pool->current_connections >= pool->max_connections) {
        DISTFS_LOG_WARN("Connection pool limit reached");
        pthread_mutex_unlock(&pool->mutex);
        return NULL;
    }
    
    pool->cache_misses++;
    
    pthread_mutex_unlock(&pool->mutex);
    
    // 创建新连接
    pool_conn = create_pool_connection(hostname, port);
    if (!pool_conn) {
        DISTFS_LOG_ERROR("Failed to create new connection to %s:%d", hostname, port);
        return NULL;
    }
    
    pthread_mutex_lock(&pool->mutex);
    
    // 添加到连接池
    pool_conn->next = pool->connections;
    pool->connections = pool_conn;
    pool->current_connections++;
    pool->total_created++;
    
    pool_conn->in_use = true;
    pool_conn->use_count++;
    
    pthread_mutex_unlock(&pool->mutex);
    
    DISTFS_LOG_DEBUG("Created new connection to %s:%d", hostname, port);
    
    return pool_conn->conn;
}

/* 归还连接到连接池 */
int distfs_connection_pool_return(distfs_connection_pool_t *pool, 
                                 distfs_connection_t *conn) {
    if (!pool || !conn) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&pool->mutex);
    
    // 查找对应的池连接
    pool_connection_t *current = pool->connections;
    while (current) {
        if (current->conn == conn && current->in_use) {
            current->in_use = false;
            current->last_used_time = time(NULL);
            
            pthread_cond_signal(&pool->cond);
            pthread_mutex_unlock(&pool->mutex);
            
            return DISTFS_SUCCESS;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&pool->mutex);
    
    return DISTFS_ERROR_NOT_FOUND;
}

/* 销毁连接池 */
void distfs_connection_pool_destroy(distfs_connection_pool_t *pool) {
    if (!pool) return;
    
    pool->running = false;
    
    // 等待清理线程结束
    pthread_join(pool->cleanup_thread, NULL);
    
    pthread_mutex_lock(&pool->mutex);
    
    // 清理所有连接
    pool_connection_t *current = pool->connections;
    while (current) {
        pool_connection_t *next = current->next;
        destroy_pool_connection(current);
        current = next;
    }
    
    pthread_mutex_unlock(&pool->mutex);
    
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
    
    if (pool == g_connection_pool) {
        g_connection_pool = NULL;
    }
    
    free(pool);
    
    DISTFS_LOG_INFO("Connection pool destroyed");
}

/* 获取连接池统计信息 */
int distfs_connection_pool_get_stats(distfs_connection_pool_t *pool,
                                    distfs_connection_pool_stats_t *stats) {
    if (!pool || !stats) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&pool->mutex);
    
    stats->max_connections = pool->max_connections;
    stats->current_connections = pool->current_connections;
    stats->total_created = pool->total_created;
    stats->total_destroyed = pool->total_destroyed;
    stats->total_requests = pool->total_requests;
    stats->cache_hits = pool->cache_hits;
    stats->cache_misses = pool->cache_misses;
    
    if (pool->total_requests > 0) {
        stats->hit_rate = (double)pool->cache_hits / pool->total_requests;
    } else {
        stats->hit_rate = 0.0;
    }
    
    pthread_mutex_unlock(&pool->mutex);
    
    return DISTFS_SUCCESS;
}
