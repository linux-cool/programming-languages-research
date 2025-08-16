/*
 * DistFS 存储节点实现
 * 提供数据存储和副本管理功能
 */

#include "distfs.h"
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>

/* 存储节点结构 */
typedef struct {
    uint64_t node_id;
    char data_dir[DISTFS_MAX_PATH_LEN];
    distfs_server_t *server;
    pthread_t heartbeat_thread;
    bool running;
    
    /* 统计信息 */
    uint64_t total_capacity;
    uint64_t used_capacity;
    uint64_t total_blocks;
    uint64_t free_blocks;
    
    /* 配置 */
    uint32_t block_size;
    uint32_t replica_count;
    char metadata_server[256];
    uint16_t metadata_port;
    
    pthread_mutex_t storage_mutex;
} storage_node_t;

static storage_node_t g_storage_node = {0};

/* 初始化存储目录 */
static int init_storage_directory(const char *data_dir) {
    if (!data_dir) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    // 创建数据目录
    if (distfs_mkdir_recursive(data_dir, 0755) != DISTFS_SUCCESS) {
        return DISTFS_ERROR_PERMISSION_DENIED;
    }
    
    // 创建子目录
    char path[DISTFS_MAX_PATH_LEN];
    
    snprintf(path, sizeof(path), "%s/blocks", data_dir);
    if (distfs_mkdir_recursive(path, 0755) != DISTFS_SUCCESS) {
        return DISTFS_ERROR_PERMISSION_DENIED;
    }
    
    snprintf(path, sizeof(path), "%s/metadata", data_dir);
    if (distfs_mkdir_recursive(path, 0755) != DISTFS_SUCCESS) {
        return DISTFS_ERROR_PERMISSION_DENIED;
    }
    
    snprintf(path, sizeof(path), "%s/temp", data_dir);
    if (distfs_mkdir_recursive(path, 0755) != DISTFS_SUCCESS) {
        return DISTFS_ERROR_PERMISSION_DENIED;
    }
    
    return DISTFS_SUCCESS;
}

/* 获取存储统计信息 */
static int update_storage_stats(void) {
    struct statvfs stat;
    
    if (statvfs(g_storage_node.data_dir, &stat) != 0) {
        return DISTFS_ERROR_UNKNOWN;
    }
    
    g_storage_node.total_capacity = stat.f_blocks * stat.f_frsize;
    g_storage_node.free_blocks = stat.f_bavail;
    g_storage_node.used_capacity = g_storage_node.total_capacity - 
                                   (stat.f_bavail * stat.f_frsize);
    
    // 统计数据块数量
    char blocks_dir[DISTFS_MAX_PATH_LEN];
    snprintf(blocks_dir, sizeof(blocks_dir), "%s/blocks", g_storage_node.data_dir);
    
    DIR *dir = opendir(blocks_dir);
    if (dir) {
        struct dirent *entry;
        uint64_t block_count = 0;
        
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG) {
                block_count++;
            }
        }
        
        g_storage_node.total_blocks = block_count;
        closedir(dir);
    }
    
    return DISTFS_SUCCESS;
}

/* 生成数据块路径 */
static void get_block_path(uint64_t block_id, char *path, size_t size) {
    // 使用分层目录结构避免单个目录文件过多
    uint32_t dir1 = (block_id >> 16) & 0xFF;
    uint32_t dir2 = (block_id >> 8) & 0xFF;
    
    snprintf(path, size, "%s/blocks/%02x/%02x/%016lx.dat",
             g_storage_node.data_dir, dir1, dir2, block_id);
}

/* 创建数据块目录 */
static int create_block_directory(uint64_t block_id) {
    uint32_t dir1 = (block_id >> 16) & 0xFF;
    uint32_t dir2 = (block_id >> 8) & 0xFF;
    
    char dir_path[DISTFS_MAX_PATH_LEN];
    snprintf(dir_path, sizeof(dir_path), "%s/blocks/%02x/%02x",
             g_storage_node.data_dir, dir1, dir2);
    
    return distfs_mkdir_recursive(dir_path, 0755);
}

/* 存储数据块 */
static int store_data_block(uint64_t block_id, const void *data, uint32_t size) {
    if (!data || size == 0) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_storage_node.storage_mutex);
    
    // 创建目录
    if (create_block_directory(block_id) != DISTFS_SUCCESS) {
        pthread_mutex_unlock(&g_storage_node.storage_mutex);
        return DISTFS_ERROR_PERMISSION_DENIED;
    }
    
    // 生成文件路径
    char block_path[DISTFS_MAX_PATH_LEN];
    get_block_path(block_id, block_path, sizeof(block_path));
    
    // 写入临时文件
    char temp_path[DISTFS_MAX_PATH_LEN];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", block_path);
    
    int fd = open(temp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        pthread_mutex_unlock(&g_storage_node.storage_mutex);
        return DISTFS_ERROR_PERMISSION_DENIED;
    }
    
    ssize_t written = write(fd, data, size);
    if (written != size) {
        close(fd);
        unlink(temp_path);
        pthread_mutex_unlock(&g_storage_node.storage_mutex);
        return DISTFS_ERROR_STORAGE_FULL;
    }
    
    // 同步到磁盘
    fsync(fd);
    close(fd);
    
    // 原子性重命名
    if (rename(temp_path, block_path) != 0) {
        unlink(temp_path);
        pthread_mutex_unlock(&g_storage_node.storage_mutex);
        return DISTFS_ERROR_UNKNOWN;
    }
    
    pthread_mutex_unlock(&g_storage_node.storage_mutex);
    return DISTFS_SUCCESS;
}

