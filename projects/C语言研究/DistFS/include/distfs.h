/*
 * DistFS - 分布式文件系统核心头文件
 * 定义系统的核心数据结构、常量和API接口
 */

#ifndef DISTFS_H
#define DISTFS_H

#include <stdint.h>
#include <stddef.h>
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

#ifdef __cplusplus
}
#endif

#endif /* DISTFS_H */
