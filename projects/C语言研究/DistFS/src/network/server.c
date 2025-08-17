/*
 * DistFS 网络服务器实现
 * 提供高性能的网络服务器功能
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
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>

/* 连接状态 */
typedef enum {
    CONN_STATE_CONNECTING = 0,
    CONN_STATE_CONNECTED = 1,
    CONN_STATE_READING = 2,
    CONN_STATE_WRITING = 3,
    CONN_STATE_CLOSING = 4,
    CONN_STATE_CLOSED = 5
} connection_state_t;

/* 连接结构 */
typedef struct server_connection {
    int fd;
    connection_state_t state;
    struct sockaddr_in addr;
    
    /* 读缓冲区 */
    char *read_buffer;
    size_t read_buffer_size;
    size_t read_pos;
    size_t read_len;
    
    /* 写缓冲区 */
    char *write_buffer;
    size_t write_buffer_size;
    size_t write_pos;
    size_t write_len;
    
    /* 消息处理 */
    distfs_message_t *current_message;
    bool message_header_complete;
    
    /* 时间戳 */
    time_t created_time;
    time_t last_activity;
    
    /* 统计信息 */
    uint64_t bytes_received;
    uint64_t bytes_sent;
    uint64_t messages_received;
    uint64_t messages_sent;
    
    struct server_connection *next;
} server_connection_t;

/* 工作线程结构 */
typedef struct worker_thread {
    pthread_t thread;
    int epoll_fd;
    bool running;
    distfs_network_server_t *server;
    
    /* 统计信息 */
    uint64_t events_processed;
    uint64_t connections_handled;
} worker_thread_t;

/* 网络服务器结构 */
struct distfs_network_server {
    int listen_fd;
    uint16_t port;
    int max_connections;
    int current_connections;
    
    /* 工作线程池 */
    worker_thread_t *workers;
    int worker_count;
    int next_worker;
    
    /* 连接管理 */
    server_connection_t *connections;
    pthread_mutex_t connections_mutex;
    
    /* 消息处理回调 */
    distfs_message_handler_t message_handler;
    void *user_data;
    
    /* 配置参数 */
    int connection_timeout;
    int read_buffer_size;
    int write_buffer_size;
    
    /* 统计信息 */
    uint64_t total_connections;
    uint64_t active_connections;
    uint64_t total_messages;
    uint64_t total_bytes;
    
    bool running;
    pthread_mutex_t mutex;
};

/* 设置socket为非阻塞模式 */
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        return -1;
    }
    
    return 0;
}

/* 创建服务器连接 */
static server_connection_t* create_server_connection(int fd, struct sockaddr_in *addr) {
    server_connection_t *conn = calloc(1, sizeof(server_connection_t));
    if (!conn) return NULL;
    
    conn->fd = fd;
    conn->state = CONN_STATE_CONNECTED;
    conn->addr = *addr;
    
    conn->read_buffer_size = 8192;
    conn->read_buffer = malloc(conn->read_buffer_size);
    if (!conn->read_buffer) {
        free(conn);
        return NULL;
    }
    
    conn->write_buffer_size = 8192;
    conn->write_buffer = malloc(conn->write_buffer_size);
    if (!conn->write_buffer) {
        free(conn->read_buffer);
        free(conn);
        return NULL;
    }
    
    conn->created_time = time(NULL);
    conn->last_activity = conn->created_time;
    
    return conn;
}

/* 销毁服务器连接 */
static void destroy_server_connection(server_connection_t *conn) {
    if (!conn) return;
    
    if (conn->fd >= 0) {
        close(conn->fd);
    }
    
    if (conn->read_buffer) {
        free(conn->read_buffer);
    }
    
    if (conn->write_buffer) {
        free(conn->write_buffer);
    }
    
    if (conn->current_message) {
        distfs_message_destroy(conn->current_message);
    }
    
    free(conn);
}

/* 扩展缓冲区 */
static int expand_buffer(char **buffer, size_t *size, size_t new_size) {
    if (new_size <= *size) return 0;
    
    char *new_buffer = realloc(*buffer, new_size);
    if (!new_buffer) return -1;
    
    *buffer = new_buffer;
    *size = new_size;
    return 0;
}

