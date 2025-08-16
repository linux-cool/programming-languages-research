/*
 * DistFS 网络客户端实现
 * 提供客户端网络连接和通信功能
 */

#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/* 创建网络客户端 */
distfs_network_client_t* distfs_network_client_create(void) {
    distfs_network_client_t *client = calloc(1, sizeof(distfs_network_client_t));
    if (!client) {
        return NULL;
    }
    
    if (pthread_mutex_init(&client->client_mutex, NULL) != 0) {
        free(client);
        return NULL;
    }
    
    client->next_sequence = 1;
    client->storage_conn_count = 0;
    client->storage_conns = NULL;
    client->metadata_conn = NULL;
    
    return client;
}

/* 销毁网络客户端 */
void distfs_network_client_destroy(distfs_network_client_t *client) {
    if (!client) return;
    
    pthread_mutex_lock(&client->client_mutex);
    
    // 关闭元数据连接
    if (client->metadata_conn) {
        distfs_connection_destroy(client->metadata_conn);
        client->metadata_conn = NULL;
    }
    
    // 关闭存储连接
    if (client->storage_conns) {
        for (int i = 0; i < client->storage_conn_count; i++) {
            if (client->storage_conns[i]) {
                distfs_connection_destroy(client->storage_conns[i]);
            }
        }
        free(client->storage_conns);
        client->storage_conns = NULL;
    }
    
    client->storage_conn_count = 0;
    
    pthread_mutex_unlock(&client->client_mutex);
    pthread_mutex_destroy(&client->client_mutex);
    
    free(client);
}

