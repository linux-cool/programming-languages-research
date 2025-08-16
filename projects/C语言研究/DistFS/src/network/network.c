/*
 * DistFS 网络通信模块实现
 * 提供高性能的网络通信功能
 */

#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <signal.h>

#define DISTFS_MAGIC 0x44495354  // "DIST"
#define EPOLL_MAX_EVENTS 1000
#define WORKER_THREAD_STACK_SIZE (2 * 1024 * 1024)  // 2MB

/* 全局变量 */
static uint32_t g_next_sequence = 1;
static pthread_mutex_t g_sequence_mutex = PTHREAD_MUTEX_INITIALIZER;

/* 获取下一个序列号 */
static uint32_t get_next_sequence(void) {
    pthread_mutex_lock(&g_sequence_mutex);
    uint32_t seq = g_next_sequence++;
    pthread_mutex_unlock(&g_sequence_mutex);
    return seq;
}

/* 设置socket为非阻塞模式 */
int distfs_connection_set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    return DISTFS_SUCCESS;
}

/* 设置socket的keepalive选项 */
int distfs_connection_set_keepalive(int sockfd) {
    int keepalive = 1;
    int keepidle = 60;   // 60秒后开始发送keepalive
    int keepintvl = 10;  // 每10秒发送一次
    int keepcnt = 3;     // 最多发送3次
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0) {
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle)) < 0) {
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl)) < 0) {
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt)) < 0) {
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    return DISTFS_SUCCESS;
}

/* 设置socket超时 */
int distfs_connection_set_timeout(int sockfd, int timeout) {
    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    return DISTFS_SUCCESS;
}

/* 创建连接对象 */
distfs_connection_t* distfs_connection_create(int sockfd, struct sockaddr_in *addr) {
    distfs_connection_t *conn = calloc(1, sizeof(distfs_connection_t));
    if (!conn) {
        return NULL;
    }
    
    conn->sockfd = sockfd;
    if (addr) {
        conn->addr = *addr;
    }
    conn->state = DISTFS_CONN_CONNECTED;
    conn->last_activity = distfs_get_timestamp();
    
    if (pthread_mutex_init(&conn->send_mutex, NULL) != 0) {
        free(conn);
        return NULL;
    }
    
    if (pthread_mutex_init(&conn->recv_mutex, NULL) != 0) {
        pthread_mutex_destroy(&conn->send_mutex);
        free(conn);
        return NULL;
    }
    
    return conn;
}

/* 销毁连接对象 */
void distfs_connection_destroy(distfs_connection_t *conn) {
    if (!conn) return;
    
    if (conn->sockfd >= 0) {
        close(conn->sockfd);
    }
    
    pthread_mutex_destroy(&conn->send_mutex);
    pthread_mutex_destroy(&conn->recv_mutex);
    free(conn);
}

/* 创建消息对象 */
distfs_message_t* distfs_message_create(distfs_msg_type_t type, 
                                        const void *payload, uint32_t length) {
    distfs_message_t *msg = calloc(1, sizeof(distfs_message_t));
    if (!msg) {
        return NULL;
    }
    
    // 设置消息头
    msg->header.magic = DISTFS_MAGIC;
    msg->header.version = DISTFS_PROTOCOL_VERSION;
    msg->header.type = type;
    msg->header.flags = DISTFS_MSG_FLAG_NONE;
    msg->header.length = length;
    msg->header.sequence = get_next_sequence();
    
    // 分配并复制payload
    if (length > 0 && payload) {
        msg->payload = malloc(length);
        if (!msg->payload) {
            free(msg);
            return NULL;
        }
        memcpy(msg->payload, payload, length);
    }
    
    // 计算校验和
    msg->header.checksum = distfs_calculate_message_checksum(msg);
    
    return msg;
}

/* 销毁消息对象 */
void distfs_message_destroy(distfs_message_t *msg) {
    if (!msg) return;
    
    if (msg->payload) {
        free(msg->payload);
    }
    free(msg);
}

