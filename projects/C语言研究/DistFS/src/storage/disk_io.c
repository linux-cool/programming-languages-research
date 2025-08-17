/*
 * DistFS 磁盘I/O模块
 * 提供高性能的异步磁盘I/O操作
 */

#define _GNU_SOURCE
#include "distfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <errno.h>
#include <stdbool.h>
#include <aio.h>

/* I/O操作类型 */
typedef enum {
    IO_OP_READ = 0,
    IO_OP_WRITE = 1,
    IO_OP_SYNC = 2
} io_operation_type_t;

/* I/O请求结构 */
typedef struct io_request {
    uint64_t request_id;
    io_operation_type_t type;
    int fd;
    void *buffer;
    size_t size;
    off_t offset;
    
    /* 异步I/O控制块 */
    struct aiocb aiocb;
    
    /* 回调函数 */
    void (*callback)(struct io_request *req, int result, void *user_data);
    void *user_data;
    
    /* 状态信息 */
    time_t submit_time;
    time_t complete_time;
    int result;
    bool completed;
    
    struct io_request *next;
} io_request_t;

/* I/O统计信息 */
typedef struct io_stats {
    uint64_t total_reads;
    uint64_t total_writes;
    uint64_t total_syncs;
    uint64_t bytes_read;
    uint64_t bytes_written;
    uint64_t read_latency_sum;
    uint64_t write_latency_sum;
    uint64_t pending_requests;
    uint64_t completed_requests;
    uint64_t failed_requests;
} io_stats_t;

/* 磁盘I/O管理器结构 */
struct distfs_disk_io_manager {
    /* 请求队列 */
    io_request_t *pending_requests;
    io_request_t *completed_requests;
    pthread_mutex_t pending_mutex;
    pthread_mutex_t completed_mutex;
    
    /* 工作线程 */
    pthread_t *worker_threads;
    int worker_count;
    bool running;
    
    /* 完成处理线程 */
    pthread_t completion_thread;
    pthread_cond_t completion_cond;
    
    /* 配置参数 */
    int max_concurrent_requests;
    int io_timeout;
    bool use_direct_io;
    bool use_sync_io;
    
    /* 统计信息 */
    io_stats_t stats;
    pthread_mutex_t stats_mutex;
    
    /* 请求ID生成器 */
    uint64_t next_request_id;
};

/* 全局磁盘I/O管理器实例 */
static distfs_disk_io_manager_t *g_disk_io_manager = NULL;

/* 生成请求ID */
static uint64_t generate_request_id(void) {
    return __sync_fetch_and_add(&g_disk_io_manager->next_request_id, 1);
}

/* 创建I/O请求 */
static io_request_t* create_io_request(io_operation_type_t type, int fd, 
                                      void *buffer, size_t size, off_t offset,
                                      void (*callback)(io_request_t*, int, void*),
                                      void *user_data) {
    io_request_t *req = calloc(1, sizeof(io_request_t));
    if (!req) return NULL;
    
    req->request_id = generate_request_id();
    req->type = type;
    req->fd = fd;
    req->buffer = buffer;
    req->size = size;
    req->offset = offset;
    req->callback = callback;
    req->user_data = user_data;
    req->submit_time = time(NULL);
    req->completed = false;
    
    // 初始化aiocb结构
    memset(&req->aiocb, 0, sizeof(struct aiocb));
    req->aiocb.aio_fildes = fd;
    req->aiocb.aio_buf = buffer;
    req->aiocb.aio_nbytes = size;
    req->aiocb.aio_offset = offset;
    
    return req;
}

/* 销毁I/O请求 */
static void destroy_io_request(io_request_t *req) {
    if (req) {
        free(req);
    }
}

/* 提交异步读请求 */
static int submit_async_read(io_request_t *req) {
    int result = aio_read(&req->aiocb);
    if (result != 0) {
        DISTFS_LOG_ERROR("Failed to submit async read: %s", strerror(errno));
        return DISTFS_ERROR_SYSTEM_ERROR;
    }
    
    return DISTFS_SUCCESS;
}

/* 提交异步写请求 */
static int submit_async_write(io_request_t *req) {
    int result = aio_write(&req->aiocb);
    if (result != 0) {
        DISTFS_LOG_ERROR("Failed to submit async write: %s", strerror(errno));
        return DISTFS_ERROR_SYSTEM_ERROR;
    }
    
    return DISTFS_SUCCESS;
}