/* 读取数据块 */
static int read_data_block(uint64_t block_id, void **data, uint32_t *size) {
    if (!data || !size) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_storage_node.storage_mutex);
    
    char block_path[DISTFS_MAX_PATH_LEN];
    get_block_path(block_id, block_path, sizeof(block_path));
    
    // 检查文件是否存在
    struct stat st;
    if (stat(block_path, &st) != 0) {
        pthread_mutex_unlock(&g_storage_node.storage_mutex);
        return DISTFS_ERROR_FILE_NOT_FOUND;
    }
    
    // 分配内存
    *data = malloc(st.st_size);
    if (!*data) {
        pthread_mutex_unlock(&g_storage_node.storage_mutex);
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    // 读取文件
    int fd = open(block_path, O_RDONLY);
    if (fd < 0) {
        free(*data);
        *data = NULL;
        pthread_mutex_unlock(&g_storage_node.storage_mutex);
        return DISTFS_ERROR_PERMISSION_DENIED;
    }
    
    ssize_t bytes_read = read(fd, *data, st.st_size);
    close(fd);
    
    if (bytes_read != st.st_size) {
        free(*data);
        *data = NULL;
        pthread_mutex_unlock(&g_storage_node.storage_mutex);
        return DISTFS_ERROR_UNKNOWN;
    }
    
    *size = st.st_size;
    
    pthread_mutex_unlock(&g_storage_node.storage_mutex);
    return DISTFS_SUCCESS;
}

/* 删除数据块 */
static int delete_data_block(uint64_t block_id) {
    pthread_mutex_lock(&g_storage_node.storage_mutex);
    
    char block_path[DISTFS_MAX_PATH_LEN];
    get_block_path(block_id, block_path, sizeof(block_path));
    
    int result = DISTFS_SUCCESS;
    if (unlink(block_path) != 0) {
        if (errno == ENOENT) {
            result = DISTFS_ERROR_FILE_NOT_FOUND;
        } else {
            result = DISTFS_ERROR_PERMISSION_DENIED;
        }
    }
    
    pthread_mutex_unlock(&g_storage_node.storage_mutex);
    return result;
}

/* 处理存储请求 */
static int handle_storage_request(distfs_connection_t *conn, distfs_message_t *msg, void *user_data) {
    (void)user_data;  // 未使用的参数
    
    distfs_message_t *response = NULL;
    int result = DISTFS_SUCCESS;
    
    switch (msg->header.type) {
        case DISTFS_MSG_WRITE_FILE: {
            // 解析写入请求
            if (msg->header.length < sizeof(uint64_t) + sizeof(uint32_t)) {
                result = DISTFS_ERROR_INVALID_PARAM;
                break;
            }
            
            uint64_t block_id = *(uint64_t*)msg->payload;
            uint32_t data_size = *(uint32_t*)(msg->payload + sizeof(uint64_t));
            void *data = msg->payload + sizeof(uint64_t) + sizeof(uint32_t);
            
            if (msg->header.length != sizeof(uint64_t) + sizeof(uint32_t) + data_size) {
                result = DISTFS_ERROR_INVALID_PARAM;
                break;
            }
            
            result = store_data_block(block_id, data, data_size);
            break;
        }
        
        case DISTFS_MSG_READ_FILE: {
            // 解析读取请求
            if (msg->header.length != sizeof(uint64_t)) {
                result = DISTFS_ERROR_INVALID_PARAM;
                break;
            }
            
            uint64_t block_id = *(uint64_t*)msg->payload;
            void *data = NULL;
            uint32_t size = 0;
            
            result = read_data_block(block_id, &data, &size);
            if (result == DISTFS_SUCCESS) {
                response = distfs_message_create(DISTFS_MSG_DATA, data, size);
                free(data);
            }
            break;
        }
        
        case DISTFS_MSG_DELETE_FILE: {
            // 解析删除请求
            if (msg->header.length != sizeof(uint64_t)) {
                result = DISTFS_ERROR_INVALID_PARAM;
                break;
            }
            
            uint64_t block_id = *(uint64_t*)msg->payload;
            result = delete_data_block(block_id);
            break;
        }
        
        case DISTFS_MSG_NODE_STATUS: {
            // 返回节点状态
            update_storage_stats();
            
            distfs_node_info_t node_info = {0};
            node_info.node_id = g_storage_node.node_id;
            node_info.type = DISTFS_NODE_STORAGE;
            node_info.status = DISTFS_NODE_STATUS_ONLINE;
            node_info.capacity = g_storage_node.total_capacity;
            node_info.used = g_storage_node.used_capacity;
            node_info.last_heartbeat = distfs_get_timestamp_sec();
            strcpy(node_info.version, "1.0.0");
            
            response = distfs_message_create(DISTFS_MSG_METADATA, &node_info, sizeof(node_info));
            break;
        }
        
        case DISTFS_MSG_PING: {
            response = distfs_message_create(DISTFS_MSG_PONG, NULL, 0);
            break;
        }
        
        default:
            result = DISTFS_ERROR_INVALID_PARAM;
            break;
    }
    
    // 发送响应
    if (!response) {
        if (result == DISTFS_SUCCESS) {
            response = distfs_message_create(DISTFS_MSG_SUCCESS, NULL, 0);
        } else {
            response = distfs_message_create(DISTFS_MSG_ERROR, &result, sizeof(result));
        }
    }
    
    if (response) {
        distfs_message_send(conn, response);
        distfs_message_destroy(response);
    }
    
    return DISTFS_SUCCESS;
}

