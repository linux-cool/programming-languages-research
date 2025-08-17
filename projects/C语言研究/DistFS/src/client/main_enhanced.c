/*
 * DistFS 增强客户端主程序
 * 提供完整的命令行工具接口
 */

#include "distfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

/* 全局客户端上下文 */
static distfs_client_context_t *g_client = NULL;

/* 显示帮助信息 */
static void show_help(const char *program_name) {
    printf("DistFS Client v1.0.0\n");
    printf("Usage: %s [OPTIONS] COMMAND [ARGS...]\n", program_name);
    printf("\nOptions:\n");
    printf("  -s, --server HOST     Metadata server hostname (default: localhost)\n");
    printf("  -p, --port PORT       Metadata server port (default: 9527)\n");
    printf("  -v, --verbose         Enable verbose output\n");
    printf("  -h, --help            Show this help message\n");
    printf("  --version             Show version information\n");
    printf("\nCommands:\n");
    printf("  ls [PATH]             List directory contents\n");
    printf("  mkdir PATH            Create directory\n");
    printf("  rmdir PATH            Remove directory\n");
    printf("  touch PATH            Create empty file\n");
    printf("  rm PATH               Remove file\n");
    printf("  cp SRC DST            Copy file\n");
    printf("  mv SRC DST            Move file\n");
    printf("  cat PATH              Display file contents\n");
    printf("  put LOCAL REMOTE      Upload local file to DistFS\n");
    printf("  get REMOTE LOCAL      Download file from DistFS\n");
    printf("  stat PATH             Show file information\n");
    printf("  df                    Show filesystem usage\n");
}

/* 显示版本信息 */
static void show_version(void) {
    printf("DistFS Client v1.0.0\n");
    printf("Built on %s %s\n", __DATE__, __TIME__);
    printf("Copyright (C) 2025 DistFS Project\n");
}

/* 处理ls命令 */
static int cmd_ls(int argc, char *argv[]) {
    const char *path = (argc > 0) ? argv[0] : "/";
    
    printf("Listing directory: %s\n", path);
    // TODO: 实现目录列表功能
    
    return 0;
}

/* 处理mkdir命令 */
static int cmd_mkdir(int argc, char *argv[]) {
    if (argc < 1) {
        fprintf(stderr, "mkdir: missing directory name\n");
        return 1;
    }
    
    const char *path = argv[0];
    printf("Creating directory: %s\n", path);
    // TODO: 实现创建目录功能
    
    return 0;
}

/* 处理touch命令 */
static int cmd_touch(int argc, char *argv[]) {
    if (argc < 1) {
        fprintf(stderr, "touch: missing file name\n");
        return 1;
    }
    
    const char *path = argv[0];
    printf("Creating file: %s\n", path);
    
    int result = distfs_create_file(g_client, path, 0644);
    if (result != DISTFS_SUCCESS) {
        fprintf(stderr, "touch: failed to create file %s: %s\n", 
                path, distfs_strerror(result));
        return 1;
    }
    
    printf("File created successfully\n");
    return 0;
}

/* 处理rm命令 */
static int cmd_rm(int argc, char *argv[]) {
    if (argc < 1) {
        fprintf(stderr, "rm: missing file name\n");
        return 1;
    }
    
    const char *path = argv[0];
    printf("Removing file: %s\n", path);
    
    int result = distfs_delete_file(g_client, path);
    if (result != DISTFS_SUCCESS) {
        fprintf(stderr, "rm: failed to remove file %s: %s\n", 
                path, distfs_strerror(result));
        return 1;
    }
    
    printf("File removed successfully\n");
    return 0;
}

/* 处理cat命令 */
static int cmd_cat(int argc, char *argv[]) {
    if (argc < 1) {
        fprintf(stderr, "cat: missing file name\n");
        return 1;
    }
    
    const char *path = argv[0];
    
    distfs_file_handle_t *handle = distfs_open_file(g_client, path, O_RDONLY);
    if (!handle) {
        fprintf(stderr, "cat: failed to open file %s\n", path);
        return 1;
    }
    
    char buffer[4096];
    ssize_t bytes_read;
    
    while ((bytes_read = distfs_read_file(handle, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, bytes_read, stdout);
    }
    
    distfs_close_file(handle);
    return 0;
}

/* 处理put命令 */
static int cmd_put(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "put: missing source or destination\n");
        return 1;
    }
    
    const char *local_path = argv[0];
    const char *remote_path = argv[1];
    
    printf("Uploading %s to %s\n", local_path, remote_path);
    
    // 打开本地文件
    FILE *local_file = fopen(local_path, "rb");
    if (!local_file) {
        fprintf(stderr, "put: failed to open local file %s: %s\n", 
                local_path, strerror(errno));
        return 1;
    }
    
    // 创建远程文件
    int result = distfs_create_file(g_client, remote_path, 0644);
    if (result != DISTFS_SUCCESS) {
        fprintf(stderr, "put: failed to create remote file %s: %s\n", 
                remote_path, distfs_strerror(result));
        fclose(local_file);
        return 1;
    }
    
    // 打开远程文件
    distfs_file_handle_t *remote_handle = distfs_open_file(g_client, remote_path, O_WRONLY);
    if (!remote_handle) {
        fprintf(stderr, "put: failed to open remote file %s\n", remote_path);
        fclose(local_file);
        return 1;
    }
    
    // 复制数据
    char buffer[4096];
    size_t bytes_read;
    size_t total_bytes = 0;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), local_file)) > 0) {
        ssize_t bytes_written = distfs_write_file(remote_handle, buffer, bytes_read);
        if (bytes_written != (ssize_t)bytes_read) {
            fprintf(stderr, "put: write error\n");
            break;
        }
        total_bytes += bytes_written;
    }
    
    fclose(local_file);
    distfs_close_file(remote_handle);
    
    printf("Uploaded %zu bytes\n", total_bytes);
    return 0;
}

