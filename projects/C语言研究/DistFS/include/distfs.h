/*
 * DistFS - 分布式文件系统核心头文件
 * 定义系统的核心数据结构、常量和API接口
 */

#ifndef DISTFS_H
#define DISTFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 系统常量定义 */
#define DISTFS_VERSION_MAJOR    1
#define DISTFS_VERSION_MINOR    0
#define DISTFS_VERSION_PATCH    0

#define DISTFS_MAX_PATH_LEN     4096
#define DISTFS_MAX_NAME_LEN     255
#define DISTFS_MAX_NODES        100
#define DISTFS_DEFAULT_REPLICAS 3
#define DISTFS_BLOCK_SIZE       (64 * 1024 * 1024)  // 64MB
#define DISTFS_MAX_BLOCKS       16

/* 错误码定义 */
typedef enum {
    DISTFS_SUCCESS = 0,
    DISTFS_ERROR_INVALID_PARAM = -1,
    DISTFS_ERROR_NO_MEMORY = -2,
    DISTFS_ERROR_FILE_NOT_FOUND = -3,
    DISTFS_ERROR_FILE_EXISTS = -4,
    DISTFS_ERROR_PERMISSION_DENIED = -5,
    DISTFS_ERROR_NETWORK_FAILURE = -6,
    DISTFS_ERROR_NODE_UNAVAILABLE = -7,
    DISTFS_ERROR_CONSISTENCY_VIOLATION = -8,
    DISTFS_ERROR_STORAGE_FULL = -9,
    DISTFS_ERROR_TIMEOUT = -10,
    DISTFS_ERROR_ALREADY_INITIALIZED = -11,
    DISTFS_ERROR_SYSTEM_ERROR = -12,
    DISTFS_ERROR_FILE_OPEN_FAILED = -13,
    DISTFS_ERROR_NOT_FOUND = -14,
    DISTFS_ERROR_UNSUPPORTED_OPERATION = -15,
    DISTFS_ERROR_UNKNOWN = -99
} distfs_error_t;

/* 节点类型定义 */
typedef enum {
    DISTFS_NODE_CLIENT = 0,
    DISTFS_NODE_METADATA = 1,
    DISTFS_NODE_STORAGE = 2
} distfs_node_type_t;

/* 节点状态定义 */
typedef enum {
    DISTFS_NODE_STATUS_UNKNOWN = 0,
    DISTFS_NODE_STATUS_ONLINE = 1,
    DISTFS_NODE_STATUS_OFFLINE = 2,
    DISTFS_NODE_STATUS_RECOVERING = 3,
    DISTFS_NODE_STATUS_FAILED = 4
} distfs_node_status_t;

/* 文件类型定义 */
typedef enum {
    DISTFS_FILE_TYPE_REGULAR = 0,
    DISTFS_FILE_TYPE_DIRECTORY = 1,
    DISTFS_FILE_TYPE_SYMLINK = 2
} distfs_file_type_t;

/* 网络地址结构 */
typedef struct {
    char ip[16];        // IPv4地址
    uint16_t port;      // 端口号
} distfs_addr_t;

/* 节点信息结构 */
typedef struct {
    uint64_t node_id;                   // 节点ID
    distfs_node_type_t type;            // 节点类型
    distfs_node_status_t status;        // 节点状态
    distfs_addr_t addr;                 // 网络地址
    uint64_t capacity;                  // 存储容量(字节)
    uint64_t used;                      // 已使用空间(字节)
    uint64_t last_heartbeat;            // 最后心跳时间
    char version[32];                   // 版本信息
} distfs_node_info_t;

/* 文件元数据结构 */
typedef struct {
    uint64_t inode;                     // 文件inode号
    char name[DISTFS_MAX_NAME_LEN + 1]; // 文件名
    distfs_file_type_t type;            // 文件类型
    uint64_t size;                      // 文件大小
    uint32_t mode;                      // 文件权限
    uint32_t uid;                       // 所有者ID
    uint32_t gid;                       // 组ID
    uint64_t atime;                     // 访问时间
    uint64_t mtime;                     // 修改时间
    uint64_t ctime;                     // 创建时间
    uint32_t nlinks;                    // 硬链接数
    uint32_t block_count;               // 数据块数量
    uint64_t blocks[DISTFS_MAX_BLOCKS]; // 数据块ID列表
    uint32_t checksum;                  // 元数据校验和
} distfs_file_metadata_t;

