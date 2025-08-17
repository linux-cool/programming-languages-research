/*
 * DistFS 存储节点实现
 * 管理数据块的存储、复制和访问
 */

#define _GNU_SOURCE
#include "distfs.h"
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <stdbool.h>
#include <dirent.h>

/* 数据块信息结构 */
typedef struct block_info {
    uint64_t block_id;
    uint64_t size;
    uint32_t checksum;
    time_t created_time;
    time_t accessed_time;
    uint32_t ref_count;
    char file_path[DISTFS_MAX_PATH_LEN];
    struct block_info *next;
} block_info_t;

/* 复制任务结构 */
typedef struct replication_task {
    uint64_t block_id;
    char source_node[64];
    char target_nodes[DISTFS_MAX_REPLICAS][64];
    int target_count;
    time_t created_time;
    struct replication_task *next;
} replication_task_t;

/* 存储节点结构 */
struct distfs_storage_node {
    char node_id[64];
    char data_dir[DISTFS_MAX_PATH_LEN];
    uint64_t capacity;
    uint64_t used_space;
    uint64_t free_space;
    
    /* 网络服务 */
    distfs_network_server_t *network_server;
    uint16_t port;
    
    /* 块管理 */
    block_info_t *blocks[1024]; // 简单哈希表
    pthread_mutex_t blocks_mutex;
    uint64_t next_block_id;
    
    /* 复制管理 */
    replication_task_t *replication_queue;
    pthread_mutex_t replication_mutex;
    pthread_t replication_thread;
    
    /* 统计信息 */
    uint64_t total_blocks;
    uint64_t total_reads;
    uint64_t total_writes;
    uint64_t bytes_read;
    uint64_t bytes_written;
    
    bool running;
    pthread_mutex_t mutex;
};

/* 全局存储节点实例 */
static distfs_storage_node_t *g_storage_node = NULL;

/* 计算块哈希值 */
static uint32_t hash_block_id(uint64_t block_id) {
    return (uint32_t)(block_id % 1024);
}

/* 生成块文件路径 */
static void generate_block_path(const char *data_dir, uint64_t block_id, char *path, size_t path_size) {
    uint32_t dir1 = (uint32_t)(block_id % 256);
    uint32_t dir2 = (uint32_t)((block_id / 256) % 256);
    snprintf(path, path_size, "%s/blocks/%02x/%02x/%016lx.dat", 
             data_dir, dir1, dir2, block_id);
}

/* 创建块目录 */
static int create_block_directories(const char *data_dir) {
    char dir_path[DISTFS_MAX_PATH_LEN];
    
    for (int i = 0; i < 256; i++) {
        for (int j = 0; j < 256; j++) {
            snprintf(dir_path, sizeof(dir_path), "%s/blocks/%02x/%02x", data_dir, i, j);
            if (mkdir(dir_path, 0755) != 0 && errno != EEXIST) {
                DISTFS_LOG_ERROR("Failed to create directory %s: %s", dir_path, strerror(errno));
                return DISTFS_ERROR_SYSTEM_ERROR;
            }
        }
    }
    
    return DISTFS_SUCCESS;
}

/* 计算文件校验和 */
static uint32_t calculate_file_checksum(const char *file_path) {
    FILE *fp = fopen(file_path, "rb");
    if (!fp) return 0;
    
    uint32_t checksum = 0;
    unsigned char buffer[4096];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        checksum = distfs_hash_crc32(buffer, bytes_read);
    }
    
    fclose(fp);
    return checksum;
}

/* 获取存储空间信息 */
static int update_storage_info(distfs_storage_node_t *node) {
    struct statvfs stat;
    
    if (statvfs(node->data_dir, &stat) != 0) {
        DISTFS_LOG_ERROR("Failed to get storage info for %s: %s", 
                        node->data_dir, strerror(errno));
        return DISTFS_ERROR_SYSTEM_ERROR;
    }
    
    pthread_mutex_lock(&node->mutex);
    
    node->capacity = stat.f_blocks * stat.f_frsize;
    node->free_space = stat.f_bavail * stat.f_frsize;
    node->used_space = node->capacity - node->free_space;
    
    pthread_mutex_unlock(&node->mutex);
    
    return DISTFS_SUCCESS;
}

