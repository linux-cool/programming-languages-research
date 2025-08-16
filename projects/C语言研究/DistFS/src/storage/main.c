/*
 * DistFS 存储节点主程序
 * 启动和管理存储节点服务
 */

#include "distfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <sys/stat.h>

/* 全局配置 */
static struct {
    char config_file[1024];
    char data_dir[1024];
    uint16_t port;
    int log_level;
    char log_file[1024];
    bool daemon_mode;
    bool verbose;
} g_config = {
    .config_file = "",
    .data_dir = "/var/lib/distfs/storage",
    .port = 9530,
    .log_level = 2,  // INFO
    .log_file = "/var/log/distfs/storage.log",
    .daemon_mode = false,
    .verbose = false
};

/* 信号处理标志 */
static volatile bool g_running = true;

/* 信号处理函数 */
static void signal_handler(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            printf("\n收到终止信号，正在关闭存储节点...\n");
            g_running = false;
            break;
        case SIGHUP:
            printf("收到重载信号，重新加载配置...\n");
            // TODO: 重新加载配置
            break;
        default:
            break;
    }
}

/* 设置信号处理 */
static void setup_signal_handlers(void) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGPIPE, SIG_IGN);  // 忽略SIGPIPE
}

/* 显示帮助信息 */
static void show_help(const char *program_name) {
    printf("DistFS 存储节点 v1.0.0\n");
    printf("用法: %s [选项]\n\n", program_name);
    
    printf("选项:\n");
    printf("  -c, --config FILE     配置文件路径\n");
    printf("  -d, --data-dir DIR    数据存储目录 (默认: /var/lib/distfs/storage)\n");
    printf("  -p, --port PORT       监听端口 (默认: 9530)\n");
    printf("  -l, --log-file FILE   日志文件路径 (默认: /var/log/distfs/storage.log)\n");
    printf("  -L, --log-level LEVEL 日志级别 0-5 (默认: 2)\n");
    printf("  -D, --daemon          以守护进程模式运行\n");
    printf("  -v, --verbose         详细输出\n");
    printf("  -h, --help            显示此帮助信息\n\n");
    
    printf("日志级别:\n");
    printf("  0 - TRACE   详细跟踪信息\n");
    printf("  1 - DEBUG   调试信息\n");
    printf("  2 - INFO    一般信息 (默认)\n");
    printf("  3 - WARN    警告信息\n");
    printf("  4 - ERROR   错误信息\n");
    printf("  5 - FATAL   致命错误\n\n");
    
    printf("示例:\n");
    printf("  %s -d /data/distfs -p 9530\n", program_name);
    printf("  %s -c /etc/distfs/storage.conf -D\n", program_name);
    printf("  %s -v -L 1\n", program_name);
}

/* 加载配置文件 */
static int load_config(const char *config_file) {
    if (!config_file || strlen(config_file) == 0) {
        return DISTFS_SUCCESS;  // 使用默认配置
    }
    
    int result = distfs_config_load(config_file);
    if (result != DISTFS_SUCCESS) {
        fprintf(stderr, "错误: 无法加载配置文件 %s\n", config_file);
        return result;
    }
    
    // 读取配置项
    const char *data_dir = distfs_config_get_string("data_dir", g_config.data_dir);
    strncpy(g_config.data_dir, data_dir, sizeof(g_config.data_dir) - 1);
    
    g_config.port = distfs_config_get_int("listen_port", g_config.port);
    g_config.log_level = distfs_config_get_int("log_level", g_config.log_level);
    
    const char *log_file = distfs_config_get_string("log_file", g_config.log_file);
    strncpy(g_config.log_file, log_file, sizeof(g_config.log_file) - 1);
    
    g_config.daemon_mode = distfs_config_get_bool("daemon_mode", g_config.daemon_mode);
    
    return DISTFS_SUCCESS;
}

