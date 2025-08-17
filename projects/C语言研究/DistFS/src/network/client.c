/*
 * DistFS 网络客户端实现
 * 提供网络客户端连接和通信功能
 */

#define _GNU_SOURCE
#include "distfs.h"
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>

/* 客户端连接结构 */
struct distfs_connection {
    int fd;
    struct sockaddr_in addr;
    
    /* 连接状态 */
    bool connected;
    bool blocking;
    
    /* 超时设置 */
    int connect_timeout;
    int send_timeout;
    int recv_timeout;
    
    /* 缓冲区 */
    char *send_buffer;
    size_t send_buffer_size;
    char *recv_buffer;
    size_t recv_buffer_size;
    
    /* 统计信息 */
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t messages_sent;
    uint64_t messages_received;
    time_t connected_time;
    time_t last_activity;
    
    pthread_mutex_t mutex;
};

/* 设置socket超时 */
static int set_socket_timeout(int fd, int timeout_ms, int option) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    return setsockopt(fd, SOL_SOCKET, option, &tv, sizeof(tv));
}

/* 设置socket为非阻塞模式 */
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    
    flags |= O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags);
}

/* 解析主机名和端口 */
static int resolve_address(const char *hostname, uint16_t port, struct sockaddr_in *addr) {
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    
    // 尝试直接解析IP地址
    if (inet_aton(hostname, &addr->sin_addr) == 1) {
        return 0;
    }
    
    // 使用DNS解析主机名
    struct hostent *host = gethostbyname(hostname);
    if (!host) {
        return -1;
    }
    
    memcpy(&addr->sin_addr, host->h_addr_list[0], host->h_length);
    return 0;
}

/* 创建连接 */
distfs_connection_t* distfs_connection_create(const char *hostname, uint16_t port) {
    if (!hostname || port == 0) {
        return NULL;
    }
    
    distfs_connection_t *conn = calloc(1, sizeof(distfs_connection_t));
    if (!conn) {
        return NULL;
    }
    
    conn->fd = -1;
    conn->connected = false;
    conn->blocking = true;
    conn->connect_timeout = 10000; // 10秒
    conn->send_timeout = 5000;     // 5秒
    conn->recv_timeout = 5000;     // 5秒
    
    // 分配缓冲区
    conn->send_buffer_size = 8192;
    conn->send_buffer = malloc(conn->send_buffer_size);
    if (!conn->send_buffer) {
        free(conn);
        return NULL;
    }
    
    conn->recv_buffer_size = 8192;
    conn->recv_buffer = malloc(conn->recv_buffer_size);
    if (!conn->recv_buffer) {
        free(conn->send_buffer);
        free(conn);
        return NULL;
    }
    
    // 初始化互斥锁
    if (pthread_mutex_init(&conn->mutex, NULL) != 0) {
        free(conn->recv_buffer);
        free(conn->send_buffer);
        free(conn);
        return NULL;
    }
    
    // 解析地址
    if (resolve_address(hostname, port, &conn->addr) != 0) {
        DISTFS_LOG_ERROR("Failed to resolve address %s:%d", hostname, port);
        pthread_mutex_destroy(&conn->mutex);
        free(conn->recv_buffer);
        free(conn->send_buffer);
        free(conn);
        return NULL;
    }
    
    return conn;
}

/* 连接到服务器 */
int distfs_connection_connect(distfs_connection_t *conn) {
    if (!conn || conn->connected) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&conn->mutex);
    
    // 创建socket
    conn->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->fd < 0) {
        DISTFS_LOG_ERROR("Failed to create socket: %s", strerror(errno));
        pthread_mutex_unlock(&conn->mutex);
        return DISTFS_ERROR_SYSTEM_ERROR;
    }
    
    // 设置超时
    if (set_socket_timeout(conn->fd, conn->send_timeout, SO_SNDTIMEO) != 0 ||
        set_socket_timeout(conn->fd, conn->recv_timeout, SO_RCVTIMEO) != 0) {
        DISTFS_LOG_WARN("Failed to set socket timeouts");
    }
    
    // 连接到服务器
    if (connect(conn->fd, (struct sockaddr*)&conn->addr, sizeof(conn->addr)) < 0) {
        DISTFS_LOG_ERROR("Failed to connect to %s:%d: %s",
                        inet_ntoa(conn->addr.sin_addr), 
                        ntohs(conn->addr.sin_port),
                        strerror(errno));
        close(conn->fd);
        conn->fd = -1;
        pthread_mutex_unlock(&conn->mutex);
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    conn->connected = true;
    conn->connected_time = time(NULL);
    conn->last_activity = conn->connected_time;
    
    pthread_mutex_unlock(&conn->mutex);
    
    DISTFS_LOG_DEBUG("Connected to %s:%d", 
                    inet_ntoa(conn->addr.sin_addr), 
                    ntohs(conn->addr.sin_port));
    
    return DISTFS_SUCCESS;
}

