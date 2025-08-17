/*
 * DistFS 元数据服务器实现
 * 管理文件系统的元数据信息
 */

#include "distfs.h"
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>

/* 文件元数据结构 */
typedef struct file_metadata {
    uint64_t inode;
    char name[DISTFS_MAX_PATH_LEN];
    uint64_t size;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    uint32_t nlinks;
    uint64_t blocks[16];
    uint32_t block_count;
    struct file_metadata *next;
} file_metadata_t;

/* 目录项结构 */
typedef struct directory_entry {
    uint64_t inode;
    char name[256];
    uint8_t type; // DT_REG, DT_DIR, etc.
    struct directory_entry *next;
} directory_entry_t;

/* 存储节点信息 */
typedef struct storage_node {
    char node_id[64];
    char address[256];
    uint16_t port;
    uint64_t capacity;
    uint64_t used_space;
    uint64_t free_space;
    time_t last_heartbeat;
    bool active;
    struct storage_node *next;
} storage_node_t;

/* 元数据服务器结构 */
struct distfs_metadata_server {
    distfs_config_t config;
    distfs_network_server_t *network_server;
    
    /* 元数据存储 */
    file_metadata_t *file_table[1024]; // 简单哈希表
    directory_entry_t *dir_table[1024];
    
    /* 存储节点管理 */
    storage_node_t *storage_nodes;
    distfs_hash_ring_t *hash_ring;
    
    /* 同步控制 */
    pthread_mutex_t metadata_mutex;
    pthread_mutex_t nodes_mutex;
    
    /* 统计信息 */
    uint64_t total_files;
    uint64_t total_directories;
    uint64_t next_inode;
    
    bool running;
};

/* 全局元数据服务器实例 */
static distfs_metadata_server_t *g_metadata_server = NULL;

/* 计算哈希值 */
static uint32_t hash_string(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % 1024;
}