/* 解析主机地址 */
static int resolve_host(const char *hostname, struct sockaddr_in *addr) {
    if (!hostname || !addr) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    
    // 尝试直接解析IP地址
    if (inet_aton(hostname, &addr->sin_addr) == 1) {
        return DISTFS_SUCCESS;
    }
    
    // 使用DNS解析主机名
    struct hostent *host_entry = gethostbyname(hostname);
    if (!host_entry) {
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    memcpy(&addr->sin_addr, host_entry->h_addr_list[0], host_entry->h_length);
    return DISTFS_SUCCESS;
}

/* 创建TCP连接 */
static int create_tcp_connection(const char *host, uint16_t port, int *sockfd) {
    if (!host || !sockfd) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    // 创建socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    // 解析地址
    struct sockaddr_in server_addr;
    int result = resolve_host(host, &server_addr);
    if (result != DISTFS_SUCCESS) {
        close(sock);
        return result;
    }
    
    server_addr.sin_port = htons(port);
    
    // 设置连接超时
    struct timeval timeout;
    timeout.tv_sec = DISTFS_CONNECT_TIMEOUT;
    timeout.tv_usec = 0;
    
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        close(sock);
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    // 连接服务器
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    // 设置socket选项
    distfs_connection_set_keepalive(sock);
    distfs_connection_set_nonblocking(sock);
    
    *sockfd = sock;
    return DISTFS_SUCCESS;
}

/* 连接到元数据服务器 */
int distfs_network_client_connect_metadata(distfs_network_client_t *client,
                                           const char *host, uint16_t port) {
    if (!client || !host) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&client->client_mutex);
    
    // 如果已经连接，先断开
    if (client->metadata_conn) {
        distfs_connection_destroy(client->metadata_conn);
        client->metadata_conn = NULL;
    }
    
    int sockfd;
    int result = create_tcp_connection(host, port, &sockfd);
    if (result != DISTFS_SUCCESS) {
        pthread_mutex_unlock(&client->client_mutex);
        return result;
    }
    
    // 创建连接对象
    struct sockaddr_in addr;
    resolve_host(host, &addr);
    addr.sin_port = htons(port);
    
    client->metadata_conn = distfs_connection_create(sockfd, &addr);
    if (!client->metadata_conn) {
        close(sockfd);
        pthread_mutex_unlock(&client->client_mutex);
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    // 发送握手消息
    distfs_message_t *handshake = distfs_message_create(DISTFS_MSG_JOIN_CLUSTER, NULL, 0);
    if (handshake) {
        result = distfs_message_send(client->metadata_conn, handshake);
        distfs_message_destroy(handshake);
        
        if (result == DISTFS_SUCCESS) {
            // 等待响应
            distfs_message_t *response = distfs_message_receive(client->metadata_conn);
            if (response) {
                if (response->header.type == DISTFS_MSG_SUCCESS) {
                    client->metadata_conn->state = DISTFS_CONN_AUTHENTICATED;
                } else {
                    result = DISTFS_ERROR_NETWORK_FAILURE;
                }
                distfs_message_destroy(response);
            } else {
                result = DISTFS_ERROR_TIMEOUT;
            }
        }
    } else {
        result = DISTFS_ERROR_NO_MEMORY;
    }
    
    if (result != DISTFS_SUCCESS) {
        distfs_connection_destroy(client->metadata_conn);
        client->metadata_conn = NULL;
    }
    
    pthread_mutex_unlock(&client->client_mutex);
    return result;
}

/* 连接到存储节点 */
int distfs_network_client_connect_storage(distfs_network_client_t *client,
                                          const char *host, uint16_t port) {
    if (!client || !host) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&client->client_mutex);
    
    int sockfd;
    int result = create_tcp_connection(host, port, &sockfd);
    if (result != DISTFS_SUCCESS) {
        pthread_mutex_unlock(&client->client_mutex);
        return result;
    }
    
    // 创建连接对象
    struct sockaddr_in addr;
    resolve_host(host, &addr);
    addr.sin_port = htons(port);
    
    distfs_connection_t *conn = distfs_connection_create(sockfd, &addr);
    if (!conn) {
        close(sockfd);
        pthread_mutex_unlock(&client->client_mutex);
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    // 扩展存储连接数组
    distfs_connection_t **new_conns = realloc(client->storage_conns,
                                              (client->storage_conn_count + 1) * sizeof(distfs_connection_t*));
    if (!new_conns) {
        distfs_connection_destroy(conn);
        pthread_mutex_unlock(&client->client_mutex);
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    client->storage_conns = new_conns;
    client->storage_conns[client->storage_conn_count] = conn;
    client->storage_conn_count++;
    
    // 发送握手消息
    distfs_message_t *handshake = distfs_message_create(DISTFS_MSG_JOIN_CLUSTER, NULL, 0);
    if (handshake) {
        result = distfs_message_send(conn, handshake);
        distfs_message_destroy(handshake);
        
        if (result == DISTFS_SUCCESS) {
            // 等待响应
            distfs_message_t *response = distfs_message_receive(conn);
            if (response) {
                if (response->header.type == DISTFS_MSG_SUCCESS) {
                    conn->state = DISTFS_CONN_AUTHENTICATED;
                } else {
                    result = DISTFS_ERROR_NETWORK_FAILURE;
                }
                distfs_message_destroy(response);
            } else {
                result = DISTFS_ERROR_TIMEOUT;
            }
        }
    } else {
        result = DISTFS_ERROR_NO_MEMORY;
    }
    
    if (result != DISTFS_SUCCESS) {
        // 移除失败的连接
        client->storage_conn_count--;
        distfs_connection_destroy(conn);
    }
    
    pthread_mutex_unlock(&client->client_mutex);
    return result;
}

/* 断开所有连接 */
int distfs_network_client_disconnect(distfs_network_client_t *client) {
    if (!client) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&client->client_mutex);
    
    // 断开元数据连接
    if (client->metadata_conn) {
        // 发送离开集群消息
        distfs_message_t *leave_msg = distfs_message_create(DISTFS_MSG_LEAVE_CLUSTER, NULL, 0);
        if (leave_msg) {
            distfs_message_send(client->metadata_conn, leave_msg);
            distfs_message_destroy(leave_msg);
        }
        
        distfs_connection_destroy(client->metadata_conn);
        client->metadata_conn = NULL;
    }
    
    // 断开存储连接
    for (int i = 0; i < client->storage_conn_count; i++) {
        if (client->storage_conns[i]) {
            // 发送离开集群消息
            distfs_message_t *leave_msg = distfs_message_create(DISTFS_MSG_LEAVE_CLUSTER, NULL, 0);
            if (leave_msg) {
                distfs_message_send(client->storage_conns[i], leave_msg);
                distfs_message_destroy(leave_msg);
            }
            
            distfs_connection_destroy(client->storage_conns[i]);
            client->storage_conns[i] = NULL;
        }
    }
    
    if (client->storage_conns) {
        free(client->storage_conns);
        client->storage_conns = NULL;
    }
    client->storage_conn_count = 0;
    
    pthread_mutex_unlock(&client->client_mutex);
    return DISTFS_SUCCESS;
}

/* 发送消息到元数据服务器 */
int distfs_network_client_send_to_metadata(distfs_network_client_t *client,
                                           distfs_message_t *message) {
    if (!client || !message) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&client->client_mutex);
    
    if (!client->metadata_conn || 
        client->metadata_conn->state != DISTFS_CONN_AUTHENTICATED) {
        pthread_mutex_unlock(&client->client_mutex);
        return DISTFS_ERROR_NETWORK_FAILURE;
    }
    
    int result = distfs_message_send(client->metadata_conn, message);
    
    pthread_mutex_unlock(&client->client_mutex);
    return result;
}

