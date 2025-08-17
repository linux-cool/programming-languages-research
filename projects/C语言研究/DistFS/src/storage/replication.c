/*
 * DistFS 数据复制模块
 * 管理数据块的复制和一致性
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

/* 复制状态 */
typedef enum {
    REPLICATION_PENDING = 0,
    REPLICATION_IN_PROGRESS = 1,
    REPLICATION_COMPLETED = 2,
    REPLICATION_FAILED = 3
} replication_status_t;

/* 复制任务 */
typedef struct replication_task {
    uint64_t task_id;
    uint64_t block_id;
    char source_node[64];
    char target_nodes[DISTFS_MAX_REPLICAS][64];
    int target_count;
    int completed_count;
    replication_status_t status;
    time_t created_time;
    time_t started_time;
    time_t completed_time;
    int retry_count;
    char error_message[256];
    struct replication_task *next;
} replication_task_t;

/* 节点状态 */
typedef struct node_status {
    char node_id[64];
    char address[256];
    uint16_t port;
    bool active;
    time_t last_heartbeat;
    uint64_t capacity;
    uint64_t used_space;
    uint64_t free_space;
    struct node_status *next;
} node_status_t;

/* 复制管理器结构 */
struct distfs_replication_manager {
    /* 任务管理 */
    replication_task_t *task_queue;
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cond;
    
    /* 节点管理 */
    node_status_t *nodes;
    pthread_mutex_t nodes_mutex;
    distfs_hash_ring_t *hash_ring;
    
    /* 工作线程 */
    pthread_t *worker_threads;
    int worker_count;
    bool running;
    
    /* 配置参数 */
    int replica_count;
    int max_retry_count;
    int heartbeat_interval;
    int task_timeout;
    
    /* 统计信息 */
    uint64_t total_tasks;
    uint64_t completed_tasks;
    uint64_t failed_tasks;
    uint64_t bytes_replicated;
    
    pthread_mutex_t stats_mutex;
};

/* 全局复制管理器实例 */
static distfs_replication_manager_t *g_replication_manager = NULL;

/* 生成任务ID */
static uint64_t generate_task_id(void) {
    static uint64_t counter = 0;
    return __sync_fetch_and_add(&counter, 1);
}

/* 创建复制任务 */
static replication_task_t* create_replication_task(uint64_t block_id, 
                                                  const char *source_node,
                                                  const char **target_nodes,
                                                  int target_count) {
    replication_task_t *task = calloc(1, sizeof(replication_task_t));
    if (!task) return NULL;
    
    task->task_id = generate_task_id();
    task->block_id = block_id;
    strncpy(task->source_node, source_node, sizeof(task->source_node) - 1);
    
    task->target_count = target_count < DISTFS_MAX_REPLICAS ? target_count : DISTFS_MAX_REPLICAS;
    for (int i = 0; i < task->target_count; i++) {
        strncpy(task->target_nodes[i], target_nodes[i], sizeof(task->target_nodes[i]) - 1);
    }
    
    task->status = REPLICATION_PENDING;
    task->created_time = time(NULL);
    task->retry_count = 0;
    
    return task;
}

/* 销毁复制任务 */
static void destroy_replication_task(replication_task_t *task) {
    if (task) {
        free(task);
    }
}

/* 添加任务到队列 */
static int enqueue_task(distfs_replication_manager_t *manager, replication_task_t *task) {
    pthread_mutex_lock(&manager->queue_mutex);
    
    task->next = manager->task_queue;
    manager->task_queue = task;
    
    pthread_mutex_lock(&manager->stats_mutex);
    manager->total_tasks++;
    pthread_mutex_unlock(&manager->stats_mutex);
    
    pthread_cond_signal(&manager->queue_cond);
    pthread_mutex_unlock(&manager->queue_mutex);
    
    return DISTFS_SUCCESS;
}

/* 从队列获取任务 */
static replication_task_t* dequeue_task(distfs_replication_manager_t *manager) {
    pthread_mutex_lock(&manager->queue_mutex);
    
    while (!manager->task_queue && manager->running) {
        pthread_cond_wait(&manager->queue_cond, &manager->queue_mutex);
    }
    
    if (!manager->running) {
        pthread_mutex_unlock(&manager->queue_mutex);
        return NULL;
    }
    
    replication_task_t *task = manager->task_queue;
    if (task) {
        manager->task_queue = task->next;
        task->next = NULL;
    }
    
    pthread_mutex_unlock(&manager->queue_mutex);
    
    return task;
}