/* 查找文件元数据 */
static file_metadata_t* find_file_metadata(const char *path) {
    uint32_t hash = hash_string(path);
    file_metadata_t *current = g_metadata_server->file_table[hash];
    
    while (current) {
        if (strcmp(current->name, path) == 0) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

/* 添加文件元数据 */
static int add_file_metadata(file_metadata_t *metadata) {
    uint32_t hash = hash_string(metadata->name);
    
    metadata->next = g_metadata_server->file_table[hash];
    g_metadata_server->file_table[hash] = metadata;
    g_metadata_server->total_files++;
    
    return DISTFS_SUCCESS;
}

/* 删除文件元数据 */
static int remove_file_metadata(const char *path) {
    uint32_t hash = hash_string(path);
    file_metadata_t *current = g_metadata_server->file_table[hash];
    file_metadata_t *prev = NULL;
    
    while (current) {
        if (strcmp(current->name, path) == 0) {
            if (prev) {
                prev->next = current->next;
            } else {
                g_metadata_server->file_table[hash] = current->next;
            }
            free(current);
            g_metadata_server->total_files--;
            return DISTFS_SUCCESS;
        }
        prev = current;
        current = current->next;
    }
    
    return DISTFS_ERROR_NOT_FOUND;
}

/* 分配新的inode */
static uint64_t allocate_inode(void) {
    return __sync_fetch_and_add(&g_metadata_server->next_inode, 1);
}

/* 选择存储节点 */
static storage_node_t* select_storage_nodes(const char *filename, int replica_count, 
                                           storage_node_t **nodes) {
    if (!g_metadata_server->hash_ring) {
        return NULL;
    }
    
    const char *node_ids[replica_count];
    int count = distfs_hash_ring_get_nodes(g_metadata_server->hash_ring, 
                                          filename, strlen(filename),
                                          node_ids, replica_count);
    
    int found = 0;
    storage_node_t *current = g_metadata_server->storage_nodes;
    
    while (current && found < count) {
        for (int i = 0; i < count; i++) {
            if (strcmp(current->node_id, node_ids[i]) == 0 && current->active) {
                nodes[found++] = current;
                break;
            }
        }
        current = current->next;
    }
    
    return found > 0 ? nodes[0] : NULL;
}

/* 处理创建文件请求 */
static int handle_create_file(distfs_connection_t *conn, distfs_message_t *request) {
    if (!request->payload || request->header.length < sizeof(struct {
        char path[DISTFS_MAX_PATH_LEN];
        uint32_t mode;
    })) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    struct {
        char path[DISTFS_MAX_PATH_LEN];
        uint32_t mode;
    } *req = (typeof(req))request->payload;
    
    pthread_mutex_lock(&g_metadata_server->metadata_mutex);
    
    // 检查文件是否已存在
    if (find_file_metadata(req->path)) {
        pthread_mutex_unlock(&g_metadata_server->metadata_mutex);
        
        int error = DISTFS_ERROR_FILE_EXISTS;
        distfs_message_t *response = distfs_message_create(DISTFS_MSG_ERROR, 
                                                          &error, sizeof(error));
        if (response) {
            distfs_message_send(conn, response);
            distfs_message_destroy(response);
        }
        return DISTFS_ERROR_FILE_EXISTS;
    }
    
    // 创建文件元数据
    file_metadata_t *metadata = calloc(1, sizeof(file_metadata_t));
    if (!metadata) {
        pthread_mutex_unlock(&g_metadata_server->metadata_mutex);
        
        int error = DISTFS_ERROR_NO_MEMORY;
        distfs_message_t *response = distfs_message_create(DISTFS_MSG_ERROR, 
                                                          &error, sizeof(error));
        if (response) {
            distfs_message_send(conn, response);
            distfs_message_destroy(response);
        }
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    metadata->inode = allocate_inode();
    strncpy(metadata->name, req->path, sizeof(metadata->name) - 1);
    metadata->size = 0;
    metadata->mode = req->mode;
    metadata->uid = 0; // TODO: 从连接获取用户ID
    metadata->gid = 0;
    metadata->atime = metadata->mtime = metadata->ctime = time(NULL);
    metadata->nlinks = 1;
    metadata->block_count = 0;
    
    add_file_metadata(metadata);
    
    pthread_mutex_unlock(&g_metadata_server->metadata_mutex);
    
    // 发送成功响应
    distfs_message_t *response = distfs_message_create(DISTFS_MSG_SUCCESS, NULL, 0);
    if (response) {
        distfs_message_send(conn, response);
        distfs_message_destroy(response);
    }
    
    return DISTFS_SUCCESS;
}

/* 处理打开文件请求 */
static int handle_open_file(distfs_connection_t *conn, distfs_message_t *request) {
    if (!request->payload || request->header.length < sizeof(struct {
        char path[DISTFS_MAX_PATH_LEN];
        int flags;
    })) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    struct {
        char path[DISTFS_MAX_PATH_LEN];
        int flags;
    } *req = (typeof(req))request->payload;
    
    pthread_mutex_lock(&g_metadata_server->metadata_mutex);
    
    file_metadata_t *metadata = find_file_metadata(req->path);
    if (!metadata) {
        pthread_mutex_unlock(&g_metadata_server->metadata_mutex);
        
        int error = DISTFS_ERROR_FILE_NOT_FOUND;
        distfs_message_t *response = distfs_message_create(DISTFS_MSG_ERROR, 
                                                          &error, sizeof(error));
        if (response) {
            distfs_message_send(conn, response);
            distfs_message_destroy(response);
        }
        return DISTFS_ERROR_FILE_NOT_FOUND;
    }
    
    uint64_t inode = metadata->inode;
    
    pthread_mutex_unlock(&g_metadata_server->metadata_mutex);
    
    // 发送成功响应，包含inode
    distfs_message_t *response = distfs_message_create(DISTFS_MSG_SUCCESS, 
                                                      &inode, sizeof(inode));
    if (response) {
        distfs_message_send(conn, response);
        distfs_message_destroy(response);
    }
    
    return DISTFS_SUCCESS;
}

/* 处理删除文件请求 */
static int handle_delete_file(distfs_connection_t *conn, distfs_message_t *request) {
    if (!request->payload || request->header.length == 0) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    char *path = (char*)request->payload;
    
    pthread_mutex_lock(&g_metadata_server->metadata_mutex);
    
    int result = remove_file_metadata(path);
    
    pthread_mutex_unlock(&g_metadata_server->metadata_mutex);
    
    if (result == DISTFS_SUCCESS) {
        distfs_message_t *response = distfs_message_create(DISTFS_MSG_SUCCESS, NULL, 0);
        if (response) {
            distfs_message_send(conn, response);
            distfs_message_destroy(response);
        }
    } else {
        int error = result;
        distfs_message_t *response = distfs_message_create(DISTFS_MSG_ERROR, 
                                                          &error, sizeof(error));
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
        case DISTFS_MSG_CREATE_FILE:
            return handle_create_file(conn, message);
            
        case DISTFS_MSG_OPEN_FILE:
            return handle_open_file(conn, message);
            
        case DISTFS_MSG_DELETE_FILE:
            return handle_delete_file(conn, message);
            
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

/* 创建元数据服务器 */
distfs_metadata_server_t* distfs_metadata_server_create(const distfs_config_t *config) {
    if (g_metadata_server) {
        return NULL; // 单例模式
    }
    
    g_metadata_server = calloc(1, sizeof(distfs_metadata_server_t));
    if (!g_metadata_server) {
        return NULL;
    }
    
    if (config) {
        g_metadata_server->config = *config;
    } else {
        // 使用默认配置
        g_metadata_server->config.listen_port = 9527;
        g_metadata_server->config.max_connections = 1000;
        g_metadata_server->config.thread_pool_size = 8;
    }
    
    // 初始化互斥锁
    if (pthread_mutex_init(&g_metadata_server->metadata_mutex, NULL) != 0 ||
        pthread_mutex_init(&g_metadata_server->nodes_mutex, NULL) != 0) {
        free(g_metadata_server);
        g_metadata_server = NULL;
        return NULL;
    }
    
    // 创建网络服务器
    g_metadata_server->network_server = distfs_network_server_create(
        g_metadata_server->config.listen_port,
        g_metadata_server->config.max_connections,
        message_handler,
        g_metadata_server
    );
    
    if (!g_metadata_server->network_server) {
        pthread_mutex_destroy(&g_metadata_server->metadata_mutex);
        pthread_mutex_destroy(&g_metadata_server->nodes_mutex);
        free(g_metadata_server);
        g_metadata_server = NULL;
        return NULL;
    }
    
    // 创建一致性哈希环
    g_metadata_server->hash_ring = distfs_hash_ring_create(150);
    if (!g_metadata_server->hash_ring) {
        distfs_network_server_destroy(g_metadata_server->network_server);
        pthread_mutex_destroy(&g_metadata_server->metadata_mutex);
        pthread_mutex_destroy(&g_metadata_server->nodes_mutex);
        free(g_metadata_server);
        g_metadata_server = NULL;
        return NULL;
    }
    
    g_metadata_server->next_inode = 1;
    g_metadata_server->running = false;
    
    return g_metadata_server;
}

/* 启动元数据服务器 */
int distfs_metadata_server_start(distfs_metadata_server_t *server) {
    if (!server || server->running) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    int result = distfs_network_server_start(server->network_server);
    if (result == DISTFS_SUCCESS) {
        server->running = true;
    }
    
    return result;
}

/* 停止元数据服务器 */
int distfs_metadata_server_stop(distfs_metadata_server_t *server) {
    if (!server || !server->running) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    server->running = false;
    return distfs_network_server_stop(server->network_server);
}

/* 销毁元数据服务器 */
void distfs_metadata_server_destroy(distfs_metadata_server_t *server) {
    if (!server) return;
    
    if (server->running) {
        distfs_metadata_server_stop(server);
    }
    
    // 清理网络服务器
    if (server->network_server) {
        distfs_network_server_destroy(server->network_server);
    }
    
    // 清理哈希环
    if (server->hash_ring) {
        distfs_hash_ring_destroy(server->hash_ring);
    }
    
    // 清理元数据
    for (int i = 0; i < 1024; i++) {
        file_metadata_t *current = server->file_table[i];
        while (current) {
            file_metadata_t *next = current->next;
            free(current);
            current = next;
        }
        
        directory_entry_t *dir_current = server->dir_table[i];
        while (dir_current) {
            directory_entry_t *next = dir_current->next;
            free(dir_current);
            dir_current = next;
        }
    }
    
    // 清理存储节点
    storage_node_t *node = server->storage_nodes;
    while (node) {
        storage_node_t *next = node->next;
        free(node);
        node = next;
    }
    
    pthread_mutex_destroy(&server->metadata_mutex);
    pthread_mutex_destroy(&server->nodes_mutex);
    
    if (server == g_metadata_server) {
        g_metadata_server = NULL;
    }
    
    free(server);
}