/* 断开连接 */
int distfs_connection_disconnect(distfs_connection_t *conn) {
    if (!conn) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&conn->mutex);
    
    if (conn->fd >= 0) {
        close(conn->fd);
        conn->fd = -1;
    }
    
    conn->connected = false;
    
    pthread_mutex_unlock(&conn->mutex);
    
    return DISTFS_SUCCESS;
}

/* 销毁连接 */
void distfs_connection_destroy(distfs_connection_t *conn) {
    if (!conn) return;
    
    distfs_connection_disconnect(conn);
    
    if (conn->send_buffer) {
        free(conn->send_buffer);
    }
    
    if (conn->recv_buffer) {
        free(conn->recv_buffer);
    }
    
    pthread_mutex_destroy(&conn->mutex);
    free(conn);
}

/* 发送完整数据 */
static int send_all(int fd, const void *data, size_t len) {
    const char *ptr = (const char*)data;
    size_t remaining = len;
    
    while (remaining > 0) {
        ssize_t sent = send(fd, ptr, remaining, 0);
        if (sent <= 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        
        ptr += sent;
        remaining -= sent;
    }
    
    return 0;
}

/* 接收完整数据 */
static int recv_all(int fd, void *data, size_t len) {
    char *ptr = (char*)data;
    size_t remaining = len;
    
    while (remaining > 0) {
        ssize_t received = recv(fd, ptr, remaining, 0);
        if (received <= 0) {
            if (received == 0) {
                return -1; // 连接关闭
            }
            if (errno == EINTR) continue;
            return -1;
        }
        
        ptr += received;
        remaining -= received;
    }
    
    return 0;
}

/* 发送消息 */
int distfs_message_send(distfs_connection_t *conn, const distfs_message_t *message) {
    if (!conn || !message || !conn->connected) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&conn->mutex);
    
    // 发送消息头
    if (send_all(conn->fd, &message->header, sizeof(message->header)) != 0) {
        DISTFS_LOG_ERROR("Failed to send message header: %s", strerror(errno));
        pthread_mutex_unlock(&conn->mutex);
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    // 发送payload（如果有）
    if (message->header.length > 0 && message->payload) {
        if (send_all(conn->fd, message->payload, message->header.length) != 0) {
            DISTFS_LOG_ERROR("Failed to send message payload: %s", strerror(errno));
            pthread_mutex_unlock(&conn->mutex);
            return DISTFS_ERROR_NETWORK_FAILURE;
        }
    }
    
    conn->bytes_sent += sizeof(message->header) + message->header.length;
    conn->messages_sent++;
    conn->last_activity = time(NULL);
    
    pthread_mutex_unlock(&conn->mutex);
    
    return DISTFS_SUCCESS;
}

/* 接收消息 */
distfs_message_t* distfs_message_receive(distfs_connection_t *conn) {
    if (!conn || !conn->connected) {
        return NULL;
    }
    
    pthread_mutex_lock(&conn->mutex);
    
    // 接收消息头
    distfs_msg_header_t header;
    if (recv_all(conn->fd, &header, sizeof(header)) != 0) {
        DISTFS_LOG_ERROR("Failed to receive message header: %s", strerror(errno));
        pthread_mutex_unlock(&conn->mutex);
        return NULL;
    }
    
    // 验证魔数
    if (header.magic != DISTFS_PROTOCOL_MAGIC) {
        DISTFS_LOG_ERROR("Invalid message magic: 0x%08x", header.magic);
        pthread_mutex_unlock(&conn->mutex);
        return NULL;
    }
    
    // 创建消息对象
    distfs_message_t *message = calloc(1, sizeof(distfs_message_t));
    if (!message) {
        pthread_mutex_unlock(&conn->mutex);
        return NULL;
    }
    
    message->header = header;
    
    // 接收payload（如果有）
    if (header.length > 0) {
        message->payload = malloc(header.length);
        if (!message->payload) {
            free(message);
            pthread_mutex_unlock(&conn->mutex);
            return NULL;
        }
        
        if (recv_all(conn->fd, message->payload, header.length) != 0) {
            DISTFS_LOG_ERROR("Failed to receive message payload: %s", strerror(errno));
            distfs_message_destroy(message);
            pthread_mutex_unlock(&conn->mutex);
            return NULL;
        }
    }
    
    conn->bytes_received += sizeof(header) + header.length;
    conn->messages_received++;
    conn->last_activity = time(NULL);
    
    pthread_mutex_unlock(&conn->mutex);
    
    return message;
}