/* 处理连接读取 */
static int handle_connection_read(server_connection_t *conn, distfs_network_server_t *server) {
    while (true) {
        // 确保读缓冲区有足够空间
        size_t available = conn->read_buffer_size - conn->read_len;
        if (available < 1024) {
            if (expand_buffer(&conn->read_buffer, &conn->read_buffer_size, 
                            conn->read_buffer_size * 2) != 0) {
                return -1;
            }
            available = conn->read_buffer_size - conn->read_len;
        }
        
        ssize_t bytes = recv(conn->fd, conn->read_buffer + conn->read_len, available, 0);
        if (bytes <= 0) {
            if (bytes == 0) {
                // 连接关闭
                return -1;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 没有更多数据
                break;
            } else {
                // 读取错误
                return -1;
            }
        }
        
        conn->read_len += bytes;
        conn->bytes_received += bytes;
        conn->last_activity = time(NULL);
        
        // 处理完整的消息
        while (conn->read_len > conn->read_pos) {
            if (!conn->message_header_complete) {
                // 检查是否有完整的消息头
                if (conn->read_len - conn->read_pos >= sizeof(distfs_msg_header_t)) {
                    distfs_msg_header_t *header = (distfs_msg_header_t*)(conn->read_buffer + conn->read_pos);
                    
                    // 验证魔数
                    if (header->magic != DISTFS_PROTOCOL_MAGIC) {
                        DISTFS_LOG_ERROR("Invalid message magic: 0x%08x", header->magic);
                        return -1;
                    }
                    
                    // 创建消息对象
                    conn->current_message = calloc(1, sizeof(distfs_message_t));
                    if (!conn->current_message) {
                        return -1;
                    }
                    
                    conn->current_message->header = *header;
                    conn->message_header_complete = true;
                    conn->read_pos += sizeof(distfs_msg_header_t);
                    
                    // 分配payload缓冲区
                    if (header->length > 0) {
                        conn->current_message->payload = malloc(header->length);
                        if (!conn->current_message->payload) {
                            distfs_message_destroy(conn->current_message);
                            conn->current_message = NULL;
                            return -1;
                        }
                    }
                } else {
                    // 消息头不完整，等待更多数据
                    break;
                }
            }
            
            if (conn->message_header_complete && conn->current_message) {
                // 读取payload
                uint32_t payload_len = conn->current_message->header.length;
                if (payload_len > 0) {
                    size_t remaining = payload_len - conn->current_message->payload_pos;
                    size_t available_data = conn->read_len - conn->read_pos;
                    size_t to_copy = remaining < available_data ? remaining : available_data;
                    
                    memcpy((char*)conn->current_message->payload + conn->current_message->payload_pos,
                           conn->read_buffer + conn->read_pos, to_copy);
                    
                    conn->current_message->payload_pos += to_copy;
                    conn->read_pos += to_copy;
                    
                    if (conn->current_message->payload_pos < payload_len) {
                        // payload不完整，等待更多数据
                        break;
                    }
                }
                
                // 消息完整，处理消息
                if (server->message_handler) {
                    distfs_connection_t client_conn = {
                        .fd = conn->fd,
                        .addr = conn->addr
                    };
                    
                    server->message_handler(&client_conn, conn->current_message, server->user_data);
                }
                
                conn->messages_received++;
                
                // 清理当前消息
                distfs_message_destroy(conn->current_message);
                conn->current_message = NULL;
                conn->message_header_complete = false;
            }
        }
        
        // 压缩读缓冲区
        if (conn->read_pos > 0) {
            memmove(conn->read_buffer, conn->read_buffer + conn->read_pos, 
                   conn->read_len - conn->read_pos);
            conn->read_len -= conn->read_pos;
            conn->read_pos = 0;
        }
    }
    
    return 0;
}

/* 处理连接写入 */
static int handle_connection_write(server_connection_t *conn) {
    while (conn->write_len > conn->write_pos) {
        ssize_t bytes = send(conn->fd, conn->write_buffer + conn->write_pos,
                           conn->write_len - conn->write_pos, 0);

        if (bytes <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 发送缓冲区满，稍后重试
                break;
            } else {
                // 发送错误
                return -1;
            }
        }

        conn->write_pos += bytes;
        conn->bytes_sent += bytes;
        conn->last_activity = time(NULL);
    }

    // 如果所有数据都已发送，重置写缓冲区
    if (conn->write_pos >= conn->write_len) {
        conn->write_pos = 0;
        conn->write_len = 0;
    }

    return 0;
}

/* 向连接发送数据 */
static int connection_send_data(server_connection_t *conn, const void *data, size_t len) {
    // 确保写缓冲区有足够空间
    if (conn->write_len + len > conn->write_buffer_size) {
        size_t new_size = conn->write_buffer_size;
        while (new_size < conn->write_len + len) {
            new_size *= 2;
        }

        if (expand_buffer(&conn->write_buffer, &conn->write_buffer_size, new_size) != 0) {
            return -1;
        }
    }

    // 复制数据到写缓冲区
    memcpy(conn->write_buffer + conn->write_len, data, len);
    conn->write_len += len;

    return 0;
}