/* 数据块结构 */
typedef struct {
    uint64_t block_id;                  // 块ID
    uint32_t size;                      // 块大小
    uint32_t checksum;                  // 数据校验和
    uint8_t replica_count;              // 副本数量
    uint64_t replica_nodes[DISTFS_DEFAULT_REPLICAS]; // 副本节点ID
    uint64_t version;                   // 版本号
    uint8_t *data;                      // 数据内容指针
} distfs_data_block_t;

/* 集群状态结构 */
typedef struct {
    uint32_t total_nodes;               // 总节点数
    uint32_t online_nodes;              // 在线节点数
    uint32_t metadata_nodes;            // 元数据节点数
    uint32_t storage_nodes;             // 存储节点数
    uint64_t total_capacity;            // 总容量
    uint64_t used_capacity;             // 已使用容量
    uint64_t total_files;               // 总文件数
    double load_factor;                 // 负载因子
} distfs_cluster_status_t;

/* 性能统计结构 */
typedef struct {
    uint64_t read_ops;                  // 读操作次数
    uint64_t write_ops;                 // 写操作次数
    uint64_t read_bytes;                // 读取字节数
    uint64_t write_bytes;               // 写入字节数
    uint64_t read_latency_avg;          // 平均读延迟(微秒)
    uint64_t write_latency_avg;         // 平均写延迟(微秒)
    uint64_t network_in;                // 网络入流量
    uint64_t network_out;               // 网络出流量
    uint32_t active_connections;        // 活跃连接数
    double cpu_usage;                   // CPU使用率
    double memory_usage;                // 内存使用率
} distfs_stats_t;

/* 配置结构 */
typedef struct {
    char config_file[DISTFS_MAX_PATH_LEN];  // 配置文件路径
    uint16_t listen_port;                   // 监听端口
    uint32_t max_connections;               // 最大连接数
    uint32_t thread_pool_size;              // 线程池大小
    uint32_t replica_count;                 // 副本数量
    uint32_t block_size;                    // 块大小
    uint32_t heartbeat_interval;            // 心跳间隔(秒)
    uint32_t timeout;                       // 超时时间(秒)
    char data_dir[DISTFS_MAX_PATH_LEN];     // 数据目录
    char log_file[DISTFS_MAX_PATH_LEN];     // 日志文件
    int log_level;                          // 日志级别
    bool enable_compression;                // 启用压缩
    bool enable_encryption;                 // 启用加密
} distfs_config_t;

/* 客户端句柄 */
typedef struct distfs_client distfs_client_t;

/* 文件句柄 */
typedef struct {
    int fd;                             // 文件描述符
    uint64_t inode;                     // 文件inode
    int flags;                          // 打开标志
    off_t offset;                       // 当前偏移量
    distfs_client_t *client;            // 客户端句柄
} distfs_file_t;

/* 目录项结构 */
typedef struct {
    uint64_t inode;                     // inode号
    char name[DISTFS_MAX_NAME_LEN + 1]; // 文件名
    distfs_file_type_t type;            // 文件类型
} distfs_dirent_t;

/* 客户端API接口 */

/* 系统初始化和清理 */
distfs_client_t* distfs_init(const char *config_file);
int distfs_cleanup(distfs_client_t *client);

/* 文件操作接口 */
int distfs_create(distfs_client_t *client, const char *path, mode_t mode);
distfs_file_t* distfs_open(distfs_client_t *client, const char *path, int flags);
ssize_t distfs_read(distfs_file_t *file, void *buf, size_t count);
ssize_t distfs_write(distfs_file_t *file, const void *buf, size_t count);
off_t distfs_lseek(distfs_file_t *file, off_t offset, int whence);
int distfs_close(distfs_file_t *file);
int distfs_unlink(distfs_client_t *client, const char *path);

/* 目录操作接口 */
int distfs_mkdir(distfs_client_t *client, const char *path, mode_t mode);
int distfs_rmdir(distfs_client_t *client, const char *path);
int distfs_readdir(distfs_client_t *client, const char *path, 
                   distfs_dirent_t **entries, int *count);

