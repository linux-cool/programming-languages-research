/*
 * DistFS 网络通信模块头文件
 * 定义网络协议、消息格式和通信接口
 */

#ifndef DISTFS_NETWORK_H
#define DISTFS_NETWORK_H

#include "distfs.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 网络协议常量 */
#define DISTFS_PROTOCOL_VERSION     1
#define DISTFS_MAX_MESSAGE_SIZE     (16 * 1024 * 1024)  // 16MB
#define DISTFS_HEADER_SIZE          16
#define DISTFS_MAX_CONNECTIONS      10000
#define DISTFS_CONNECT_TIMEOUT      30
#define DISTFS_READ_TIMEOUT         60
#define DISTFS_WRITE_TIMEOUT        60

/* 消息类型定义 */
typedef enum {
    /* 客户端请求消息 */
    DISTFS_MSG_CREATE_FILE = 0x0001,
    DISTFS_MSG_OPEN_FILE = 0x0002,
    DISTFS_MSG_READ_FILE = 0x0003,
    DISTFS_MSG_WRITE_FILE = 0x0004,
    DISTFS_MSG_CLOSE_FILE = 0x0005,
    DISTFS_MSG_DELETE_FILE = 0x0006,
    DISTFS_MSG_CREATE_DIR = 0x0007,
    DISTFS_MSG_DELETE_DIR = 0x0008,
    DISTFS_MSG_LIST_DIR = 0x0009,
    DISTFS_MSG_GET_STAT = 0x000A,
    DISTFS_MSG_SET_ATTR = 0x000B,
    
    /* 节点管理消息 */
    DISTFS_MSG_JOIN_CLUSTER = 0x0101,
    DISTFS_MSG_LEAVE_CLUSTER = 0x0102,
    DISTFS_MSG_HEARTBEAT = 0x0103,
    DISTFS_MSG_NODE_STATUS = 0x0104,
    DISTFS_MSG_CLUSTER_INFO = 0x0105,
    
    /* 数据同步消息 */
    DISTFS_MSG_REPLICATE_DATA = 0x0201,
    DISTFS_MSG_SYNC_METADATA = 0x0202,
    DISTFS_MSG_REPAIR_DATA = 0x0203,
    DISTFS_MSG_MIGRATE_DATA = 0x0204,

    /* 块操作消息 */
    DISTFS_MSG_READ_BLOCK = 0x0301,
    DISTFS_MSG_WRITE_BLOCK = 0x0302,
    DISTFS_MSG_DELETE_BLOCK = 0x0303,
    
    /* 响应消息 */
    DISTFS_MSG_SUCCESS = 0x8000,
    DISTFS_MSG_ERROR = 0x8001,
    DISTFS_MSG_DATA = 0x8002,
    DISTFS_MSG_METADATA = 0x8003,
    
    /* 内部消息 */
    DISTFS_MSG_PING = 0xF001,
    DISTFS_MSG_PONG = 0xF002
} distfs_msg_type_t;

/* 消息标志 */
typedef enum {
    DISTFS_MSG_FLAG_NONE = 0x00,
    DISTFS_MSG_FLAG_COMPRESSED = 0x01,
    DISTFS_MSG_FLAG_ENCRYPTED = 0x02,
    DISTFS_MSG_FLAG_URGENT = 0x04,
    DISTFS_MSG_FLAG_RELIABLE = 0x08
} distfs_msg_flags_t;

/* 消息头结构 */
typedef struct {
    uint32_t magic;         // 魔数 0x44495354 ("DIST")
    uint16_t version;       // 协议版本
    uint16_t type;          // 消息类型
    uint32_t flags;         // 消息标志
    uint32_t length;        // 消息长度(不包括头部)
    uint32_t sequence;      // 序列号
    uint32_t checksum;      // 校验和
} __attribute__((packed)) distfs_msg_header_t;

/* 通用消息结构 */
typedef struct {
    distfs_msg_header_t header;
    uint8_t *payload;
} distfs_message_t;

/* 连接状态 */
typedef enum {
    DISTFS_CONN_DISCONNECTED = 0,
    DISTFS_CONN_CONNECTING = 1,
    DISTFS_CONN_CONNECTED = 2,
    DISTFS_CONN_AUTHENTICATED = 3,
    DISTFS_CONN_ERROR = 4
} distfs_conn_state_t;

/* 网络连接结构 */
typedef struct {
    int sockfd;                     // socket文件描述符
    struct sockaddr_in addr;        // 远程地址
    distfs_conn_state_t state;      // 连接状态
    uint64_t node_id;               // 远程节点ID
    uint32_t last_sequence;         // 最后序列号
    uint64_t last_activity;         // 最后活动时间
    pthread_mutex_t send_mutex;     // 发送互斥锁
    pthread_mutex_t recv_mutex;     // 接收互斥锁
    uint64_t bytes_sent;            // 发送字节数
    uint64_t bytes_received;        // 接收字节数
    uint32_t messages_sent;         // 发送消息数
    uint32_t messages_received;     // 接收消息数
} distfs_connection_t;