/* 从元数据服务器接收消息 */
distfs_message_t* distfs_network_client_receive_from_metadata(distfs_network_client_t *client) {
    if (!client) {
        return NULL;
    }
    
    pthread_mutex_lock(&client->client_mutex);
    
    if (!client->metadata_conn || 
        client->metadata_conn->state != DISTFS_CONN_AUTHENTICATED) {
        pthread_mutex_unlock(&client->client_mutex);
        return NULL;
    }
    
    distfs_message_t *message = distfs_message_receive(client->metadata_conn);
    
    pthread_mutex_unlock(&client->client_mutex);
    return message;
}

/* 选择存储节点 */
distfs_connection_t* distfs_network_client_select_storage(distfs_network_client_t *client,
                                                          uint64_t key) {
    if (!client || client->storage_conn_count == 0) {
        return NULL;
    }
    
    pthread_mutex_lock(&client->client_mutex);
    
    // 使用简单的哈希选择策略
    int index = key % client->storage_conn_count;
    distfs_connection_t *conn = client->storage_conns[index];
    
    // 检查连接状态
    if (!conn || conn->state != DISTFS_CONN_AUTHENTICATED) {
        conn = NULL;
        
        // 尝试找到可用的连接
        for (int i = 0; i < client->storage_conn_count; i++) {
            if (client->storage_conns[i] && 
                client->storage_conns[i]->state == DISTFS_CONN_AUTHENTICATED) {
                conn = client->storage_conns[i];
                break;
            }
        }
    }
    
    pthread_mutex_unlock(&client->client_mutex);
    return conn;
}

/* 获取客户端统计信息 */
int distfs_network_client_get_stats(distfs_network_client_t *client,
                                    int *metadata_connected,
                                    int *storage_connected,
                                    uint64_t *bytes_sent,
                                    uint64_t *bytes_received) {
    if (!client) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&client->client_mutex);
    
    if (metadata_connected) {
        *metadata_connected = (client->metadata_conn && 
                              client->metadata_conn->state == DISTFS_CONN_AUTHENTICATED) ? 1 : 0;
    }
    
    if (storage_connected) {
        int connected = 0;
        for (int i = 0; i < client->storage_conn_count; i++) {
            if (client->storage_conns[i] && 
                client->storage_conns[i]->state == DISTFS_CONN_AUTHENTICATED) {
                connected++;
            }
        }
        *storage_connected = connected;
    }
    
    if (bytes_sent || bytes_received) {
        uint64_t total_sent = 0, total_received = 0;
        
        if (client->metadata_conn) {
            total_sent += client->metadata_conn->bytes_sent;
            total_received += client->metadata_conn->bytes_received;
        }
        
        for (int i = 0; i < client->storage_conn_count; i++) {
            if (client->storage_conns[i]) {
                total_sent += client->storage_conns[i]->bytes_sent;
                total_received += client->storage_conns[i]->bytes_received;
            }
        }
        
        if (bytes_sent) *bytes_sent = total_sent;
        if (bytes_received) *bytes_received = total_received;
    }
    
    pthread_mutex_unlock(&client->client_mutex);
    return DISTFS_SUCCESS;
}