/* 元数据接口 */
int distfs_stat(distfs_client_t *client, const char *path, struct stat *buf);
int distfs_chmod(distfs_client_t *client, const char *path, mode_t mode);
int distfs_chown(distfs_client_t *client, const char *path, uid_t owner, gid_t group);

/* 集群管理接口 */
int distfs_add_node(distfs_client_t *client, const char *node_addr);
int distfs_remove_node(distfs_client_t *client, const char *node_addr);
int distfs_get_cluster_status(distfs_client_t *client, distfs_cluster_status_t *status);

/* 监控接口 */
int distfs_get_stats(distfs_client_t *client, distfs_stats_t *stats);
int distfs_get_node_info(distfs_client_t *client, const char *node_addr, 
                         distfs_node_info_t *info);

/* 工具函数 */
const char* distfs_strerror(distfs_error_t error);
uint64_t distfs_get_timestamp(void);
uint32_t distfs_calculate_checksum(const void *data, size_t size);

/* ========== 配置管理API ========== */
typedef struct distfs_config_manager distfs_config_manager_t;

int distfs_config_init(const char *config_file);
void distfs_config_cleanup(void);
int distfs_config_load(const char *config_file);
const char* distfs_config_get_string(const char *key, const char *default_value);
int distfs_config_get_int(const char *key, int default_value);
bool distfs_config_get_bool(const char *key, bool default_value);
int distfs_config_set(const char *key, const char *value);
bool distfs_config_is_modified(void);
int distfs_config_reload(void);

/* ========== 日志系统API ========== */
typedef struct distfs_logger distfs_logger_t;

/* 日志级别定义 */
typedef enum {
    DISTFS_LOG_TRACE = 0,
    DISTFS_LOG_DEBUG = 1,
    DISTFS_LOG_INFO = 2,
    DISTFS_LOG_WARN = 3,
    DISTFS_LOG_ERROR = 4,
    DISTFS_LOG_FATAL = 5
} distfs_log_level_t;

int distfs_log_init(const char *log_file, int level);
void distfs_log_cleanup(void);
void distfs_log_set_level(int level);
int distfs_log_get_level(void);
void distfs_log_set_options(bool use_color, bool use_timestamp);
void distfs_log_set_rotation(uint64_t max_file_size, int max_backup_files);
void distfs_log_write(int level, const char *file, int line, const char *func,
                      const char *format, ...);
void distfs_log_hex(int level, const char *file, int line, const char *func,
                    const char *prefix, const void *data, size_t size);
bool distfs_log_is_enabled(int level);

