/*
 * DistFS 客户端API实现
 * 提供分布式文件系统的客户端接口
 */

#include "distfs.h"
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* 客户端结构定义 */
struct distfs_client {
    distfs_config_t config;
    distfs_network_client_t *network;
    pthread_mutex_t client_mutex;
    bool initialized;
    
    /* 缓存和统计 */
    uint64_t total_reads;
    uint64_t total_writes;
    uint64_t bytes_read;
    uint64_t bytes_written;
};

/* 默认配置 */
static const distfs_config_t default_config = {
    .config_file = "",
    .listen_port = 9527,
    .max_connections = 1000,
    .thread_pool_size = 8,
    .replica_count = 3,
    .block_size = DISTFS_BLOCK_SIZE,
    .heartbeat_interval = 30,
    .timeout = 60,
    .data_dir = "/tmp/distfs",
    .log_file = "/tmp/distfs.log",
    .log_level = 2,
    .enable_compression = false,
    .enable_encryption = false
};

/* 加载配置文件 */
static int load_config(const char *config_file, distfs_config_t *config) {
    if (!config) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    // 使用默认配置
    *config = default_config;
    
    if (!config_file || strlen(config_file) == 0) {
        return DISTFS_SUCCESS;
    }
    
    FILE *fp = fopen(config_file, "r");
    if (!fp) {
        return DISTFS_ERROR_FILE_NOT_FOUND;
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        // 跳过注释和空行
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        
        char key[256], value[256];
        if (sscanf(line, "%255s = %255s", key, value) == 2) {
            if (strcmp(key, "listen_port") == 0) {
                config->listen_port = atoi(value);
            } else if (strcmp(key, "max_connections") == 0) {
                config->max_connections = atoi(value);
            } else if (strcmp(key, "thread_pool_size") == 0) {
                config->thread_pool_size = atoi(value);
            } else if (strcmp(key, "replica_count") == 0) {
                config->replica_count = atoi(value);
            } else if (strcmp(key, "block_size") == 0) {
                config->block_size = atoi(value);
            } else if (strcmp(key, "heartbeat_interval") == 0) {
                config->heartbeat_interval = atoi(value);
            } else if (strcmp(key, "timeout") == 0) {
                config->timeout = atoi(value);
            } else if (strcmp(key, "data_dir") == 0) {
                strncpy(config->data_dir, value, sizeof(config->data_dir) - 1);
            } else if (strcmp(key, "log_file") == 0) {
                strncpy(config->log_file, value, sizeof(config->log_file) - 1);
            } else if (strcmp(key, "log_level") == 0) {
                config->log_level = atoi(value);
            } else if (strcmp(key, "enable_compression") == 0) {
                config->enable_compression = (strcmp(value, "true") == 0);
            } else if (strcmp(key, "enable_encryption") == 0) {
                config->enable_encryption = (strcmp(value, "true") == 0);
            }
        }
    }
    
    fclose(fp);
    return DISTFS_SUCCESS;
}

/* 初始化客户端 */
distfs_client_t* distfs_init(const char *config_file) {
    distfs_client_t *client = calloc(1, sizeof(distfs_client_t));
    if (!client) {
        return NULL;
    }
    
    // 加载配置
    if (load_config(config_file, &client->config) != DISTFS_SUCCESS) {
        free(client);
        return NULL;
    }
    
    // 初始化互斥锁
    if (pthread_mutex_init(&client->client_mutex, NULL) != 0) {
        free(client);
        return NULL;
    }
    
    // 创建网络客户端
    client->network = distfs_network_client_create();
    if (!client->network) {
        pthread_mutex_destroy(&client->client_mutex);
        free(client);
        return NULL;
    }
    
    // 连接到元数据服务器
    if (distfs_network_client_connect_metadata(client->network, "127.0.0.1", 9527) != DISTFS_SUCCESS) {
        distfs_network_client_destroy(client->network);
        pthread_mutex_destroy(&client->client_mutex);
        free(client);
        return NULL;
    }
    
    client->initialized = true;
    return client;
}

/* 清理客户端 */
int distfs_cleanup(distfs_client_t *client) {
    if (!client) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&client->client_mutex);
    
    if (client->network) {
        distfs_network_client_disconnect(client->network);
        distfs_network_client_destroy(client->network);
    }
    
    client->initialized = false;
    
    pthread_mutex_unlock(&client->client_mutex);
    pthread_mutex_destroy(&client->client_mutex);
    
    free(client);
    return DISTFS_SUCCESS;
}