/* 查找块信息 */
static block_info_t* find_block_info(distfs_storage_node_t *node, uint64_t block_id) {
    uint32_t hash = hash_block_id(block_id);
    block_info_t *current = node->blocks[hash];
    
    while (current) {
        if (current->block_id == block_id) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

/* 添加块信息 */
static int add_block_info(distfs_storage_node_t *node, block_info_t *block) {
    uint32_t hash = hash_block_id(block->block_id);
    
    pthread_mutex_lock(&node->blocks_mutex);
    
    block->next = node->blocks[hash];
    node->blocks[hash] = block;
    node->total_blocks++;
    
    pthread_mutex_unlock(&node->blocks_mutex);
    
    return DISTFS_SUCCESS;
}

/* 删除块信息 */
static int remove_block_info(distfs_storage_node_t *node, uint64_t block_id) {
    uint32_t hash = hash_block_id(block_id);
    
    pthread_mutex_lock(&node->blocks_mutex);
    
    block_info_t *current = node->blocks[hash];
    block_info_t *prev = NULL;
    
    while (current) {
        if (current->block_id == block_id) {
            if (prev) {
                prev->next = current->next;
            } else {
                node->blocks[hash] = current->next;
            }
            
            free(current);
            node->total_blocks--;
            
            pthread_mutex_unlock(&node->blocks_mutex);
            return DISTFS_SUCCESS;
        }
        prev = current;
        current = current->next;
    }
    
    pthread_mutex_unlock(&node->blocks_mutex);
    return DISTFS_ERROR_NOT_FOUND;
}

/* 写入数据块 */
static int write_block(distfs_storage_node_t *node, uint64_t block_id, 
                      const void *data, size_t size) {
    char block_path[DISTFS_MAX_PATH_LEN];
    generate_block_path(node->data_dir, block_id, block_path, sizeof(block_path));
    
    // 创建临时文件
    char temp_path[DISTFS_MAX_PATH_LEN];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", block_path);
    
    FILE *fp = fopen(temp_path, "wb");
    if (!fp) {
        DISTFS_LOG_ERROR("Failed to create temp file %s: %s", temp_path, strerror(errno));
        return DISTFS_ERROR_FILE_OPEN_FAILED;
    }
    
    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);
    
    if (written != size) {
        unlink(temp_path);
        DISTFS_LOG_ERROR("Failed to write block data: written %zu, expected %zu", written, size);
        return DISTFS_ERROR_SYSTEM_ERROR;
    }
    
    // 原子性重命名
    if (rename(temp_path, block_path) != 0) {
        unlink(temp_path);
        DISTFS_LOG_ERROR("Failed to rename temp file: %s", strerror(errno));
        return DISTFS_ERROR_SYSTEM_ERROR;
    }
    
    // 创建块信息
    block_info_t *block = calloc(1, sizeof(block_info_t));
    if (!block) {
        unlink(block_path);
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    block->block_id = block_id;
    block->size = size;
    block->checksum = distfs_hash_crc32(data, size);
    block->created_time = time(NULL);
    block->accessed_time = block->created_time;
    block->ref_count = 1;
    strncpy(block->file_path, block_path, sizeof(block->file_path) - 1);
    
    add_block_info(node, block);
    
    // 更新统计信息
    pthread_mutex_lock(&node->mutex);
    node->total_writes++;
    node->bytes_written += size;
    pthread_mutex_unlock(&node->mutex);
    
    DISTFS_LOG_DEBUG("Block %lu written successfully, size: %zu", block_id, size);
    
    return DISTFS_SUCCESS;
}

/* 读取数据块 */
static int read_block(distfs_storage_node_t *node, uint64_t block_id, 
                     void **data, size_t *size) {
    pthread_mutex_lock(&node->blocks_mutex);
    
    block_info_t *block = find_block_info(node, block_id);
    if (!block) {
        pthread_mutex_unlock(&node->blocks_mutex);
        return DISTFS_ERROR_NOT_FOUND;
    }
    
    // 更新访问时间
    block->accessed_time = time(NULL);
    
    char block_path[DISTFS_MAX_PATH_LEN];
    strncpy(block_path, block->file_path, sizeof(block_path) - 1);
    size_t block_size = block->size;
    
    pthread_mutex_unlock(&node->blocks_mutex);
    
    FILE *fp = fopen(block_path, "rb");
    if (!fp) {
        DISTFS_LOG_ERROR("Failed to open block file %s: %s", block_path, strerror(errno));
        return DISTFS_ERROR_FILE_OPEN_FAILED;
    }
    
    void *buffer = malloc(block_size);
    if (!buffer) {
        fclose(fp);
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    size_t bytes_read = fread(buffer, 1, block_size, fp);
    fclose(fp);
    
    if (bytes_read != block_size) {
        free(buffer);
        DISTFS_LOG_ERROR("Failed to read block data: read %zu, expected %zu", bytes_read, block_size);
        return DISTFS_ERROR_SYSTEM_ERROR;
    }
    
    // 验证校验和
    uint32_t checksum = distfs_hash_crc32(buffer, block_size);
    if (checksum != block->checksum) {
        free(buffer);
        DISTFS_LOG_ERROR("Block checksum mismatch: calculated %u, expected %u", 
                        checksum, block->checksum);
        return DISTFS_ERROR_CONSISTENCY_VIOLATION;
    }
    
    *data = buffer;
    *size = block_size;
    
    // 更新统计信息
    pthread_mutex_lock(&node->mutex);
    node->total_reads++;
    node->bytes_read += block_size;
    pthread_mutex_unlock(&node->mutex);
    
    DISTFS_LOG_DEBUG("Block %lu read successfully, size: %zu", block_id, block_size);

    return DISTFS_SUCCESS;
}

/* 删除数据块 */
static int delete_block(distfs_storage_node_t *node, uint64_t block_id) {
    pthread_mutex_lock(&node->blocks_mutex);

    block_info_t *block = find_block_info(node, block_id);
    if (!block) {
        pthread_mutex_unlock(&node->blocks_mutex);
        return DISTFS_ERROR_NOT_FOUND;
    }

    char block_path[DISTFS_MAX_PATH_LEN];
    strncpy(block_path, block->file_path, sizeof(block_path) - 1);

    pthread_mutex_unlock(&node->blocks_mutex);

    // 删除文件
    if (unlink(block_path) != 0) {
        DISTFS_LOG_ERROR("Failed to delete block file %s: %s", block_path, strerror(errno));
        return DISTFS_ERROR_SYSTEM_ERROR;
    }

    // 删除块信息
    remove_block_info(node, block_id);

    DISTFS_LOG_DEBUG("Block %lu deleted successfully", block_id);

    return DISTFS_SUCCESS;
}

/* 处理写块请求 */
static int handle_write_block(distfs_connection_t *conn, distfs_message_t *request) {
    if (!request->payload || request->header.length < sizeof(struct {
        uint64_t block_id;
        uint64_t size;
    })) {
        return DISTFS_ERROR_INVALID_PARAM;
    }

    struct {
        uint64_t block_id;
        uint64_t size;
        char data[];
    } *req = (typeof(req))request->payload;

    uint64_t block_id = req->block_id;
    uint64_t size = req->size;

    if (request->header.length != sizeof(*req) - sizeof(req->data) + size) {
        DISTFS_LOG_ERROR("Invalid write block request size");

        int error = DISTFS_ERROR_INVALID_PARAM;
        distfs_message_t *response = distfs_message_create(DISTFS_MSG_ERROR,
                                                          &error, sizeof(error));
        if (response) {
            distfs_message_send(conn, response);
            distfs_message_destroy(response);
        }
        return DISTFS_ERROR_INVALID_PARAM;
    }

    int result = write_block(g_storage_node, block_id, req->data, size);

    if (result == DISTFS_SUCCESS) {
        distfs_message_t *response = distfs_message_create(DISTFS_MSG_SUCCESS, NULL, 0);
        if (response) {
            distfs_message_send(conn, response);
            distfs_message_destroy(response);
        }
    } else {
        distfs_message_t *response = distfs_message_create(DISTFS_MSG_ERROR,
                                                          &result, sizeof(result));
        if (response) {
            distfs_message_send(conn, response);
            distfs_message_destroy(response);
        }
    }

    return result;
}

/* 处理读块请求 */
static int handle_read_block(distfs_connection_t *conn, distfs_message_t *request) {
    if (!request->payload || request->header.length != sizeof(uint64_t)) {
        return DISTFS_ERROR_INVALID_PARAM;
    }

    uint64_t block_id = *(uint64_t*)request->payload;

    void *data;
    size_t size;
    int result = read_block(g_storage_node, block_id, &data, &size);

    if (result == DISTFS_SUCCESS) {
        distfs_message_t *response = distfs_message_create(DISTFS_MSG_DATA, data, size);
        if (response) {
            distfs_message_send(conn, response);
            distfs_message_destroy(response);
        }
        free(data);
    } else {
        distfs_message_t *response = distfs_message_create(DISTFS_MSG_ERROR,
                                                          &result, sizeof(result));
        if (response) {
            distfs_message_send(conn, response);
            distfs_message_destroy(response);
        }
    }

    return result;
}

/* 处理删除块请求 */
static int handle_delete_block(distfs_connection_t *conn, distfs_message_t *request) {
    if (!request->payload || request->header.length != sizeof(uint64_t)) {
        return DISTFS_ERROR_INVALID_PARAM;
    }

    uint64_t block_id = *(uint64_t*)request->payload;

    int result = delete_block(g_storage_node, block_id);

    if (result == DISTFS_SUCCESS) {
        distfs_message_t *response = distfs_message_create(DISTFS_MSG_SUCCESS, NULL, 0);
        if (response) {
            distfs_message_send(conn, response);
            distfs_message_destroy(response);
        }
    } else {
        distfs_message_t *response = distfs_message_create(DISTFS_MSG_ERROR,
                                                          &result, sizeof(result));
        if (response) {
            distfs_message_send(conn, response);
            distfs_message_destroy(response);
        }
    }

    return result;
}

/* 消息处理回调 */
static int message_handler(distfs_connection_t *conn, distfs_message_t *message, void *user_data) {
    (void)user_data; // 未使用

    switch (message->header.type) {
        case DISTFS_MSG_WRITE_BLOCK:
            return handle_write_block(conn, message);

        case DISTFS_MSG_READ_BLOCK:
            return handle_read_block(conn, message);

        case DISTFS_MSG_DELETE_BLOCK:
            return handle_delete_block(conn, message);

        case DISTFS_MSG_PING:
            {
                distfs_message_t *pong = distfs_message_create(DISTFS_MSG_PONG, NULL, 0);
                if (pong) {
                    distfs_message_send(conn, pong);
                    distfs_message_destroy(pong);
                }
                return DISTFS_SUCCESS;
            }

        default:
            return DISTFS_ERROR_UNSUPPORTED_OPERATION;
    }
}

/* 创建存储节点 */
distfs_storage_node_t* distfs_storage_node_create(const char *node_id,
                                                  const char *data_dir,
                                                  uint16_t port) {
    if (!node_id || !data_dir) {
        return NULL;
    }

    if (g_storage_node) {
        return NULL; // 单例模式
    }

    g_storage_node = calloc(1, sizeof(distfs_storage_node_t));
    if (!g_storage_node) {
        return NULL;
    }

    strncpy(g_storage_node->node_id, node_id, sizeof(g_storage_node->node_id) - 1);
    strncpy(g_storage_node->data_dir, data_dir, sizeof(g_storage_node->data_dir) - 1);
    g_storage_node->port = port;
    g_storage_node->next_block_id = 1;

    // 初始化互斥锁
    if (pthread_mutex_init(&g_storage_node->mutex, NULL) != 0 ||
        pthread_mutex_init(&g_storage_node->blocks_mutex, NULL) != 0 ||
        pthread_mutex_init(&g_storage_node->replication_mutex, NULL) != 0) {
        free(g_storage_node);
        g_storage_node = NULL;
        return NULL;
    }

    // 创建数据目录
    if (mkdir(data_dir, 0755) != 0 && errno != EEXIST) {
        DISTFS_LOG_ERROR("Failed to create data directory %s: %s", data_dir, strerror(errno));
        distfs_storage_node_destroy(g_storage_node);
        return NULL;
    }

    // 创建块目录结构
    if (create_block_directories(data_dir) != DISTFS_SUCCESS) {
        distfs_storage_node_destroy(g_storage_node);
        return NULL;
    }

    // 创建网络服务器
    g_storage_node->network_server = distfs_network_server_create(
        port, 1000, message_handler, g_storage_node
    );

    if (!g_storage_node->network_server) {
        distfs_storage_node_destroy(g_storage_node);
        return NULL;
    }

    // 更新存储信息
    update_storage_info(g_storage_node);

    return g_storage_node;
}

/* 启动存储节点 */
int distfs_storage_node_start(distfs_storage_node_t *node) {
    if (!node || node->running) {
        return DISTFS_ERROR_INVALID_PARAM;
    }

    int result = distfs_network_server_start(node->network_server);
    if (result == DISTFS_SUCCESS) {
        node->running = true;
        DISTFS_LOG_INFO("Storage node %s started on port %d", node->node_id, node->port);
    }

    return result;
}

/* 停止存储节点 */
int distfs_storage_node_stop(distfs_storage_node_t *node) {
    if (!node || !node->running) {
        return DISTFS_ERROR_INVALID_PARAM;
    }

    node->running = false;

    int result = distfs_network_server_stop(node->network_server);

    DISTFS_LOG_INFO("Storage node %s stopped", node->node_id);

    return result;
}

/* 销毁存储节点 */
void distfs_storage_node_destroy(distfs_storage_node_t *node) {
    if (!node) return;

    if (node->running) {
        distfs_storage_node_stop(node);
    }

    // 清理网络服务器
    if (node->network_server) {
        distfs_network_server_destroy(node->network_server);
    }

    // 清理块信息
    for (int i = 0; i < 1024; i++) {
        block_info_t *current = node->blocks[i];
        while (current) {
            block_info_t *next = current->next;
            free(current);
            current = next;
        }
    }

    // 清理复制任务
    replication_task_t *task = node->replication_queue;
    while (task) {
        replication_task_t *next = task->next;
        free(task);
        task = next;
    }

    pthread_mutex_destroy(&node->mutex);
    pthread_mutex_destroy(&node->blocks_mutex);
    pthread_mutex_destroy(&node->replication_mutex);

    if (node == g_storage_node) {
        g_storage_node = NULL;
    }

    free(node);
}
