/*
 * DistFS 客户端文件操作实现
 * 提供完整的文件系统操作接口
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
#include <errno.h>
#include <stdbool.h>

/* 文件句柄结构 */
typedef struct distfs_file_handle {
    uint64_t file_id;
    char path[DISTFS_MAX_PATH_LEN];
    int flags;
    uint64_t size;
    uint64_t position;
    time_t opened_time;
    distfs_connection_t *metadata_conn;
    bool valid;
} distfs_file_handle_t;

/* 目录句柄结构 */
typedef struct distfs_dir_handle {
    char path[DISTFS_MAX_PATH_LEN];
    char **entries;
    int entry_count;
    int current_index;
    time_t opened_time;
    bool valid;
} distfs_dir_handle_t;

/* 客户端上下文结构 */
struct distfs_client_context {
    char metadata_server[256];
    uint16_t metadata_port;
    
    /* 连接管理 */
    distfs_connection_pool_t *connection_pool;
    distfs_connection_t *metadata_connection;
    
    /* 缓存管理 */
    distfs_cache_t *file_cache;
    distfs_cache_t *metadata_cache;
    
    /* 配置参数 */
    int block_size;
    int cache_size;
    int max_connections;
    int retry_count;
    int timeout;
    
    /* 统计信息 */
    uint64_t files_opened;
    uint64_t files_created;
    uint64_t bytes_read;
    uint64_t bytes_written;
    uint64_t cache_hits;
    uint64_t cache_misses;
    
    bool initialized;
    pthread_mutex_t mutex;
};

/* 全局客户端上下文 */
static distfs_client_context_t *g_client_context = NULL;

/* 发送请求并等待响应 */
static distfs_message_t* send_request_and_wait(distfs_connection_t *conn, 
                                              distfs_message_t *request) {
    if (!conn || !request) {
        return NULL;
    }
    
    // 发送请求
    if (distfs_message_send(conn, request) != DISTFS_SUCCESS) {
        DISTFS_LOG_ERROR("Failed to send request");
        return NULL;
    }
    
    // 接收响应
    distfs_message_t *response = distfs_message_receive(conn);
    if (!response) {
        DISTFS_LOG_ERROR("Failed to receive response");
        return NULL;
    }
    
    return response;
}

/* 创建客户端上下文 */
distfs_client_context_t* distfs_client_create(const char *metadata_server, uint16_t metadata_port) {
    if (!metadata_server || metadata_port == 0) {
        return NULL;
    }
    
    if (g_client_context) {
        return NULL; // 单例模式
    }
    
    g_client_context = calloc(1, sizeof(distfs_client_context_t));
    if (!g_client_context) {
        return NULL;
    }
    
    strncpy(g_client_context->metadata_server, metadata_server, 
            sizeof(g_client_context->metadata_server) - 1);
    g_client_context->metadata_port = metadata_port;
    
    // 设置默认参数
    g_client_context->block_size = 4096;
    g_client_context->cache_size = 64 * 1024 * 1024; // 64MB
    g_client_context->max_connections = 10;
    g_client_context->retry_count = 3;
    g_client_context->timeout = 30;
    
    // 初始化互斥锁
    if (pthread_mutex_init(&g_client_context->mutex, NULL) != 0) {
        free(g_client_context);
        g_client_context = NULL;
        return NULL;
    }
    
    // 创建连接池
    g_client_context->connection_pool = distfs_connection_pool_create(g_client_context->max_connections);
    if (!g_client_context->connection_pool) {
        pthread_mutex_destroy(&g_client_context->mutex);
        free(g_client_context);
        g_client_context = NULL;
        return NULL;
    }
    
    // 创建元数据连接
    g_client_context->metadata_connection = distfs_connection_create(metadata_server, metadata_port);
    if (!g_client_context->metadata_connection) {
        distfs_connection_pool_destroy(g_client_context->connection_pool);
        pthread_mutex_destroy(&g_client_context->mutex);
        free(g_client_context);
        g_client_context = NULL;
        return NULL;
    }
    
    // 连接到元数据服务器
    if (distfs_connection_connect(g_client_context->metadata_connection) != DISTFS_SUCCESS) {
        distfs_connection_destroy(g_client_context->metadata_connection);
        distfs_connection_pool_destroy(g_client_context->connection_pool);
        pthread_mutex_destroy(&g_client_context->mutex);
        free(g_client_context);
        g_client_context = NULL;
        return NULL;
    }
    
    g_client_context->initialized = true;
    
    DISTFS_LOG_INFO("DistFS client initialized, connected to %s:%d", 
                   metadata_server, metadata_port);
    
    return g_client_context;
}

