/*
 * DistFS 元数据服务器主程序
 * 启动和管理元数据服务器
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
static distfs_metadata_server_t *g_server = NULL;
static volatile bool g_running = true;

/* 信号处理函数 */
static void signal_handler(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            printf("\nReceived signal %d, shutting down...\n", sig);
            g_running = false;
            if (g_server) {
                distfs_metadata_server_stop(g_server);
            }
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
    printf("  -p, --port PORT       Listen port (default: 9527)\n");
    printf("  -d, --daemon          Run as daemon\n");
    printf("  -P, --pid-file FILE   PID file path\n");
    printf("  -l, --log-file FILE   Log file path\n");
    printf("  -L, --log-level LEVEL Log level (0-5, default: 2)\n");
    printf("  -h, --help            Show this help message\n");
    printf("  -v, --version         Show version information\n");
    printf("\nLog levels:\n");
    printf("  0 - TRACE\n");
    printf("  1 - DEBUG\n");
    printf("  2 - INFO\n");
    printf("  3 - WARN\n");
    printf("  4 - ERROR\n");
    printf("  5 - FATAL\n");
}

/* 显示版本信息 */
static void show_version(void) {
    printf("DistFS Metadata Server v1.0.0\n");
    printf("Built on %s %s\n", __DATE__, __TIME__);
    printf("Copyright (C) 2025 DistFS Project\n");
}

/* 主函数 */
int main(int argc, char *argv[]) {
    const char *config_file = NULL;
    const char *pid_file = NULL;
    const char *log_file = NULL;
    int port = 9527;
    int log_level = 2;
    bool daemon_mode = false;
    
    /* 解析命令行参数 */
    static struct option long_options[] = {
        {"config",    required_argument, 0, 'c'},
        {"port",      required_argument, 0, 'p'},
        {"daemon",    no_argument,       0, 'd'},
        {"pid-file",  required_argument, 0, 'P'},
        {"log-file",  required_argument, 0, 'l'},
        {"log-level", required_argument, 0, 'L'},
        {"help",      no_argument,       0, 'h'},
        {"version",   no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "c:p:dP:l:L:hv", long_options, NULL)) != -1) {
        switch (c) {
            case 'c':
                config_file = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    return 1;
                }
                break;
            case 'd':
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
    
    /* 初始化日志系统 */
    if (distfs_log_init(log_file, log_level) != DISTFS_SUCCESS) {
        fprintf(stderr, "Failed to initialize logging system\n");
        return 1;
    }
    
    DISTFS_LOG_INFO("Starting DistFS Metadata Server v1.0.0");
    
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
    
    /* 创建配置结构 */
    distfs_config_t config = {0};
    config.listen_port = port;
    config.max_connections = distfs_config_get_int("max_connections", 1000);
    config.thread_pool_size = distfs_config_get_int("thread_pool_size", 8);
    config.replica_count = distfs_config_get_int("replica_count", 3);
    config.heartbeat_interval = distfs_config_get_int("heartbeat_interval", 30);
    config.timeout = distfs_config_get_int("timeout", 60);
    
    const char *data_dir = distfs_config_get_string("data_dir", "/tmp/distfs");
    strncpy(config.data_dir, data_dir, sizeof(config.data_dir) - 1);
    
    /* 创建数据目录 */
    if (mkdir(config.data_dir, 0755) != 0 && errno != EEXIST) {
        DISTFS_LOG_ERROR("Failed to create data directory %s: %s", 
                        config.data_dir, strerror(errno));
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
    
    /* 创建元数据服务器 */
    g_server = distfs_metadata_server_create(&config);
    if (!g_server) {
        DISTFS_LOG_ERROR("Failed to create metadata server");
        if (pid_file) unlink(pid_file);
        distfs_memory_cleanup();
        distfs_config_cleanup();
        distfs_log_cleanup();
        return 1;
    }
    
    /* 启动服务器 */
    DISTFS_LOG_INFO("Starting metadata server on port %d", config.listen_port);
    if (distfs_metadata_server_start(g_server) != DISTFS_SUCCESS) {
        DISTFS_LOG_ERROR("Failed to start metadata server");
        distfs_metadata_server_destroy(g_server);
        if (pid_file) unlink(pid_file);
        distfs_memory_cleanup();
        distfs_config_cleanup();
        distfs_log_cleanup();
        return 1;
    }
    
    DISTFS_LOG_INFO("Metadata server started successfully");
    
    /* 主循环 */
    while (g_running) {
        sleep(1);
        
        // 检查配置文件是否修改
        if (distfs_config_is_modified()) {
            DISTFS_LOG_INFO("Configuration file modified, reloading...");
            distfs_config_reload();
        }
    }
    
    /* 清理资源 */
    DISTFS_LOG_INFO("Shutting down metadata server");
    
    if (g_server) {
        distfs_metadata_server_destroy(g_server);
    }
    
    if (pid_file) {
        unlink(pid_file);
    }
    
    distfs_memory_cleanup();
    distfs_config_cleanup();
    
    DISTFS_LOG_INFO("Metadata server shutdown complete");
    distfs_log_cleanup();
    
    return 0;
}