/* 创建文件 */
int distfs_create(distfs_client_t *client, const char *path, mode_t mode) {
    if (!client || !path) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    if (!client->initialized) {
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    pthread_mutex_lock(&client->client_mutex);
    
    // 构造创建文件请求
    struct {
        char path[DISTFS_MAX_PATH_LEN];
        uint32_t mode;
    } request;
    
    strncpy(request.path, path, sizeof(request.path) - 1);
    request.path[sizeof(request.path) - 1] = '\0';
    request.mode = mode;
    
    distfs_message_t *req_msg = distfs_message_create(DISTFS_MSG_CREATE_FILE, 
                                                      &request, sizeof(request));
    if (!req_msg) {
        pthread_mutex_unlock(&client->client_mutex);
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    distfs_message_t *resp_msg = NULL;
    int result = distfs_message_send_sync(client->network->metadata_conn, 
                                          req_msg, &resp_msg, client->config.timeout);
    
    distfs_message_destroy(req_msg);
    
    if (result != DISTFS_SUCCESS) {
        pthread_mutex_unlock(&client->client_mutex);
        return result;
    }
    
    // 检查响应
    if (resp_msg->header.type == DISTFS_MSG_SUCCESS) {
        result = DISTFS_SUCCESS;
    } else if (resp_msg->header.type == DISTFS_MSG_ERROR) {
        result = DISTFS_ERROR_UNKNOWN;
        if (resp_msg->payload && resp_msg->header.length >= sizeof(int)) {
            result = *(int*)resp_msg->payload;
        }
    } else {
        result = DISTFS_ERROR_UNKNOWN;
    }
    
    distfs_message_destroy(resp_msg);
    pthread_mutex_unlock(&client->client_mutex);
    
    return result;
}

/* 打开文件 */
distfs_file_t* distfs_open(distfs_client_t *client, const char *path, int flags) {
    if (!client || !path) {
        return NULL;
    }
    
    if (!client->initialized) {
        return NULL;
    }
    
    pthread_mutex_lock(&client->client_mutex);
    
    // 构造打开文件请求
    struct {
        char path[DISTFS_MAX_PATH_LEN];
        int flags;
    } request;
    
    strncpy(request.path, path, sizeof(request.path) - 1);
    request.path[sizeof(request.path) - 1] = '\0';
    request.flags = flags;
    
    distfs_message_t *req_msg = distfs_message_create(DISTFS_MSG_OPEN_FILE, 
                                                      &request, sizeof(request));
    if (!req_msg) {
        pthread_mutex_unlock(&client->client_mutex);
        return NULL;
    }
    
    distfs_message_t *resp_msg = NULL;
    int result = distfs_message_send_sync(client->network->metadata_conn, 
                                          req_msg, &resp_msg, client->config.timeout);
    
    distfs_message_destroy(req_msg);
    
    if (result != DISTFS_SUCCESS) {
        pthread_mutex_unlock(&client->client_mutex);
        return NULL;
    }
    
    distfs_file_t *file = NULL;
    
    // 检查响应
    if (resp_msg->header.type == DISTFS_MSG_SUCCESS && 
        resp_msg->payload && resp_msg->header.length >= sizeof(uint64_t)) {
        
        uint64_t inode = *(uint64_t*)resp_msg->payload;
        
        file = calloc(1, sizeof(distfs_file_t));
        if (file) {
            file->fd = -1;  // 分布式文件系统不使用本地fd
            file->inode = inode;
            file->flags = flags;
            file->offset = 0;
            file->client = client;
        }
    }
    
    distfs_message_destroy(resp_msg);
    pthread_mutex_unlock(&client->client_mutex);
    
    return file;
}

/* 关闭文件 */
int distfs_close(distfs_file_t *file) {
    if (!file) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    // 在分布式文件系统中，关闭操作主要是释放本地资源
    // 实际的文件状态由服务器维护
    
    free(file);
    return DISTFS_SUCCESS;
}

/* 删除文件 */
int distfs_unlink(distfs_client_t *client, const char *path) {
    if (!client || !path) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    if (!client->initialized) {
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    pthread_mutex_lock(&client->client_mutex);
    
    // 构造删除文件请求
    char request[DISTFS_MAX_PATH_LEN];
    strncpy(request, path, sizeof(request) - 1);
    request[sizeof(request) - 1] = '\0';
    
    distfs_message_t *req_msg = distfs_message_create(DISTFS_MSG_DELETE_FILE, 
                                                      request, strlen(request) + 1);
    if (!req_msg) {
        pthread_mutex_unlock(&client->client_mutex);
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    distfs_message_t *resp_msg = NULL;
    int result = distfs_message_send_sync(client->network->metadata_conn, 
                                          req_msg, &resp_msg, client->config.timeout);
    
    distfs_message_destroy(req_msg);
    
    if (result != DISTFS_SUCCESS) {
        pthread_mutex_unlock(&client->client_mutex);
        return result;
    }
    
    // 检查响应
    if (resp_msg->header.type == DISTFS_MSG_SUCCESS) {
        result = DISTFS_SUCCESS;
    } else if (resp_msg->header.type == DISTFS_MSG_ERROR) {
        result = DISTFS_ERROR_UNKNOWN;
        if (resp_msg->payload && resp_msg->header.length >= sizeof(int)) {
            result = *(int*)resp_msg->payload;
        }
    } else {
        result = DISTFS_ERROR_UNKNOWN;
    }
    
    distfs_message_destroy(resp_msg);
    pthread_mutex_unlock(&client->client_mutex);
    
    return result;
}