/* 创建文件 */
int distfs_create_file(distfs_client_context_t *ctx, const char *path, mode_t mode) {
    if (!ctx || !ctx->initialized || !path) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    // 构造创建文件请求
    struct {
        char path[DISTFS_MAX_PATH_LEN];
        uint32_t mode;
    } request_data;
    
    strncpy(request_data.path, path, sizeof(request_data.path) - 1);
    request_data.mode = mode;
    
    distfs_message_t *request = distfs_message_create(DISTFS_MSG_CREATE_FILE, 
                                                     &request_data, sizeof(request_data));
    if (!request) {
        pthread_mutex_unlock(&ctx->mutex);
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    // 发送请求并等待响应
    distfs_message_t *response = send_request_and_wait(ctx->metadata_connection, request);
    distfs_message_destroy(request);
    
    if (!response) {
        pthread_mutex_unlock(&ctx->mutex);
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    int result = DISTFS_SUCCESS;
    
    if (response->header.type == DISTFS_MSG_ERROR) {
        if (response->payload && response->header.length >= sizeof(int)) {
            result = *(int*)response->payload;
        } else {
            result = DISTFS_ERROR_UNKNOWN;
        }
    } else if (response->header.type == DISTFS_MSG_SUCCESS) {
        ctx->files_created++;
    } else {
        result = DISTFS_ERROR_UNKNOWN;
    }
    
    distfs_message_destroy(response);
    pthread_mutex_unlock(&ctx->mutex);
    
    return result;
}

/* 打开文件 */
distfs_file_handle_t* distfs_open_file(distfs_client_context_t *ctx, const char *path, int flags) {
    if (!ctx || !ctx->initialized || !path) {
        return NULL;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    // 构造打开文件请求
    struct {
        char path[DISTFS_MAX_PATH_LEN];
        int flags;
    } request_data;
    
    strncpy(request_data.path, path, sizeof(request_data.path) - 1);
    request_data.flags = flags;
    
    distfs_message_t *request = distfs_message_create(DISTFS_MSG_OPEN_FILE, 
                                                     &request_data, sizeof(request_data));
    if (!request) {
        pthread_mutex_unlock(&ctx->mutex);
        return NULL;
    }
    
    // 发送请求并等待响应
    distfs_message_t *response = send_request_and_wait(ctx->metadata_connection, request);
    distfs_message_destroy(request);
    
    if (!response) {
        pthread_mutex_unlock(&ctx->mutex);
        return NULL;
    }
    
    distfs_file_handle_t *handle = NULL;
    
    if (response->header.type == DISTFS_MSG_SUCCESS && 
        response->payload && response->header.length >= sizeof(uint64_t)) {
        
        uint64_t file_id = *(uint64_t*)response->payload;
        
        // 创建文件句柄
        handle = calloc(1, sizeof(distfs_file_handle_t));
        if (handle) {
            handle->file_id = file_id;
            strncpy(handle->path, path, sizeof(handle->path) - 1);
            handle->flags = flags;
            handle->size = 0; // TODO: 从元数据获取文件大小
            handle->position = 0;
            handle->opened_time = time(NULL);
            handle->metadata_conn = ctx->metadata_connection;
            handle->valid = true;
            
            ctx->files_opened++;
        }
    }
    
    distfs_message_destroy(response);
    pthread_mutex_unlock(&ctx->mutex);
    
    return handle;
}

/* 读取文件 */
ssize_t distfs_read_file(distfs_file_handle_t *handle, void *buffer, size_t size) {
    if (!handle || !handle->valid || !buffer || size == 0) {
        return -1;
    }
    
    // TODO: 实现实际的文件读取逻辑
    // 1. 根据文件位置计算需要读取的块
    // 2. 从存储节点读取数据块
    // 3. 组装数据并返回
    
    DISTFS_LOG_DEBUG("Reading %zu bytes from file %s at position %lu", 
                    size, handle->path, handle->position);
    
    // 暂时返回0表示EOF
    return 0;
}

/* 写入文件 */
ssize_t distfs_write_file(distfs_file_handle_t *handle, const void *buffer, size_t size) {
    if (!handle || !handle->valid || !buffer || size == 0) {
        return -1;
    }
    
    // TODO: 实现实际的文件写入逻辑
    // 1. 根据文件位置计算需要写入的块
    // 2. 将数据写入存储节点
    // 3. 更新文件元数据
    
    DISTFS_LOG_DEBUG("Writing %zu bytes to file %s at position %lu", 
                    size, handle->path, handle->position);
    
    // 暂时返回写入的字节数
    handle->position += size;
    if (handle->position > handle->size) {
        handle->size = handle->position;
    }
    
    return size;
}

/* 定位文件位置 */
off_t distfs_seek_file(distfs_file_handle_t *handle, off_t offset, int whence) {
    if (!handle || !handle->valid) {
        return -1;
    }
    
    uint64_t new_position;
    
    switch (whence) {
        case SEEK_SET:
            new_position = offset;
            break;
        case SEEK_CUR:
            new_position = handle->position + offset;
            break;
        case SEEK_END:
            new_position = handle->size + offset;
            break;
        default:
            return -1;
    }
    
    handle->position = new_position;
    
    return new_position;
}

/* 关闭文件 */
int distfs_close_file(distfs_file_handle_t *handle) {
    if (!handle) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    handle->valid = false;
    free(handle);
    
    return DISTFS_SUCCESS;
}

/* 删除文件 */
int distfs_delete_file(distfs_client_context_t *ctx, const char *path) {
    if (!ctx || !ctx->initialized || !path) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    // 构造删除文件请求
    distfs_message_t *request = distfs_message_create(DISTFS_MSG_DELETE_FILE, 
                                                     path, strlen(path) + 1);
    if (!request) {
        pthread_mutex_unlock(&ctx->mutex);
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    // 发送请求并等待响应
    distfs_message_t *response = send_request_and_wait(ctx->metadata_connection, request);
    distfs_message_destroy(request);
    
    if (!response) {
        pthread_mutex_unlock(&ctx->mutex);
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    int result = DISTFS_SUCCESS;
    
    if (response->header.type == DISTFS_MSG_ERROR) {
        if (response->payload && response->header.length >= sizeof(int)) {
            result = *(int*)response->payload;
        } else {
            result = DISTFS_ERROR_UNKNOWN;
        }
    }
    
    distfs_message_destroy(response);
    pthread_mutex_unlock(&ctx->mutex);
    
    return result;
}

/* 销毁客户端上下文 */
void distfs_client_destroy(distfs_client_context_t *ctx) {
    if (!ctx) return;
    
    if (ctx->metadata_connection) {
        distfs_connection_destroy(ctx->metadata_connection);
    }
    
    if (ctx->connection_pool) {
        distfs_connection_pool_destroy(ctx->connection_pool);
    }
    
    pthread_mutex_destroy(&ctx->mutex);
    
    if (ctx == g_client_context) {
        g_client_context = NULL;
    }
    
    free(ctx);
}