/* 心跳线程 */
static void* heartbeat_thread_func(void *arg) {
    (void)arg;  // 未使用的参数
    
    while (g_storage_node.running) {
        // 更新统计信息
        update_storage_stats();
        
        // TODO: 发送心跳到元数据服务器
        
        sleep(30);  // 30秒心跳间隔
    }
    
    return NULL;
}

/* 初始化存储节点 */
int distfs_storage_node_init(const char *data_dir, uint16_t port) {
    if (!data_dir) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    memset(&g_storage_node, 0, sizeof(g_storage_node));
    
    // 初始化互斥锁
    if (pthread_mutex_init(&g_storage_node.storage_mutex, NULL) != 0) {
        return DISTFS_ERROR_UNKNOWN;
    }
    
    // 设置配置
    strncpy(g_storage_node.data_dir, data_dir, sizeof(g_storage_node.data_dir) - 1);
    g_storage_node.node_id = distfs_random_uint64();
    g_storage_node.block_size = DISTFS_BLOCK_SIZE;
    g_storage_node.replica_count = DISTFS_DEFAULT_REPLICAS;
    
    // 初始化存储目录
    int result = init_storage_directory(data_dir);
    if (result != DISTFS_SUCCESS) {
        pthread_mutex_destroy(&g_storage_node.storage_mutex);
        return result;
    }
    
    // 创建网络服务器
    g_storage_node.server = distfs_server_create(port, 1000);
    if (!g_storage_node.server) {
        pthread_mutex_destroy(&g_storage_node.storage_mutex);
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    // 注册消息处理器
    distfs_server_register_handler(g_storage_node.server, DISTFS_MSG_WRITE_FILE, 
                                   handle_storage_request, NULL);
    distfs_server_register_handler(g_storage_node.server, DISTFS_MSG_READ_FILE, 
                                   handle_storage_request, NULL);
    distfs_server_register_handler(g_storage_node.server, DISTFS_MSG_DELETE_FILE, 
                                   handle_storage_request, NULL);
    distfs_server_register_handler(g_storage_node.server, DISTFS_MSG_NODE_STATUS, 
                                   handle_storage_request, NULL);
    distfs_server_register_handler(g_storage_node.server, DISTFS_MSG_PING, 
                                   handle_storage_request, NULL);
    
    // 更新统计信息
    update_storage_stats();
    
    g_storage_node.running = true;
    
    return DISTFS_SUCCESS;
}

/* 启动存储节点 */
int distfs_storage_node_start(void) {
    if (!g_storage_node.server) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    // 启动网络服务器
    int result = distfs_server_start(g_storage_node.server);
    if (result != DISTFS_SUCCESS) {
        return result;
    }
    
    // 启动心跳线程
    if (pthread_create(&g_storage_node.heartbeat_thread, NULL, 
                       heartbeat_thread_func, NULL) != 0) {
        distfs_server_stop(g_storage_node.server);
        return DISTFS_ERROR_UNKNOWN;
    }
    
    return DISTFS_SUCCESS;
}

/* 停止存储节点 */
int distfs_storage_node_stop(void) {
    g_storage_node.running = false;
    
    // 停止心跳线程
    if (g_storage_node.heartbeat_thread) {
        pthread_join(g_storage_node.heartbeat_thread, NULL);
    }
    
    // 停止网络服务器
    if (g_storage_node.server) {
        distfs_server_stop(g_storage_node.server);
    }
    
    return DISTFS_SUCCESS;
}

/* 清理存储节点 */
void distfs_storage_node_cleanup(void) {
    distfs_storage_node_stop();
    
    if (g_storage_node.server) {
        distfs_server_destroy(g_storage_node.server);
        g_storage_node.server = NULL;
    }
    
    pthread_mutex_destroy(&g_storage_node.storage_mutex);
    
    memset(&g_storage_node, 0, sizeof(g_storage_node));
}