/* 验证配置 */
static int validate_config(void) {
    // 检查数据目录
    if (strlen(g_config.data_dir) == 0) {
        fprintf(stderr, "错误: 数据目录不能为空\n");
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    // 检查端口范围
    if (g_config.port < 1024 || g_config.port > 65535) {
        fprintf(stderr, "错误: 端口号必须在1024-65535范围内\n");
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    // 检查日志级别
    if (g_config.log_level < 0 || g_config.log_level > 5) {
        fprintf(stderr, "错误: 日志级别必须在0-5范围内\n");
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    return DISTFS_SUCCESS;
}

/* 创建守护进程 */
static int daemonize(void) {
    pid_t pid = fork();
    
    if (pid < 0) {
        return DISTFS_ERROR_UNKNOWN;
    }
    
    if (pid > 0) {
        exit(0);  // 父进程退出
    }
    
    // 子进程继续
    if (setsid() < 0) {
        return DISTFS_ERROR_UNKNOWN;
    }
    
    // 第二次fork
    pid = fork();
    if (pid < 0) {
        return DISTFS_ERROR_UNKNOWN;
    }
    
    if (pid > 0) {
        exit(0);  // 第一个子进程退出
    }
    
    // 改变工作目录
    chdir("/");
    
    // 关闭标准文件描述符
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    return DISTFS_SUCCESS;
}

/* 显示启动信息 */
static void show_startup_info(void) {
    printf("DistFS 存储节点启动中...\n");
    printf("配置信息:\n");
    printf("  数据目录: %s\n", g_config.data_dir);
    printf("  监听端口: %u\n", g_config.port);
    printf("  日志文件: %s\n", g_config.log_file);
    printf("  日志级别: %d\n", g_config.log_level);
    printf("  守护模式: %s\n", g_config.daemon_mode ? "是" : "否");
    printf("  详细输出: %s\n", g_config.verbose ? "是" : "否");
    printf("\n");
}

/* 主函数 */
int main(int argc, char *argv[]) {
    // 解析命令行参数
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"data-dir", required_argument, 0, 'd'},
        {"port", required_argument, 0, 'p'},
        {"log-file", required_argument, 0, 'l'},
        {"log-level", required_argument, 0, 'L'},
        {"daemon", no_argument, 0, 'D'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "c:d:p:l:L:Dvh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                strncpy(g_config.config_file, optarg, sizeof(g_config.config_file) - 1);
                break;
            case 'd':
                strncpy(g_config.data_dir, optarg, sizeof(g_config.data_dir) - 1);
                break;
            case 'p':
                g_config.port = atoi(optarg);
                break;
            case 'l':
                strncpy(g_config.log_file, optarg, sizeof(g_config.log_file) - 1);
                break;
            case 'L':
                g_config.log_level = atoi(optarg);
                break;
            case 'D':
                g_config.daemon_mode = true;
                break;
            case 'v':
                g_config.verbose = true;
                break;
            case 'h':
                show_help(argv[0]);
                return 0;
            default:
                show_help(argv[0]);
                return 1;
        }
    }
    
    // 加载配置文件
    if (load_config(g_config.config_file[0] ? g_config.config_file : NULL) != DISTFS_SUCCESS) {
        return 1;
    }
    
    // 验证配置
    if (validate_config() != DISTFS_SUCCESS) {
        return 1;
    }
    
    // 显示启动信息
    if (!g_config.daemon_mode) {
        show_startup_info();
    }
    
    // 初始化日志系统
    if (distfs_log_init(g_config.log_file, g_config.log_level) != DISTFS_SUCCESS) {
        fprintf(stderr, "错误: 无法初始化日志系统\n");
        return 1;
    }
    
    // 设置控制台输出
    distfs_log_set_console(!g_config.daemon_mode);
    distfs_log_set_timestamp(true);
    distfs_log_set_file_line(g_config.verbose);
    
    // 创建守护进程
    if (g_config.daemon_mode) {
        if (daemonize() != DISTFS_SUCCESS) {
            fprintf(stderr, "错误: 无法创建守护进程\n");
            return 1;
        }
    }
    
    // 设置信号处理
    setup_signal_handlers();
    
    // 初始化存储节点
    printf("初始化存储节点...\n");
    if (distfs_storage_node_init(g_config.data_dir, g_config.port) != DISTFS_SUCCESS) {
        fprintf(stderr, "错误: 无法初始化存储节点\n");
        distfs_log_cleanup();
        return 1;
    }
    
    // 启动存储节点
    printf("启动存储节点服务...\n");
    if (distfs_storage_node_start() != DISTFS_SUCCESS) {
        fprintf(stderr, "错误: 无法启动存储节点\n");
        distfs_storage_node_cleanup();
        distfs_log_cleanup();
        return 1;
    }
    
    printf("存储节点已启动，监听端口 %u\n", g_config.port);
    printf("数据目录: %s\n", g_config.data_dir);
    printf("按 Ctrl+C 停止服务\n\n");
    
    // 主循环
    while (g_running) {
        sleep(1);
    }
    
    // 清理资源
    printf("正在停止存储节点...\n");
    distfs_storage_node_cleanup();
    distfs_config_cleanup();
    distfs_log_cleanup();
    
    printf("存储节点已停止\n");
    return 0;
}
