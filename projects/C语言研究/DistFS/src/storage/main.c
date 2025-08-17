/*
 * DistFS 存储节点主程序
 * 启动和管理存储节点服务
 */

#include "distfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <errno.h>

/* 全局变量 */
static distfs_storage_node_t *g_storage_node = NULL;
static distfs_block_manager_t *g_block_manager = NULL;
static distfs_replication_manager_t *g_replication_manager = NULL;
static distfs_disk_io_manager_t *g_disk_io_manager = NULL;
static volatile bool g_running = true;

/* 信号处理函数 */
static void signal_handler(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            printf("\nReceived signal %d, shutting down...\n", sig);
            g_running = false;
            break;
        case SIGHUP:
            printf("Received SIGHUP, reloading configuration...\n");
            // TODO: 重新加载配置
            break;
        default:
            break;
    }
}

/* 设置信号处理 */
static void setup_signal_handlers(void) {
    struct sigaction sa;
    
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    
    // 忽略SIGPIPE
    signal(SIGPIPE, SIG_IGN);
}

/* 创建守护进程 */
static int daemonize(void) {
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    
    if (pid > 0) {
        exit(0); // 父进程退出
    }
    
    // 子进程继续
    if (setsid() < 0) {
        perror("setsid");
        return -1;
    }
    
    // 第二次fork
    pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    
    if (pid > 0) {
        exit(0);
    }
    
    // 改变工作目录
    if (chdir("/") < 0) {
        perror("chdir");
        return -1;
    }
    
    // 关闭标准文件描述符
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    // 重定向到/dev/null
    open("/dev/null", O_RDONLY); // stdin
    open("/dev/null", O_WRONLY); // stdout
    open("/dev/null", O_WRONLY); // stderr
    
    return 0;
}

/* 写入PID文件 */
static int write_pid_file(const char *pid_file) {
    FILE *fp = fopen(pid_file, "w");
    if (!fp) {
        fprintf(stderr, "Failed to create PID file %s: %s\n", 
                pid_file, strerror(errno));
        return -1;
    }
    
    fprintf(fp, "%d\n", getpid());
    fclose(fp);
    
    return 0;
}

/* 显示帮助信息 */
static void show_help(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\nOptions:\n");
    printf("  -c, --config FILE     Configuration file path\n");
    printf("  -n, --node-id ID      Node identifier\n");
    printf("  -d, --data-dir DIR    Data directory path\n");
    printf("  -p, --port PORT       Listen port (default: 9528)\n");
    printf("  -s, --block-size SIZE Block size in bytes (default: 4096)\n");
    printf("  -b, --total-blocks N  Total number of blocks (default: 1000000)\n");
    printf("  -r, --replicas N      Number of replicas (default: 3)\n");
    printf("  -w, --workers N       Number of worker threads (default: 4)\n");
    printf("  -D, --daemon          Run as daemon\n");
    printf("  -P, --pid-file FILE   PID file path\n");
    printf("  -l, --log-file FILE   Log file path\n");
    printf("  -L, --log-level LEVEL Log level (0-5, default: 2)\n");
    printf("  -h, --help            Show this help message\n");
    printf("  -v, --version         Show version information\n");
}

/* 显示版本信息 */
static void show_version(void) {
    printf("DistFS Storage Node v1.0.0\n");
    printf("Built on %s %s\n", __DATE__, __TIME__);
    printf("Copyright (C) 2025 DistFS Project\n");
}