/* 计算消息校验和 */
uint32_t distfs_calculate_message_checksum(const distfs_message_t *msg) {
    if (!msg) return 0;
    
    uint32_t checksum = 0;
    
    // 计算头部校验和(除了checksum字段)
    checksum ^= msg->header.magic;
    checksum ^= msg->header.version;
    checksum ^= msg->header.type;
    checksum ^= msg->header.flags;
    checksum ^= msg->header.length;
    checksum ^= msg->header.sequence;
    
    // 计算payload校验和
    if (msg->payload && msg->header.length > 0) {
        checksum ^= distfs_calculate_checksum(msg->payload, msg->header.length);
    }
    
    return checksum;
}

/* 验证消息 */
int distfs_validate_message(const distfs_message_t *msg) {
    if (!msg) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    // 检查魔数
    if (msg->header.magic != DISTFS_MAGIC) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    // 检查版本
    if (msg->header.version != DISTFS_PROTOCOL_VERSION) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    // 检查长度
    if (msg->header.length > DISTFS_MAX_MESSAGE_SIZE) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    // 检查payload
    if (msg->header.length > 0 && !msg->payload) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    // 验证校验和
    uint32_t calculated_checksum = distfs_calculate_message_checksum(msg);
    if (calculated_checksum != msg->header.checksum) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    return DISTFS_SUCCESS;
}

/* 发送完整数据 */
static ssize_t send_all(int sockfd, const void *buf, size_t len) {
    const uint8_t *ptr = (const uint8_t *)buf;
    size_t total_sent = 0;
    
    while (total_sent < len) {
        ssize_t sent = send(sockfd, ptr + total_sent, len - total_sent, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;  // 重试
            }
            return -1;
        }
        if (sent == 0) {
            return -1;  // 连接关闭
        }
        total_sent += sent;
    }
    
    return total_sent;
}

/* 接收完整数据 */
static ssize_t recv_all(int sockfd, void *buf, size_t len) {
    uint8_t *ptr = (uint8_t *)buf;
    size_t total_received = 0;
    
    while (total_received < len) {
        ssize_t received = recv(sockfd, ptr + total_received, len - total_received, 0);
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;  // 重试
            }
            return -1;
        }
        if (received == 0) {
            return -1;  // 连接关闭
        }
        total_received += received;
    }
    
    return total_received;
}