/* 工作线程函数 */
static void* worker_thread_func(void *arg) {
    worker_thread_t *worker = (worker_thread_t*)arg;
    distfs_network_server_t *server = worker->server;

    struct epoll_event events[64];

    while (worker->running) {
        int nfds = epoll_wait(worker->epoll_fd, events, 64, 1000);

        if (nfds < 0) {
            if (errno == EINTR) continue;
            DISTFS_LOG_ERROR("epoll_wait failed: %s", strerror(errno));
            break;
        }

        for (int i = 0; i < nfds; i++) {
            server_connection_t *conn = (server_connection_t*)events[i].data.ptr;
            uint32_t event_mask = events[i].events;

            if (event_mask & (EPOLLERR | EPOLLHUP)) {
                // 连接错误或关闭
                conn->state = CONN_STATE_CLOSING;
                continue;
            }

            if (event_mask & EPOLLIN) {
                // 可读事件
                if (handle_connection_read(conn, server) != 0) {
                    conn->state = CONN_STATE_CLOSING;
                    continue;
                }
            }

            if (event_mask & EPOLLOUT) {
                // 可写事件
                if (handle_connection_write(conn) != 0) {
                    conn->state = CONN_STATE_CLOSING;
                    continue;
                }
            }

            // 检查连接超时
            time_t now = time(NULL);
            if (now - conn->last_activity > server->connection_timeout) {
                conn->state = CONN_STATE_CLOSING;
                continue;
            }

            // 清理关闭的连接
            if (conn->state == CONN_STATE_CLOSING) {
                epoll_ctl(worker->epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);

                pthread_mutex_lock(&server->connections_mutex);

                // 从连接列表中移除
                server_connection_t *prev = NULL;
                server_connection_t *current = server->connections;
                while (current) {
                    if (current == conn) {
                        if (prev) {
                            prev->next = current->next;
                        } else {
                            server->connections = current->next;
                        }
                        break;
                    }
                    prev = current;
                    current = current->next;
                }

                server->current_connections--;

                pthread_mutex_unlock(&server->connections_mutex);

                destroy_server_connection(conn);
            }

            worker->events_processed++;
        }
    }

    return NULL;
}

/* 接受新连接 */
static int accept_new_connection(distfs_network_server_t *server) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = accept(server->listen_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0; // 没有新连接
        }
        DISTFS_LOG_ERROR("accept failed: %s", strerror(errno));
        return -1;
    }

    // 检查连接数限制
    if (server->current_connections >= server->max_connections) {
        DISTFS_LOG_WARN("Connection limit reached, rejecting new connection");
        close(client_fd);
        return 0;
    }

    // 设置为非阻塞模式
    if (set_nonblocking(client_fd) != 0) {
        DISTFS_LOG_ERROR("Failed to set client socket non-blocking");
        close(client_fd);
        return -1;
    }

    // 创建连接对象
    server_connection_t *conn = create_server_connection(client_fd, &client_addr);
    if (!conn) {
        DISTFS_LOG_ERROR("Failed to create connection object");
        close(client_fd);
        return -1;
    }

    // 选择工作线程
    worker_thread_t *worker = &server->workers[server->next_worker];
    server->next_worker = (server->next_worker + 1) % server->worker_count;

    // 添加到epoll
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    event.data.ptr = conn;

    if (epoll_ctl(worker->epoll_fd, EPOLL_CTL_ADD, client_fd, &event) != 0) {
        DISTFS_LOG_ERROR("Failed to add connection to epoll: %s", strerror(errno));
        destroy_server_connection(conn);
        return -1;
    }

    // 添加到连接列表
    pthread_mutex_lock(&server->connections_mutex);

    conn->next = server->connections;
    server->connections = conn;
    server->current_connections++;
    server->total_connections++;

    pthread_mutex_unlock(&server->connections_mutex);

    worker->connections_handled++;

    DISTFS_LOG_DEBUG("New connection from %s:%d",
                    inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    return 0;
}

/* 创建网络服务器 */
distfs_network_server_t* distfs_network_server_create(uint16_t port, int max_connections,
                                                     distfs_message_handler_t handler,
                                                     void *user_data) {
    if (port == 0 || max_connections <= 0 || !handler) {
        return NULL;
    }

    distfs_network_server_t *server = calloc(1, sizeof(distfs_network_server_t));
    if (!server) {
        return NULL;
    }

    server->port = port;
    server->max_connections = max_connections;
    server->message_handler = handler;
    server->user_data = user_data;
    server->worker_count = 4; // 默认4个工作线程
    server->connection_timeout = 300; // 5分钟超时
    server->read_buffer_size = 8192;
    server->write_buffer_size = 8192;
    server->listen_fd = -1;

    // 初始化互斥锁
    if (pthread_mutex_init(&server->connections_mutex, NULL) != 0 ||
        pthread_mutex_init(&server->mutex, NULL) != 0) {
        free(server);
        return NULL;
    }

    return server;
}