/* 网络服务器结构 */
typedef struct {
    int listen_fd;                  // 监听socket
    uint16_t port;                  // 监听端口
    int epoll_fd;                   // epoll文件描述符
    pthread_t *worker_threads;      // 工作线程池
    int thread_count;               // 线程数量
    bool running;                   // 运行状态
    distfs_connection_t *connections; // 连接池
    int max_connections;            // 最大连接数
    int active_connections;         // 活跃连接数
    pthread_mutex_t conn_mutex;     // 连接池互斥锁
    
    /* 统计信息 */
    uint64_t total_connections;     // 总连接数
    uint64_t total_requests;        // 总请求数
    uint64_t total_errors;          // 总错误数
} distfs_server_t;

/* 网络客户端结构 */
typedef struct {
    distfs_connection_t *metadata_conn;  // 元数据服务连接
    distfs_connection_t **storage_conns; // 存储节点连接数组
    int storage_conn_count;              // 存储连接数量
    pthread_mutex_t client_mutex;       // 客户端互斥锁
    uint32_t next_sequence;              // 下一个序列号
} distfs_network_client_t;

/* 消息处理回调函数类型 */
typedef int (*distfs_msg_handler_t)(distfs_connection_t *conn, 
                                    distfs_message_t *msg, void *user_data);

/* 网络服务器接口 */
distfs_server_t* distfs_server_create(uint16_t port, int max_connections);
int distfs_server_start(distfs_server_t *server);
int distfs_server_stop(distfs_server_t *server);
void distfs_server_destroy(distfs_server_t *server);
int distfs_server_register_handler(distfs_server_t *server, 
                                   distfs_msg_type_t type,
                                   distfs_msg_handler_t handler,
                                   void *user_data);

/* 网络客户端接口 */
distfs_network_client_t* distfs_network_client_create(void);
void distfs_network_client_destroy(distfs_network_client_t *client);
int distfs_network_client_connect_metadata(distfs_network_client_t *client,
                                           const char *host, uint16_t port);
int distfs_network_client_connect_storage(distfs_network_client_t *client,
                                          const char *host, uint16_t port);
int distfs_network_client_disconnect(distfs_network_client_t *client);

/* 连接管理接口 */
distfs_connection_t* distfs_connection_create(int sockfd, struct sockaddr_in *addr);
void distfs_connection_destroy(distfs_connection_t *conn);
int distfs_connection_set_nonblocking(int sockfd);
int distfs_connection_set_keepalive(int sockfd);
int distfs_connection_set_timeout(int sockfd, int timeout);

/* 消息处理接口 */
distfs_message_t* distfs_message_create(distfs_msg_type_t type, 
                                        const void *payload, uint32_t length);
void distfs_message_destroy(distfs_message_t *msg);
int distfs_message_send(distfs_connection_t *conn, distfs_message_t *msg);
distfs_message_t* distfs_message_receive(distfs_connection_t *conn);
int distfs_message_send_sync(distfs_connection_t *conn, distfs_message_t *request,
                             distfs_message_t **response, int timeout);

/* 协议处理接口 */
int distfs_protocol_serialize_file_metadata(const distfs_file_metadata_t *metadata,
                                            uint8_t **buffer, uint32_t *length);
int distfs_protocol_deserialize_file_metadata(const uint8_t *buffer, uint32_t length,
                                              distfs_file_metadata_t *metadata);
int distfs_protocol_serialize_node_info(const distfs_node_info_t *info,
                                        uint8_t **buffer, uint32_t *length);
int distfs_protocol_deserialize_node_info(const uint8_t *buffer, uint32_t length,
                                          distfs_node_info_t *info);

/* 工具函数 */
uint32_t distfs_calculate_message_checksum(const distfs_message_t *msg);
int distfs_validate_message(const distfs_message_t *msg);
const char* distfs_msg_type_to_string(distfs_msg_type_t type);
int distfs_compress_data(const uint8_t *input, uint32_t input_len,
                        uint8_t **output, uint32_t *output_len);
int distfs_decompress_data(const uint8_t *input, uint32_t input_len,
                          uint8_t **output, uint32_t *output_len);

/* 网络统计接口 */
int distfs_network_get_stats(distfs_server_t *server, distfs_stats_t *stats);
void distfs_network_print_stats(distfs_server_t *server);

#ifdef __cplusplus
}
#endif

#endif /* DISTFS_NETWORK_H */