/* 处理get命令 */
static int cmd_get(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "get: missing source or destination\n");
        return 1;
    }
    
    const char *remote_path = argv[0];
    const char *local_path = argv[1];
    
    printf("Downloading %s to %s\n", remote_path, local_path);
    
    // 打开远程文件
    distfs_file_handle_t *remote_handle = distfs_open_file(g_client, remote_path, O_RDONLY);
    if (!remote_handle) {
        fprintf(stderr, "get: failed to open remote file %s\n", remote_path);
        return 1;
    }
    
    // 创建本地文件
    FILE *local_file = fopen(local_path, "wb");
    if (!local_file) {
        fprintf(stderr, "get: failed to create local file %s: %s\n", 
                local_path, strerror(errno));
        distfs_close_file(remote_handle);
        return 1;
    }
    
    // 复制数据
    char buffer[4096];
    ssize_t bytes_read;
    size_t total_bytes = 0;
    
    while ((bytes_read = distfs_read_file(remote_handle, buffer, sizeof(buffer))) > 0) {
        size_t bytes_written = fwrite(buffer, 1, bytes_read, local_file);
        if (bytes_written != (size_t)bytes_read) {
            fprintf(stderr, "get: write error\n");
            break;
        }
        total_bytes += bytes_written;
    }
    
    distfs_close_file(remote_handle);
    fclose(local_file);
    
    printf("Downloaded %zu bytes\n", total_bytes);
    return 0;
}

/* 命令分发表 */
static struct {
    const char *name;
    int (*handler)(int argc, char *argv[]);
    const char *description;
} commands[] = {
    {"ls",    cmd_ls,    "List directory contents"},
    {"mkdir", cmd_mkdir, "Create directory"},
    {"touch", cmd_touch, "Create empty file"},
    {"rm",    cmd_rm,    "Remove file"},
    {"cat",   cmd_cat,   "Display file contents"},
    {"put",   cmd_put,   "Upload file"},
    {"get",   cmd_get,   "Download file"},
    {NULL,    NULL,      NULL}
};

/* 主函数 */
int main(int argc, char *argv[]) {
    const char *server = "localhost";
    int port = 9527;
    bool verbose = false;
    
    /* 解析命令行选项 */
    static struct option long_options[] = {
        {"server",  required_argument, 0, 's'},
        {"port",    required_argument, 0, 'p'},
        {"verbose", no_argument,       0, 'v'},
        {"help",    no_argument,       0, 'h'},
        {"version", no_argument,       0, 'V'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "s:p:vh", long_options, NULL)) != -1) {
        switch (c) {
            case 's':
                server = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    return 1;
                }
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                show_help(argv[0]);
                return 0;
            case 'V':
                show_version();
                return 0;
            case '?':
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return 1;
            default:
                break;
        }
    }
    
    /* 检查是否有命令 */
    if (optind >= argc) {
        fprintf(stderr, "No command specified.\n");
        show_help(argv[0]);
        return 1;
    }
    
    const char *command = argv[optind];
    
    /* 初始化日志系统 */
    distfs_log_init(NULL, verbose ? DISTFS_LOG_DEBUG : DISTFS_LOG_INFO);
    
    if (verbose) {
        printf("DistFS Client v1.0.0\n");
        printf("Connecting to %s:%d...\n", server, port);
    }
    
    /* 创建客户端上下文 */
    g_client = distfs_client_create(server, port);
    if (!g_client) {
        fprintf(stderr, "Failed to connect to DistFS server %s:%d\n", server, port);
        distfs_log_cleanup();
        return 1;
    }
    
    if (verbose) {
        printf("Connected successfully\n");
    }
    
    /* 查找并执行命令 */
    int result = 1;
    for (int i = 0; commands[i].name; i++) {
        if (strcmp(command, commands[i].name) == 0) {
            result = commands[i].handler(argc - optind - 1, argv + optind + 1);
            break;
        }
    }
    
    if (result == 1 && commands[0].name) {
        // 命令未找到
        bool found = false;
        for (int i = 0; commands[i].name; i++) {
            if (strcmp(command, commands[i].name) == 0) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            fprintf(stderr, "Unknown command: %s\n", command);
            printf("\nAvailable commands:\n");
            for (int i = 0; commands[i].name; i++) {
                printf("  %-8s %s\n", commands[i].name, commands[i].description);
            }
        }
    }
    
    /* 清理资源 */
    distfs_client_destroy(g_client);
    distfs_log_cleanup();
    
    return result;
}