/* 启动网络服务器 */
int distfs_network_server_start(distfs_network_server_t *server) {
    if (!server || server->running) {
        return DISTFS_ERROR_INVALID_PARAM;
    }

    // 创建监听socket
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) {
        DISTFS_LOG_ERROR("Failed to create listen socket: %s", strerror(errno));
        return DISTFS_ERROR_SYSTEM_ERROR;
    }

    // 设置socket选项
    int opt = 1;
    if (setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        DISTFS_LOG_ERROR("Failed to set SO_REUSEADDR: %s", strerror(errno));
        close(server->listen_fd);
        return DISTFS_ERROR_SYSTEM_ERROR;
    }

    // 设置为非阻塞模式
    if (set_nonblocking(server->listen_fd) != 0) {
        DISTFS_LOG_ERROR("Failed to set listen socket non-blocking");
        close(server->listen_fd);
        return DISTFS_ERROR_SYSTEM_ERROR;
    }

    // 绑定地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server->port);

    if (bind(server->listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        DISTFS_LOG_ERROR("Failed to bind to port %d: %s", server->port, strerror(errno));
        close(server->listen_fd);
        return DISTFS_ERROR_SYSTEM_ERROR;
    }

    // 开始监听
    if (listen(server->listen_fd, 128) < 0) {
        DISTFS_LOG_ERROR("Failed to listen: %s", strerror(errno));
        close(server->listen_fd);
        return DISTFS_ERROR_SYSTEM_ERROR;
    }

    // 创建工作线程
    server->workers = calloc(server->worker_count, sizeof(worker_thread_t));
    if (!server->workers) {
        close(server->listen_fd);
        return DISTFS_ERROR_NO_MEMORY;
    }

    for (int i = 0; i < server->worker_count; i++) {
        worker_thread_t *worker = &server->workers[i];
        worker->server = server;
        worker->running = true;

        // 创建epoll实例
        worker->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
        if (worker->epoll_fd < 0) {
            DISTFS_LOG_ERROR("Failed to create epoll instance: %s", strerror(errno));

            // 清理已创建的工作线程
            for (int j = 0; j < i; j++) {
                server->workers[j].running = false;
                pthread_join(server->workers[j].thread, NULL);
                close(server->workers[j].epoll_fd);
            }

            free(server->workers);
            close(server->listen_fd);
            return DISTFS_ERROR_SYSTEM_ERROR;
        }

        // 启动工作线程
        if (pthread_create(&worker->thread, NULL, worker_thread_func, worker) != 0) {
            DISTFS_LOG_ERROR("Failed to create worker thread: %s", strerror(errno));

            close(worker->epoll_fd);

            // 清理已创建的工作线程
            for (int j = 0; j < i; j++) {
                server->workers[j].running = false;
                pthread_join(server->workers[j].thread, NULL);
                close(server->workers[j].epoll_fd);
            }

            free(server->workers);
            close(server->listen_fd);
            return DISTFS_ERROR_SYSTEM_ERROR;
        }
    }

    server->running = true;

    DISTFS_LOG_INFO("Network server started on port %d with %d workers",
                   server->port, server->worker_count);

    // 主循环处理新连接
    while (server->running) {
        if (accept_new_connection(server) < 0) {
            break;
        }
        usleep(1000); // 1ms
    }

    return DISTFS_SUCCESS;
}

/* 停止网络服务器 */
int distfs_network_server_stop(distfs_network_server_t *server) {
    if (!server || !server->running) {
        return DISTFS_ERROR_INVALID_PARAM;
    }

    server->running = false;

    // 停止工作线程
    if (server->workers) {
        for (int i = 0; i < server->worker_count; i++) {
            server->workers[i].running = false;
            pthread_join(server->workers[i].thread, NULL);
            close(server->workers[i].epoll_fd);
        }
        free(server->workers);
        server->workers = NULL;
    }

    // 关闭监听socket
    if (server->listen_fd >= 0) {
        close(server->listen_fd);
        server->listen_fd = -1;
    }

    DISTFS_LOG_INFO("Network server stopped");

    return DISTFS_SUCCESS;
}

/* 销毁网络服务器 */
void distfs_network_server_destroy(distfs_network_server_t *server) {
    if (!server) return;

    if (server->running) {
        distfs_network_server_stop(server);
    }

    // 清理所有连接
    pthread_mutex_lock(&server->connections_mutex);

    server_connection_t *conn = server->connections;
    while (conn) {
        server_connection_t *next = conn->next;
        destroy_server_connection(conn);
        conn = next;
    }

    pthread_mutex_unlock(&server->connections_mutex);

    pthread_mutex_destroy(&server->connections_mutex);
    pthread_mutex_destroy(&server->mutex);

    free(server);
}