/* 提交同步请求 */
static int submit_sync_request(io_request_t *req) {
    int result = aio_fsync(O_SYNC, &req->aiocb);
    if (result != 0) {
        DISTFS_LOG_ERROR("Failed to submit async sync: %s", strerror(errno));
        return DISTFS_ERROR_SYSTEM_ERROR;
    }
    
    return DISTFS_SUCCESS;
}

/* 检查I/O请求完成状态 */
static int check_io_completion(io_request_t *req) {
    int error = aio_error(&req->aiocb);
    
    if (error == EINPROGRESS) {
        return 0; // 仍在进行中
    } else if (error == 0) {
        // 操作完成
        ssize_t result = aio_return(&req->aiocb);
        req->result = (int)result;
        req->completed = true;
        req->complete_time = time(NULL);
        return 1; // 完成
    } else {
        // 操作失败
        req->result = -error;
        req->completed = true;
        req->complete_time = time(NULL);
        return -1; // 失败
    }
}

/* 添加请求到待处理队列 */
static int enqueue_pending_request(distfs_disk_io_manager_t *manager, io_request_t *req) {
    pthread_mutex_lock(&manager->pending_mutex);
    
    req->next = manager->pending_requests;
    manager->pending_requests = req;
    
    pthread_mutex_lock(&manager->stats_mutex);
    manager->stats.pending_requests++;
    pthread_mutex_unlock(&manager->stats_mutex);
    
    pthread_mutex_unlock(&manager->pending_mutex);
    
    return DISTFS_SUCCESS;
}

/* 从待处理队列获取请求 */
static io_request_t* dequeue_pending_request(distfs_disk_io_manager_t *manager) {
    pthread_mutex_lock(&manager->pending_mutex);
    
    io_request_t *req = manager->pending_requests;
    if (req) {
        manager->pending_requests = req->next;
        req->next = NULL;
        
        pthread_mutex_lock(&manager->stats_mutex);
        manager->stats.pending_requests--;
        pthread_mutex_unlock(&manager->stats_mutex);
    }
    
    pthread_mutex_unlock(&manager->pending_mutex);
    
    return req;
}

/* 添加请求到完成队列 */
static int enqueue_completed_request(distfs_disk_io_manager_t *manager, io_request_t *req) {
    pthread_mutex_lock(&manager->completed_mutex);
    
    req->next = manager->completed_requests;
    manager->completed_requests = req;
    
    pthread_cond_signal(&manager->completion_cond);
    pthread_mutex_unlock(&manager->completed_mutex);
    
    return DISTFS_SUCCESS;
}

/* 从完成队列获取请求 */
static io_request_t* dequeue_completed_request(distfs_disk_io_manager_t *manager) {
    pthread_mutex_lock(&manager->completed_mutex);
    
    while (!manager->completed_requests && manager->running) {
        pthread_cond_wait(&manager->completion_cond, &manager->completed_mutex);
    }
    
    if (!manager->running) {
        pthread_mutex_unlock(&manager->completed_mutex);
        return NULL;
    }
    
    io_request_t *req = manager->completed_requests;
    if (req) {
        manager->completed_requests = req->next;
        req->next = NULL;
    }
    
    pthread_mutex_unlock(&manager->completed_mutex);
    
    return req;
}

/* 工作线程函数 */
static void* worker_thread(void *arg) {
    distfs_disk_io_manager_t *manager = (distfs_disk_io_manager_t*)arg;
    
    while (manager->running) {
        io_request_t *req = dequeue_pending_request(manager);
        if (!req) {
            usleep(1000); // 1ms
            continue;
        }
        
        int result = DISTFS_SUCCESS;
        
        // 提交I/O请求
        switch (req->type) {
            case IO_OP_READ:
                result = submit_async_read(req);
                break;
            case IO_OP_WRITE:
                result = submit_async_write(req);
                break;
            case IO_OP_SYNC:
                result = submit_sync_request(req);
                break;
            default:
                result = DISTFS_ERROR_INVALID_PARAM;
                break;
        }
        
        if (result != DISTFS_SUCCESS) {
            req->result = result;
            req->completed = true;
            req->complete_time = time(NULL);
            enqueue_completed_request(manager, req);
            continue;
        }
        
        // 等待I/O完成
        while (manager->running) {
            int status = check_io_completion(req);
            if (status != 0) {
                // I/O完成或失败
                enqueue_completed_request(manager, req);
                break;
            }
            usleep(1000); // 1ms
        }
    }
    
    return NULL;
}