/* 日志宏定义 */
#define DISTFS_LOG_TRACE(...) \
    distfs_log_write(DISTFS_LOG_TRACE, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define DISTFS_LOG_DEBUG(...) \
    distfs_log_write(DISTFS_LOG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define DISTFS_LOG_INFO(...) \
    distfs_log_write(DISTFS_LOG_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define DISTFS_LOG_WARN(...) \
    distfs_log_write(DISTFS_LOG_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define DISTFS_LOG_ERROR(...) \
    distfs_log_write(DISTFS_LOG_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define DISTFS_LOG_FATAL(...) \
    distfs_log_write(DISTFS_LOG_FATAL, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define DISTFS_LOG_HEX(level, prefix, data, size) \
    distfs_log_hex(level, __FILE__, __LINE__, __func__, prefix, data, size)

/* ========== 内存管理API ========== */
typedef struct distfs_memory_manager distfs_memory_manager_t;

typedef struct {
    uint64_t total_allocated;
    uint64_t total_freed;
    uint64_t current_usage;
    uint64_t peak_usage;
    uint64_t allocation_count;
    uint64_t free_count;
    uint64_t pool_hits;
    uint64_t pool_misses;
} distfs_memory_stats_t;

int distfs_memory_init(void);
void distfs_memory_cleanup(void);
void* distfs_malloc(size_t size);
void distfs_free(void *ptr);
void* distfs_realloc(void *ptr, size_t new_size);
void* distfs_calloc(size_t count, size_t size);
int distfs_memory_get_stats(distfs_memory_stats_t *stats);

/* ========== 哈希算法API ========== */
typedef struct distfs_hash_ring distfs_hash_ring_t;

uint32_t distfs_hash_crc32(const void *data, size_t len);
uint32_t distfs_hash_fnv1a(const void *data, size_t len);
uint32_t distfs_hash_murmur3(const void *data, size_t len, uint32_t seed);
uint32_t distfs_hash_string(const char *str);
uint64_t distfs_hash64(const void *data, size_t len);

distfs_hash_ring_t* distfs_hash_ring_create(int virtual_nodes);
void distfs_hash_ring_destroy(distfs_hash_ring_t *ring);
int distfs_hash_ring_add_node(distfs_hash_ring_t *ring, const char *node_id, void *data);
int distfs_hash_ring_remove_node(distfs_hash_ring_t *ring, const char *node_id);
const char* distfs_hash_ring_get_node(distfs_hash_ring_t *ring, const void *key, size_t key_len);
int distfs_hash_ring_get_nodes(distfs_hash_ring_t *ring, const void *key, size_t key_len,
                               const char **nodes, int max_nodes);

/* ========== 元数据服务器API ========== */
typedef struct distfs_metadata_server distfs_metadata_server_t;

distfs_metadata_server_t* distfs_metadata_server_create(const distfs_config_t *config);
int distfs_metadata_server_start(distfs_metadata_server_t *server);
int distfs_metadata_server_stop(distfs_metadata_server_t *server);
void distfs_metadata_server_destroy(distfs_metadata_server_t *server);

/* ========== 存储节点API ========== */
typedef struct distfs_storage_node distfs_storage_node_t;

distfs_storage_node_t* distfs_storage_node_create(const char *node_id,
                                                  const char *data_dir,
                                                  uint16_t port);
int distfs_storage_node_start(distfs_storage_node_t *node);
int distfs_storage_node_stop(distfs_storage_node_t *node);
void distfs_storage_node_destroy(distfs_storage_node_t *node);

/* ========== 块管理器API ========== */
typedef struct distfs_block_manager distfs_block_manager_t;

typedef struct {
    uint64_t total_blocks;
    uint64_t free_blocks;
    uint64_t used_blocks;
    uint64_t block_size;
    uint64_t allocations;
    uint64_t deallocations;
    uint64_t reads;
    uint64_t writes;
} distfs_block_stats_t;

distfs_block_manager_t* distfs_block_manager_create(const char *data_dir,
                                                   uint64_t block_size,
                                                   uint64_t total_blocks);
void distfs_block_manager_destroy(distfs_block_manager_t *manager);
int distfs_block_manager_sync(distfs_block_manager_t *manager);
int distfs_block_manager_get_stats(distfs_block_manager_t *manager,
                                  distfs_block_stats_t *stats);

uint64_t distfs_block_allocate(distfs_block_manager_t *manager);
int distfs_block_free(distfs_block_manager_t *manager, uint64_t block_id);
bool distfs_block_is_allocated(distfs_block_manager_t *manager, uint64_t block_id);
uint64_t distfs_block_get_free_count(distfs_block_manager_t *manager);
int distfs_block_allocate_batch(distfs_block_manager_t *manager,
                               uint64_t count, uint64_t *block_ids);

/* ========== 复制管理器API ========== */
typedef struct distfs_replication_manager distfs_replication_manager_t;

distfs_replication_manager_t* distfs_replication_manager_create(int replica_count,
                                                               int worker_count);
int distfs_replication_manager_start(distfs_replication_manager_t *manager);
int distfs_replication_manager_stop(distfs_replication_manager_t *manager);
void distfs_replication_manager_destroy(distfs_replication_manager_t *manager);

/* ========== 磁盘I/O管理器API ========== */
typedef struct distfs_disk_io_manager distfs_disk_io_manager_t;
typedef struct io_request io_request_t;

typedef struct {
    uint64_t total_reads;
    uint64_t total_writes;
    uint64_t total_syncs;
    uint64_t bytes_read;
    uint64_t bytes_written;
    uint64_t pending_requests;
    uint64_t completed_requests;
    uint64_t failed_requests;
    uint64_t avg_read_latency;
    uint64_t avg_write_latency;
} distfs_disk_io_stats_t;

distfs_disk_io_manager_t* distfs_disk_io_manager_create(int worker_count,
                                                       int max_concurrent_requests);
int distfs_disk_io_manager_start(distfs_disk_io_manager_t *manager);
int distfs_disk_io_manager_stop(distfs_disk_io_manager_t *manager);
void distfs_disk_io_manager_destroy(distfs_disk_io_manager_t *manager);

int distfs_disk_io_read_async(distfs_disk_io_manager_t *manager, int fd,
                             void *buffer, size_t size, off_t offset,
                             void (*callback)(io_request_t*, int, void*),
                             void *user_data);
int distfs_disk_io_write_async(distfs_disk_io_manager_t *manager, int fd,
                              const void *buffer, size_t size, off_t offset,
                              void (*callback)(io_request_t*, int, void*),
                              void *user_data);
int distfs_disk_io_get_stats(distfs_disk_io_manager_t *manager,
                            distfs_disk_io_stats_t *stats);

/* ========== 网络服务器API ========== */
typedef struct distfs_network_server distfs_network_server_t;
typedef int (*distfs_message_handler_t)(distfs_connection_t *conn,
                                        distfs_message_t *message,
                                        void *user_data);

distfs_network_server_t* distfs_network_server_create(uint16_t port, int max_connections,
                                                     distfs_message_handler_t handler,
                                                     void *user_data);
int distfs_network_server_start(distfs_network_server_t *server);
int distfs_network_server_stop(distfs_network_server_t *server);
void distfs_network_server_destroy(distfs_network_server_t *server);

/* ========== 连接池API ========== */
typedef struct distfs_connection_pool distfs_connection_pool_t;

typedef struct {
    int max_connections;
    int current_connections;
    uint64_t total_created;
    uint64_t total_destroyed;
    uint64_t total_requests;
    uint64_t cache_hits;
    uint64_t cache_misses;
    double hit_rate;
} distfs_connection_pool_stats_t;

distfs_connection_pool_t* distfs_connection_pool_create(int max_connections);
distfs_connection_t* distfs_connection_pool_get(distfs_connection_pool_t *pool,
                                               const char *hostname, uint16_t port);
int distfs_connection_pool_return(distfs_connection_pool_t *pool,
                                 distfs_connection_t *conn);
void distfs_connection_pool_destroy(distfs_connection_pool_t *pool);
int distfs_connection_pool_get_stats(distfs_connection_pool_t *pool,
                                    distfs_connection_pool_stats_t *stats);

/* ========== 客户端API ========== */
typedef struct distfs_client_context distfs_client_context_t;
typedef struct distfs_file_handle distfs_file_handle_t;
typedef struct distfs_dir_handle distfs_dir_handle_t;

distfs_client_context_t* distfs_client_create(const char *metadata_server, uint16_t metadata_port);
void distfs_client_destroy(distfs_client_context_t *ctx);

int distfs_create_file(distfs_client_context_t *ctx, const char *path, mode_t mode);
distfs_file_handle_t* distfs_open_file(distfs_client_context_t *ctx, const char *path, int flags);
ssize_t distfs_read_file(distfs_file_handle_t *handle, void *buffer, size_t size);
ssize_t distfs_write_file(distfs_file_handle_t *handle, const void *buffer, size_t size);
off_t distfs_seek_file(distfs_file_handle_t *handle, off_t offset, int whence);
int distfs_close_file(distfs_file_handle_t *handle);
int distfs_delete_file(distfs_client_context_t *ctx, const char *path);

/* ========== 缓存API ========== */
typedef struct distfs_cache distfs_cache_t;

typedef struct {
    size_t max_size;
    size_t current_size;
    int max_entries;
    int current_entries;
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
    uint64_t insertions;
    double hit_rate;
} distfs_cache_stats_t;

distfs_cache_t* distfs_cache_create(size_t max_size, int max_entries, int ttl);
int distfs_cache_put(distfs_cache_t *cache, const char *key, const void *data, size_t size);
int distfs_cache_get(distfs_cache_t *cache, const char *key, void **data, size_t *size);
int distfs_cache_remove(distfs_cache_t *cache, const char *key);
void distfs_cache_clear(distfs_cache_t *cache);
int distfs_cache_get_stats(distfs_cache_t *cache, distfs_cache_stats_t *stats);
void distfs_cache_destroy(distfs_cache_t *cache);

/* ========== 工具函数API ========== */
const char* distfs_strerror(int error_code);
uint64_t distfs_get_timestamp(void);
uint64_t distfs_get_timestamp_sec(void);
uint32_t distfs_calculate_checksum(const void *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* DISTFS_H */