/* 查找节点状态 */
static node_status_t* find_node(distfs_replication_manager_t *manager, const char *node_id) {
    node_status_t *current = manager->nodes;
    while (current) {
        if (strcmp(current->node_id, node_id) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/* 执行单个复制操作 */
static int replicate_block_to_node(const char *source_node, const char *target_node, 
                                  uint64_t block_id) {
    // 连接到源节点
    distfs_connection_t *source_conn = distfs_connection_create(source_node, 9528);
    if (!source_conn) {
        DISTFS_LOG_ERROR("Failed to connect to source node %s", source_node);
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    // 读取块数据
    distfs_message_t *read_request = distfs_message_create(DISTFS_MSG_READ_BLOCK, 
                                                          &block_id, sizeof(block_id));
    if (!read_request) {
        distfs_connection_destroy(source_conn);
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    int result = distfs_message_send(source_conn, read_request);
    distfs_message_destroy(read_request);
    
    if (result != DISTFS_SUCCESS) {
        distfs_connection_destroy(source_conn);
        return result;
    }
    
    distfs_message_t *read_response = distfs_message_receive(source_conn);
    if (!read_response || read_response->header.type != DISTFS_MSG_DATA) {
        if (read_response) distfs_message_destroy(read_response);
        distfs_connection_destroy(source_conn);
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    // 连接到目标节点
    distfs_connection_t *target_conn = distfs_connection_create(target_node, 9528);
    if (!target_conn) {
        distfs_message_destroy(read_response);
        distfs_connection_destroy(source_conn);
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    // 写入块数据
    struct {
        uint64_t block_id;
        uint64_t size;
    } write_header = {
        .block_id = block_id,
        .size = read_response->header.length
    };
    
    size_t write_size = sizeof(write_header) + read_response->header.length;
    void *write_data = malloc(write_size);
    if (!write_data) {
        distfs_message_destroy(read_response);
        distfs_connection_destroy(source_conn);
        distfs_connection_destroy(target_conn);
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    memcpy(write_data, &write_header, sizeof(write_header));
    memcpy((char*)write_data + sizeof(write_header), read_response->payload, 
           read_response->header.length);
    
    distfs_message_t *write_request = distfs_message_create(DISTFS_MSG_WRITE_BLOCK, 
                                                           write_data, write_size);
    free(write_data);
    distfs_message_destroy(read_response);
    distfs_connection_destroy(source_conn);
    
    if (!write_request) {
        distfs_connection_destroy(target_conn);
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    result = distfs_message_send(target_conn, write_request);
    distfs_message_destroy(write_request);
    
    if (result != DISTFS_SUCCESS) {
        distfs_connection_destroy(target_conn);
        return result;
    }
    
    distfs_message_t *write_response = distfs_message_receive(target_conn);
    if (!write_response || write_response->header.type != DISTFS_MSG_SUCCESS) {
        if (write_response) distfs_message_destroy(write_response);
        distfs_connection_destroy(target_conn);
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    distfs_message_destroy(write_response);
    distfs_connection_destroy(target_conn);
    
    return DISTFS_SUCCESS;
}

/* 执行复制任务 */
static int execute_replication_task(distfs_replication_manager_t *manager, 
                                   replication_task_t *task) {
    task->status = REPLICATION_IN_PROGRESS;
    task->started_time = time(NULL);
    
    int success_count = 0;
    int failure_count = 0;
    
    for (int i = 0; i < task->target_count; i++) {
        int result = replicate_block_to_node(task->source_node, 
                                           task->target_nodes[i], 
                                           task->block_id);
        
        if (result == DISTFS_SUCCESS) {
            success_count++;
            DISTFS_LOG_DEBUG("Successfully replicated block %lu to node %s", 
                           task->block_id, task->target_nodes[i]);
        } else {
            failure_count++;
            DISTFS_LOG_ERROR("Failed to replicate block %lu to node %s: %s", 
                           task->block_id, task->target_nodes[i], 
                           distfs_strerror(result));
        }
    }
    
    task->completed_count = success_count;
    task->completed_time = time(NULL);
    
    if (success_count > 0) {
        task->status = REPLICATION_COMPLETED;
        
        pthread_mutex_lock(&manager->stats_mutex);
        manager->completed_tasks++;
        pthread_mutex_unlock(&manager->stats_mutex);
        
        return DISTFS_SUCCESS;
    } else {
        task->status = REPLICATION_FAILED;
        task->retry_count++;
        
        if (task->retry_count < manager->max_retry_count) {
            // 重新加入队列进行重试
            task->status = REPLICATION_PENDING;
            enqueue_task(manager, task);
            return DISTFS_SUCCESS;
        } else {
            pthread_mutex_lock(&manager->stats_mutex);
            manager->failed_tasks++;
            pthread_mutex_unlock(&manager->stats_mutex);
            
            return DISTFS_ERROR_NETWORK_FAILURE;
        }
    }
}

/* 工作线程函数 */
static void* worker_thread(void *arg) {
    distfs_replication_manager_t *manager = (distfs_replication_manager_t*)arg;

    while (manager->running) {
        replication_task_t *task = dequeue_task(manager);
        if (!task) break;

        DISTFS_LOG_DEBUG("Processing replication task %lu for block %lu",
                        task->task_id, task->block_id);

        int result = execute_replication_task(manager, task);

        if (result != DISTFS_SUCCESS && task->status == REPLICATION_FAILED) {
            DISTFS_LOG_ERROR("Replication task %lu failed permanently", task->task_id);
            destroy_replication_task(task);
        } else if (task->status == REPLICATION_COMPLETED) {
            DISTFS_LOG_INFO("Replication task %lu completed successfully", task->task_id);
            destroy_replication_task(task);
        }
        // 如果任务被重新加入队列，不要销毁它
    }

    return NULL;
}

/* 创建复制管理器 */
distfs_replication_manager_t* distfs_replication_manager_create(int replica_count,
                                                               int worker_count) {
    if (replica_count <= 0 || worker_count <= 0) {
        return NULL;
    }

    if (g_replication_manager) {
        return NULL; // 单例模式
    }

    g_replication_manager = calloc(1, sizeof(distfs_replication_manager_t));
    if (!g_replication_manager) {
        return NULL;
    }

    g_replication_manager->replica_count = replica_count;
    g_replication_manager->worker_count = worker_count;
    g_replication_manager->max_retry_count = 3;
    g_replication_manager->heartbeat_interval = 30;
    g_replication_manager->task_timeout = 300;

    // 初始化互斥锁和条件变量
    if (pthread_mutex_init(&g_replication_manager->queue_mutex, NULL) != 0 ||
        pthread_cond_init(&g_replication_manager->queue_cond, NULL) != 0 ||
        pthread_mutex_init(&g_replication_manager->nodes_mutex, NULL) != 0 ||
        pthread_mutex_init(&g_replication_manager->stats_mutex, NULL) != 0) {
        free(g_replication_manager);
        g_replication_manager = NULL;
        return NULL;
    }

    // 创建一致性哈希环
    g_replication_manager->hash_ring = distfs_hash_ring_create(150);
    if (!g_replication_manager->hash_ring) {
        pthread_mutex_destroy(&g_replication_manager->queue_mutex);
        pthread_cond_destroy(&g_replication_manager->queue_cond);
        pthread_mutex_destroy(&g_replication_manager->nodes_mutex);
        pthread_mutex_destroy(&g_replication_manager->stats_mutex);
        free(g_replication_manager);
        g_replication_manager = NULL;
        return NULL;
    }

    // 分配工作线程数组
    g_replication_manager->worker_threads = calloc(worker_count, sizeof(pthread_t));
    if (!g_replication_manager->worker_threads) {
        distfs_hash_ring_destroy(g_replication_manager->hash_ring);
        pthread_mutex_destroy(&g_replication_manager->queue_mutex);
        pthread_cond_destroy(&g_replication_manager->queue_cond);
        pthread_mutex_destroy(&g_replication_manager->nodes_mutex);
        pthread_mutex_destroy(&g_replication_manager->stats_mutex);
        free(g_replication_manager);
        g_replication_manager = NULL;
        return NULL;
    }

    return g_replication_manager;
}

/* 启动复制管理器 */
int distfs_replication_manager_start(distfs_replication_manager_t *manager) {
    if (!manager || manager->running) {
        return DISTFS_ERROR_INVALID_PARAM;
    }

    manager->running = true;

    // 启动工作线程
    for (int i = 0; i < manager->worker_count; i++) {
        if (pthread_create(&manager->worker_threads[i], NULL, worker_thread, manager) != 0) {
            manager->running = false;

            // 等待已启动的线程结束
            for (int j = 0; j < i; j++) {
                pthread_join(manager->worker_threads[j], NULL);
            }

            return DISTFS_ERROR_SYSTEM_ERROR;
        }
    }

    DISTFS_LOG_INFO("Replication manager started with %d workers", manager->worker_count);

    return DISTFS_SUCCESS;
}

/* 停止复制管理器 */
int distfs_replication_manager_stop(distfs_replication_manager_t *manager) {
    if (!manager || !manager->running) {
        return DISTFS_ERROR_INVALID_PARAM;
    }

    manager->running = false;

    // 唤醒所有等待的工作线程
    pthread_cond_broadcast(&manager->queue_cond);

    // 等待所有工作线程结束
    for (int i = 0; i < manager->worker_count; i++) {
        pthread_join(manager->worker_threads[i], NULL);
    }

    DISTFS_LOG_INFO("Replication manager stopped");

    return DISTFS_SUCCESS;
}

/* 销毁复制管理器 */
void distfs_replication_manager_destroy(distfs_replication_manager_t *manager) {
    if (!manager) return;

    if (manager->running) {
        distfs_replication_manager_stop(manager);
    }

    // 清理任务队列
    replication_task_t *task = manager->task_queue;
    while (task) {
        replication_task_t *next = task->next;
        destroy_replication_task(task);
        task = next;
    }

    // 清理节点列表
    node_status_t *node = manager->nodes;
    while (node) {
        node_status_t *next = node->next;
        free(node);
        node = next;
    }

    // 清理哈希环
    if (manager->hash_ring) {
        distfs_hash_ring_destroy(manager->hash_ring);
    }

    // 清理工作线程数组
    if (manager->worker_threads) {
        free(manager->worker_threads);
    }

    pthread_mutex_destroy(&manager->queue_mutex);
    pthread_cond_destroy(&manager->queue_cond);
    pthread_mutex_destroy(&manager->nodes_mutex);
    pthread_mutex_destroy(&manager->stats_mutex);

    if (manager == g_replication_manager) {
        g_replication_manager = NULL;
    }

    free(manager);
}