/* 完成处理线程函数 */
static void* completion_thread(void *arg) {
    distfs_disk_io_manager_t *manager = (distfs_disk_io_manager_t*)arg;
    
    while (manager->running) {
        io_request_t *req = dequeue_completed_request(manager);
        if (!req) break;
        
        // 更新统计信息
        pthread_mutex_lock(&manager->stats_mutex);
        
        if (req->result >= 0) {
            manager->stats.completed_requests++;
            
            uint64_t latency = (req->complete_time - req->submit_time) * 1000; // ms
            
            switch (req->type) {
                case IO_OP_READ:
                    manager->stats.total_reads++;
                    manager->stats.bytes_read += req->result;
                    manager->stats.read_latency_sum += latency;
                    break;
                case IO_OP_WRITE:
                    manager->stats.total_writes++;
                    manager->stats.bytes_written += req->result;
                    manager->stats.write_latency_sum += latency;
                    break;
                case IO_OP_SYNC:
                    manager->stats.total_syncs++;
                    break;
            }
        } else {
            manager->stats.failed_requests++;
        }
        
        pthread_mutex_unlock(&manager->stats_mutex);
        
        // 调用回调函数
        if (req->callback) {
            req->callback(req, req->result, req->user_data);
        }
        
        destroy_io_request(req);
    }
    
    return NULL;
}

/* 创建磁盘I/O管理器 */
distfs_disk_io_manager_t* distfs_disk_io_manager_create(int worker_count,
                                                       int max_concurrent_requests) {
    if (worker_count <= 0 || max_concurrent_requests <= 0) {
        return NULL;
    }

    if (g_disk_io_manager) {
        return NULL; // 单例模式
    }

    g_disk_io_manager = calloc(1, sizeof(distfs_disk_io_manager_t));
    if (!g_disk_io_manager) {
        return NULL;
    }

    g_disk_io_manager->worker_count = worker_count;
    g_disk_io_manager->max_concurrent_requests = max_concurrent_requests;
    g_disk_io_manager->io_timeout = 30; // 30秒超时
    g_disk_io_manager->use_direct_io = false;
    g_disk_io_manager->use_sync_io = false;
    g_disk_io_manager->next_request_id = 1;

    // 初始化互斥锁和条件变量
    if (pthread_mutex_init(&g_disk_io_manager->pending_mutex, NULL) != 0 ||
        pthread_mutex_init(&g_disk_io_manager->completed_mutex, NULL) != 0 ||
        pthread_mutex_init(&g_disk_io_manager->stats_mutex, NULL) != 0 ||
        pthread_cond_init(&g_disk_io_manager->completion_cond, NULL) != 0) {
        free(g_disk_io_manager);
        g_disk_io_manager = NULL;
        return NULL;
    }

    // 分配工作线程数组
    g_disk_io_manager->worker_threads = calloc(worker_count, sizeof(pthread_t));
    if (!g_disk_io_manager->worker_threads) {
        pthread_mutex_destroy(&g_disk_io_manager->pending_mutex);
        pthread_mutex_destroy(&g_disk_io_manager->completed_mutex);
        pthread_mutex_destroy(&g_disk_io_manager->stats_mutex);
        pthread_cond_destroy(&g_disk_io_manager->completion_cond);
        free(g_disk_io_manager);
        g_disk_io_manager = NULL;
        return NULL;
    }

    return g_disk_io_manager;
}

/* 启动磁盘I/O管理器 */
int distfs_disk_io_manager_start(distfs_disk_io_manager_t *manager) {
    if (!manager || manager->running) {
        return DISTFS_ERROR_INVALID_PARAM;
    }

    manager->running = true;

    // 启动完成处理线程
    if (pthread_create(&manager->completion_thread, NULL, completion_thread, manager) != 0) {
        manager->running = false;
        return DISTFS_ERROR_SYSTEM_ERROR;
    }

    // 启动工作线程
    for (int i = 0; i < manager->worker_count; i++) {
        if (pthread_create(&manager->worker_threads[i], NULL, worker_thread, manager) != 0) {
            manager->running = false;

            // 等待已启动的线程结束
            pthread_join(manager->completion_thread, NULL);
            for (int j = 0; j < i; j++) {
                pthread_join(manager->worker_threads[j], NULL);
            }

            return DISTFS_ERROR_SYSTEM_ERROR;
        }
    }

    DISTFS_LOG_INFO("Disk I/O manager started with %d workers", manager->worker_count);

    return DISTFS_SUCCESS;
}