/* 主函数 */
int main(int argc, char *argv[]) {
    const char *config_file = NULL;
    const char *node_id = NULL;
    const char *data_dir = "/tmp/distfs_storage";
    const char *pid_file = NULL;
    const char *log_file = NULL;
    int port = 9528;
    uint64_t block_size = 4096;
    uint64_t total_blocks = 1000000;
    int replica_count = 3;
    int worker_count = 4;
    int log_level = 2;
    bool daemon_mode = false;
    
    /* 解析命令行参数 */
    static struct option long_options[] = {
        {"config",       required_argument, 0, 'c'},
        {"node-id",      required_argument, 0, 'n'},
        {"data-dir",     required_argument, 0, 'd'},
        {"port",         required_argument, 0, 'p'},
        {"block-size",   required_argument, 0, 's'},
        {"total-blocks", required_argument, 0, 'b'},
        {"replicas",     required_argument, 0, 'r'},
        {"workers",      required_argument, 0, 'w'},
        {"daemon",       no_argument,       0, 'D'},
        {"pid-file",     required_argument, 0, 'P'},
        {"log-file",     required_argument, 0, 'l'},
        {"log-level",    required_argument, 0, 'L'},
        {"help",         no_argument,       0, 'h'},
        {"version",      no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "c:n:d:p:s:b:r:w:DP:l:L:hv", long_options, NULL)) != -1) {
        switch (c) {
            case 'c':
                config_file = optarg;
                break;
            case 'n':
                node_id = optarg;
                break;
            case 'd':
                data_dir = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    return 1;
                }
                break;
            case 's':
                block_size = strtoull(optarg, NULL, 10);
                if (block_size == 0) {
                    fprintf(stderr, "Invalid block size: %s\n", optarg);
                    return 1;
                }
                break;
            case 'b':
                total_blocks = strtoull(optarg, NULL, 10);
                if (total_blocks == 0) {
                    fprintf(stderr, "Invalid total blocks: %s\n", optarg);
                    return 1;
                }
                break;
            case 'r':
                replica_count = atoi(optarg);
                if (replica_count <= 0) {
                    fprintf(stderr, "Invalid replica count: %s\n", optarg);
                    return 1;
                }
                break;
            case 'w':
                worker_count = atoi(optarg);
                if (worker_count <= 0) {
                    fprintf(stderr, "Invalid worker count: %s\n", optarg);
                    return 1;
                }
                break;
            case 'D':
                daemon_mode = true;
                break;
            case 'P':
                pid_file = optarg;
                break;
            case 'l':
                log_file = optarg;
                break;
            case 'L':
                log_level = atoi(optarg);
                if (log_level < 0 || log_level > 5) {
                    fprintf(stderr, "Invalid log level: %s\n", optarg);
                    return 1;
                }
                break;
            case 'h':
                show_help(argv[0]);
                return 0;
            case 'v':
                show_version();
                return 0;
            case '?':
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return 1;
            default:
                break;
        }
    }
    
    /* 检查必需参数 */
    if (!node_id) {
        fprintf(stderr, "Node ID is required. Use -n or --node-id option.\n");
        return 1;
    }
    
    /* 初始化日志系统 */
    if (distfs_log_init(log_file, log_level) != DISTFS_SUCCESS) {
        fprintf(stderr, "Failed to initialize logging system\n");
        return 1;
    }
    
    DISTFS_LOG_INFO("Starting DistFS Storage Node v1.0.0");
    DISTFS_LOG_INFO("Node ID: %s", node_id);
    DISTFS_LOG_INFO("Data directory: %s", data_dir);
    DISTFS_LOG_INFO("Listen port: %d", port);
    
    /* 初始化配置系统 */
    if (distfs_config_init(config_file) != DISTFS_SUCCESS) {
        DISTFS_LOG_ERROR("Failed to initialize configuration system");
        distfs_log_cleanup();
        return 1;
    }
    
    /* 初始化内存管理系统 */
    if (distfs_memory_init() != DISTFS_SUCCESS) {
        DISTFS_LOG_ERROR("Failed to initialize memory management system");
        distfs_config_cleanup();
        distfs_log_cleanup();
        return 1;
    }

    /* 创建数据目录 */
    if (mkdir(data_dir, 0755) != 0 && errno != EEXIST) {
        DISTFS_LOG_ERROR("Failed to create data directory %s: %s",
                        data_dir, strerror(errno));
        distfs_memory_cleanup();
        distfs_config_cleanup();
        distfs_log_cleanup();
        return 1;
    }

    /* 守护进程模式 */
    if (daemon_mode) {
        DISTFS_LOG_INFO("Running in daemon mode");
        if (daemonize() != 0) {
            DISTFS_LOG_ERROR("Failed to daemonize");
            distfs_memory_cleanup();
            distfs_config_cleanup();
            distfs_log_cleanup();
            return 1;
        }
    }

    /* 写入PID文件 */
    if (pid_file) {
        if (write_pid_file(pid_file) != 0) {
            DISTFS_LOG_ERROR("Failed to write PID file");
            distfs_memory_cleanup();
            distfs_config_cleanup();
            distfs_log_cleanup();
            return 1;
        }
    }

    /* 设置信号处理 */
    setup_signal_handlers();

    /* 创建磁盘I/O管理器 */
    g_disk_io_manager = distfs_disk_io_manager_create(worker_count, 1000);
    if (!g_disk_io_manager) {
        DISTFS_LOG_ERROR("Failed to create disk I/O manager");
        if (pid_file) unlink(pid_file);
        distfs_memory_cleanup();
        distfs_config_cleanup();
        distfs_log_cleanup();
        return 1;
    }

    /* 创建块管理器 */
    g_block_manager = distfs_block_manager_create(data_dir, block_size, total_blocks);
    if (!g_block_manager) {
        DISTFS_LOG_ERROR("Failed to create block manager");
        distfs_disk_io_manager_destroy(g_disk_io_manager);
        if (pid_file) unlink(pid_file);
        distfs_memory_cleanup();
        distfs_config_cleanup();
        distfs_log_cleanup();
        return 1;
    }

    /* 创建复制管理器 */
    g_replication_manager = distfs_replication_manager_create(replica_count, worker_count);
    if (!g_replication_manager) {
        DISTFS_LOG_ERROR("Failed to create replication manager");
        distfs_block_manager_destroy(g_block_manager);
        distfs_disk_io_manager_destroy(g_disk_io_manager);
        if (pid_file) unlink(pid_file);
        distfs_memory_cleanup();
        distfs_config_cleanup();
        distfs_log_cleanup();
        return 1;
    }

    /* 创建存储节点 */
    g_storage_node = distfs_storage_node_create(node_id, data_dir, port);
    if (!g_storage_node) {
        DISTFS_LOG_ERROR("Failed to create storage node");
        distfs_replication_manager_destroy(g_replication_manager);
        distfs_block_manager_destroy(g_block_manager);
        distfs_disk_io_manager_destroy(g_disk_io_manager);
        if (pid_file) unlink(pid_file);
        distfs_memory_cleanup();
        distfs_config_cleanup();
        distfs_log_cleanup();
        return 1;
    }

    /* 启动各个组件 */
    DISTFS_LOG_INFO("Starting disk I/O manager...");
    if (distfs_disk_io_manager_start(g_disk_io_manager) != DISTFS_SUCCESS) {
        DISTFS_LOG_ERROR("Failed to start disk I/O manager");
        goto cleanup;
    }

    DISTFS_LOG_INFO("Starting replication manager...");
    if (distfs_replication_manager_start(g_replication_manager) != DISTFS_SUCCESS) {
        DISTFS_LOG_ERROR("Failed to start replication manager");
        goto cleanup;
    }

    DISTFS_LOG_INFO("Starting storage node...");
    if (distfs_storage_node_start(g_storage_node) != DISTFS_SUCCESS) {
        DISTFS_LOG_ERROR("Failed to start storage node");
        goto cleanup;
    }

    DISTFS_LOG_INFO("Storage node started successfully");

    /* 主循环 */
    while (g_running) {
        sleep(1);

        // 检查配置文件是否修改
        if (distfs_config_is_modified()) {
            DISTFS_LOG_INFO("Configuration file modified, reloading...");
            distfs_config_reload();
        }

        // 定期同步块管理器元数据
        static time_t last_sync = 0;
        time_t now = time(NULL);
        if (now - last_sync > 60) { // 每分钟同步一次
            distfs_block_manager_sync(g_block_manager);
            last_sync = now;
        }
    }

cleanup:
    /* 清理资源 */
    DISTFS_LOG_INFO("Shutting down storage node");

    if (g_storage_node) {
        distfs_storage_node_destroy(g_storage_node);
    }

    if (g_replication_manager) {
        distfs_replication_manager_destroy(g_replication_manager);
    }

    if (g_block_manager) {
        distfs_block_manager_destroy(g_block_manager);
    }

    if (g_disk_io_manager) {
        distfs_disk_io_manager_destroy(g_disk_io_manager);
    }

    if (pid_file) {
        unlink(pid_file);
    }

    distfs_memory_cleanup();
    distfs_config_cleanup();

    DISTFS_LOG_INFO("Storage node shutdown complete");
    distfs_log_cleanup();

    return 0;
}