/* 发送消息 */
int distfs_message_send(distfs_connection_t *conn, distfs_message_t *msg) {
    if (!conn || !msg) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    if (conn->state != DISTFS_CONN_CONNECTED && 
        conn->state != DISTFS_CONN_AUTHENTICATED) {
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    pthread_mutex_lock(&conn->send_mutex);
    
    int result = DISTFS_SUCCESS;
    
    // 发送消息头
    if (send_all(conn->sockfd, &msg->header, sizeof(msg->header)) < 0) {
        result = DISTFS_ERROR_NETWORK_FAILURE;
        goto cleanup;
    }
    
    // 发送payload
    if (msg->header.length > 0 && msg->payload) {
        if (send_all(conn->sockfd, msg->payload, msg->header.length) < 0) {
            result = DISTFS_ERROR_NETWORK_FAILURE;
            goto cleanup;
        }
    }
    
    // 更新统计信息
    conn->bytes_sent += sizeof(msg->header) + msg->header.length;
    conn->messages_sent++;
    conn->last_activity = distfs_get_timestamp();
    
cleanup:
    pthread_mutex_unlock(&conn->send_mutex);
    return result;
}

/* 接收消息 */
distfs_message_t* distfs_message_receive(distfs_connection_t *conn) {
    if (!conn) {
        return NULL;
    }
    
    if (conn->state != DISTFS_CONN_CONNECTED && 
        conn->state != DISTFS_CONN_AUTHENTICATED) {
        return NULL;
    }
    
    pthread_mutex_lock(&conn->recv_mutex);
    
    distfs_message_t *msg = NULL;
    
    // 接收消息头
    distfs_msg_header_t header;
    if (recv_all(conn->sockfd, &header, sizeof(header)) < 0) {
        goto cleanup;
    }
    
    // 验证消息头
    if (header.magic != DISTFS_MAGIC || 
        header.version != DISTFS_PROTOCOL_VERSION ||
        header.length > DISTFS_MAX_MESSAGE_SIZE) {
        goto cleanup;
    }
    
    // 创建消息对象
    msg = calloc(1, sizeof(distfs_message_t));
    if (!msg) {
        goto cleanup;
    }
    
    msg->header = header;
    
    // 接收payload
    if (header.length > 0) {
        msg->payload = malloc(header.length);
        if (!msg->payload) {
            free(msg);
            msg = NULL;
            goto cleanup;
        }
        
        if (recv_all(conn->sockfd, msg->payload, header.length) < 0) {
            free(msg->payload);
            free(msg);
            msg = NULL;
            goto cleanup;
        }
    }
    
    // 验证消息
    if (distfs_validate_message(msg) != DISTFS_SUCCESS) {
        distfs_message_destroy(msg);
        msg = NULL;
        goto cleanup;
    }
    
    // 更新统计信息
    conn->bytes_received += sizeof(header) + header.length;
    conn->messages_received++;
    conn->last_activity = distfs_get_timestamp();
    
cleanup:
    pthread_mutex_unlock(&conn->recv_mutex);
    return msg;
}

/* 同步发送消息并等待响应 */
int distfs_message_send_sync(distfs_connection_t *conn, distfs_message_t *request,
                             distfs_message_t **response, int timeout) {
    if (!conn || !request || !response) {
        return DISTFS_ERROR_INVALID_PARAM;
    }

    *response = NULL;

    // 发送请求
    int result = distfs_message_send(conn, request);
    if (result != DISTFS_SUCCESS) {
        return result;
    }

    // 设置超时
    if (timeout > 0) {
        distfs_connection_set_timeout(conn->sockfd, timeout);
    }

    // 接收响应
    *response = distfs_message_receive(conn);
    if (!*response) {
        return DISTFS_ERROR_TIMEOUT;
    }

    return DISTFS_SUCCESS;
}

/* 消息类型转字符串 */
const char* distfs_msg_type_to_string(distfs_msg_type_t type) {
    switch (type) {
        case DISTFS_MSG_CREATE_FILE: return "CREATE_FILE";
        case DISTFS_MSG_OPEN_FILE: return "OPEN_FILE";
        case DISTFS_MSG_READ_FILE: return "READ_FILE";
        case DISTFS_MSG_WRITE_FILE: return "WRITE_FILE";
        case DISTFS_MSG_CLOSE_FILE: return "CLOSE_FILE";
        case DISTFS_MSG_DELETE_FILE: return "DELETE_FILE";
        case DISTFS_MSG_CREATE_DIR: return "CREATE_DIR";
        case DISTFS_MSG_DELETE_DIR: return "DELETE_DIR";
        case DISTFS_MSG_LIST_DIR: return "LIST_DIR";
        case DISTFS_MSG_GET_STAT: return "GET_STAT";
        case DISTFS_MSG_SET_ATTR: return "SET_ATTR";
        case DISTFS_MSG_JOIN_CLUSTER: return "JOIN_CLUSTER";
        case DISTFS_MSG_LEAVE_CLUSTER: return "LEAVE_CLUSTER";
        case DISTFS_MSG_HEARTBEAT: return "HEARTBEAT";
        case DISTFS_MSG_NODE_STATUS: return "NODE_STATUS";
        case DISTFS_MSG_CLUSTER_INFO: return "CLUSTER_INFO";
        case DISTFS_MSG_REPLICATE_DATA: return "REPLICATE_DATA";
        case DISTFS_MSG_SYNC_METADATA: return "SYNC_METADATA";
        case DISTFS_MSG_REPAIR_DATA: return "REPAIR_DATA";
        case DISTFS_MSG_MIGRATE_DATA: return "MIGRATE_DATA";
        case DISTFS_MSG_SUCCESS: return "SUCCESS";
        case DISTFS_MSG_ERROR: return "ERROR";
        case DISTFS_MSG_DATA: return "DATA";
        case DISTFS_MSG_METADATA: return "METADATA";
        case DISTFS_MSG_PING: return "PING";
        case DISTFS_MSG_PONG: return "PONG";
        default: return "UNKNOWN";
    }
}
