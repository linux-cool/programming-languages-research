/*
 * DistFS 客户端工具
 * 提供命令行接口操作分布式文件系统
 */

#include "distfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>

/* 命令类型定义 */
typedef enum {
    CMD_HELP = 0,
    CMD_CREATE,
    CMD_OPEN,
    CMD_READ,
    CMD_WRITE,
    CMD_DELETE,
    CMD_MKDIR,
    CMD_RMDIR,
    CMD_LIST,
    CMD_STAT,
    CMD_STATUS,
    CMD_UNKNOWN
} command_type_t;

/* 全局配置 */
static struct {
    char config_file[1024];
    char server_addr[256];
    int server_port;
    bool verbose;
    bool debug;
} g_config = {
    .config_file = "",
    .server_addr = "127.0.0.1",
    .server_port = 9527,
    .verbose = false,
    .debug = false
};

/* 显示帮助信息 */
void show_help(const char *program_name) {
    printf("DistFS 客户端工具 v1.0.0\n");
    printf("用法: %s [选项] <命令> [参数...]\n\n", program_name);
    
    printf("选项:\n");
    printf("  -c, --config FILE     配置文件路径\n");
    printf("  -s, --server ADDR     服务器地址 (默认: 127.0.0.1)\n");
    printf("  -p, --port PORT       服务器端口 (默认: 9527)\n");
    printf("  -v, --verbose         详细输出\n");
    printf("  -d, --debug           调试模式\n");
    printf("  -h, --help            显示此帮助信息\n\n");
    
    printf("命令:\n");
    printf("  create <path> [mode]  创建文件\n");
    printf("  delete <path>         删除文件\n");
    printf("  mkdir <path> [mode]   创建目录\n");
    printf("  rmdir <path>          删除目录\n");
    printf("  list <path>           列出目录内容\n");
    printf("  stat <path>           显示文件/目录信息\n");
    printf("  read <path>           读取文件内容\n");
    printf("  write <path> <data>   写入文件内容\n");
    printf("  status                显示集群状态\n\n");
    
    printf("示例:\n");
    printf("  %s create /test/file.txt 644\n", program_name);
    printf("  %s mkdir /test/dir 755\n", program_name);
    printf("  %s list /test\n", program_name);
    printf("  %s stat /test/file.txt\n", program_name);
    printf("  %s write /test/file.txt \"Hello, DistFS!\"\n", program_name);
    printf("  %s read /test/file.txt\n", program_name);
    printf("  %s status\n", program_name);
}

/* 解析命令类型 */
command_type_t parse_command(const char *cmd) {
    if (!cmd) return CMD_UNKNOWN;
    
    if (strcmp(cmd, "help") == 0) return CMD_HELP;
    if (strcmp(cmd, "create") == 0) return CMD_CREATE;
    if (strcmp(cmd, "open") == 0) return CMD_OPEN;
    if (strcmp(cmd, "read") == 0) return CMD_READ;
    if (strcmp(cmd, "write") == 0) return CMD_WRITE;
    if (strcmp(cmd, "delete") == 0 || strcmp(cmd, "rm") == 0) return CMD_DELETE;
    if (strcmp(cmd, "mkdir") == 0) return CMD_MKDIR;
    if (strcmp(cmd, "rmdir") == 0) return CMD_RMDIR;
    if (strcmp(cmd, "list") == 0 || strcmp(cmd, "ls") == 0) return CMD_LIST;
    if (strcmp(cmd, "stat") == 0) return CMD_STAT;
    if (strcmp(cmd, "status") == 0) return CMD_STATUS;
    
    return CMD_UNKNOWN;
}

/* 执行创建文件命令 */
int cmd_create(distfs_client_t *client, int argc, char *argv[]) {
    if (argc < 1) {
        fprintf(stderr, "错误: create命令需要文件路径参数\n");
        return 1;
    }
    
    const char *path = argv[0];
    mode_t mode = 0644;  // 默认权限
    
    if (argc >= 2) {
        mode = strtol(argv[1], NULL, 8);  // 八进制解析
    }
    
    if (g_config.verbose) {
        printf("创建文件: %s (权限: %o)\n", path, mode);
    }
    
    int result = distfs_create(client, path, mode);
    if (result == DISTFS_SUCCESS) {
        printf("文件创建成功: %s\n", path);
        return 0;
    } else {
        fprintf(stderr, "文件创建失败: %s (%s)\n", path, distfs_strerror(result));
        return 1;
    }
}

/* 执行删除文件命令 */
int cmd_delete(distfs_client_t *client, int argc, char *argv[]) {
    if (argc < 1) {
        fprintf(stderr, "错误: delete命令需要文件路径参数\n");
        return 1;
    }
    
    const char *path = argv[0];
    
    if (g_config.verbose) {
        printf("删除文件: %s\n", path);
    }
    
    int result = distfs_unlink(client, path);
    if (result == DISTFS_SUCCESS) {
        printf("文件删除成功: %s\n", path);
        return 0;
    } else {
        fprintf(stderr, "文件删除失败: %s (%s)\n", path, distfs_strerror(result));
        return 1;
    }
}

/* 执行创建目录命令 */
int cmd_mkdir(distfs_client_t *client, int argc, char *argv[]) {
    if (argc < 1) {
        fprintf(stderr, "错误: mkdir命令需要目录路径参数\n");
        return 1;
    }
    
    const char *path = argv[0];
    mode_t mode = 0755;  // 默认权限
    
    if (argc >= 2) {
        mode = strtol(argv[1], NULL, 8);  // 八进制解析
    }
    
    if (g_config.verbose) {
        printf("创建目录: %s (权限: %o)\n", path, mode);
    }
    
    int result = distfs_mkdir(client, path, mode);
    if (result == DISTFS_SUCCESS) {
        printf("目录创建成功: %s\n", path);
        return 0;
    } else {
        fprintf(stderr, "目录创建失败: %s (%s)\n", path, distfs_strerror(result));
        return 1;
    }
}

/* 执行删除目录命令 */
int cmd_rmdir(distfs_client_t *client, int argc, char *argv[]) {
    if (argc < 1) {
        fprintf(stderr, "错误: rmdir命令需要目录路径参数\n");
        return 1;
    }
    
    const char *path = argv[0];
    
    if (g_config.verbose) {
        printf("删除目录: %s\n", path);
    }
    
    int result = distfs_rmdir(client, path);
    if (result == DISTFS_SUCCESS) {
        printf("目录删除成功: %s\n", path);
        return 0;
    } else {
        fprintf(stderr, "目录删除失败: %s (%s)\n", path, distfs_strerror(result));
        return 1;
    }
}

/* 执行列出目录命令 */
int cmd_list(distfs_client_t *client, int argc, char *argv[]) {
    if (argc < 1) {
        fprintf(stderr, "错误: list命令需要目录路径参数\n");
        return 1;
    }
    
    const char *path = argv[0];
    
    if (g_config.verbose) {
        printf("列出目录: %s\n", path);
    }
    
    distfs_dirent_t *entries = NULL;
    int count = 0;
    
    int result = distfs_readdir(client, path, &entries, &count);
    if (result == DISTFS_SUCCESS) {
        printf("目录内容 (%s):\n", path);
        for (int i = 0; i < count; i++) {
            const char *type_str = "";
            switch (entries[i].type) {
                case DISTFS_FILE_TYPE_REGULAR: type_str = "文件"; break;
                case DISTFS_FILE_TYPE_DIRECTORY: type_str = "目录"; break;
                case DISTFS_FILE_TYPE_SYMLINK: type_str = "链接"; break;
            }
            printf("  %s\t%s\t(inode: %lu)\n", type_str, entries[i].name, entries[i].inode);
        }
        
        if (entries) {
            free(entries);
        }
        
        return 0;
    } else {
        fprintf(stderr, "列出目录失败: %s (%s)\n", path, distfs_strerror(result));
        return 1;
    }
}

/* 执行文件状态命令 */
int cmd_stat(distfs_client_t *client, int argc, char *argv[]) {
    if (argc < 1) {
        fprintf(stderr, "错误: stat命令需要文件路径参数\n");
        return 1;
    }
    
    const char *path = argv[0];
    
    if (g_config.verbose) {
        printf("获取文件状态: %s\n", path);
    }
    
    struct stat st;
    int result = distfs_stat(client, path, &st);
    if (result == DISTFS_SUCCESS) {
        printf("文件信息 (%s):\n", path);
        printf("  类型: %s\n", S_ISDIR(st.st_mode) ? "目录" : 
                              S_ISREG(st.st_mode) ? "普通文件" : "其他");
        printf("  大小: %ld 字节\n", st.st_size);
        printf("  权限: %o\n", st.st_mode & 0777);
        printf("  所有者: %d:%d\n", st.st_uid, st.st_gid);
        printf("  链接数: %ld\n", st.st_nlink);
        printf("  访问时间: %ld\n", st.st_atime);
        printf("  修改时间: %ld\n", st.st_mtime);
        printf("  状态时间: %ld\n", st.st_ctime);
        return 0;
    } else {
        fprintf(stderr, "获取文件状态失败: %s (%s)\n", path, distfs_strerror(result));
        return 1;
    }
}

/* 执行集群状态命令 */
int cmd_status(distfs_client_t *client, int argc, char *argv[]) {
    (void)argc;  // 未使用的参数
    (void)argv;
    
    if (g_config.verbose) {
        printf("获取集群状态\n");
    }
    
    distfs_cluster_status_t status;
    int result = distfs_get_cluster_status(client, &status);
    if (result == DISTFS_SUCCESS) {
        printf("集群状态:\n");
        printf("  总节点数: %u\n", status.total_nodes);
        printf("  在线节点数: %u\n", status.online_nodes);
        printf("  元数据节点数: %u\n", status.metadata_nodes);
        printf("  存储节点数: %u\n", status.storage_nodes);
        printf("  总容量: %.2f GB\n", (double)status.total_capacity / (1024*1024*1024));
        printf("  已用容量: %.2f GB\n", (double)status.used_capacity / (1024*1024*1024));
        printf("  使用率: %.1f%%\n", (double)status.used_capacity / status.total_capacity * 100);
        printf("  总文件数: %lu\n", status.total_files);
        printf("  负载因子: %.2f\n", status.load_factor);
        return 0;
    } else {
        fprintf(stderr, "获取集群状态失败: %s\n", distfs_strerror(result));
        return 1;
    }
}

/* 主函数 */
int main(int argc, char *argv[]) {
    // 解析命令行选项
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"server", required_argument, 0, 's'},
        {"port", required_argument, 0, 'p'},
        {"verbose", no_argument, 0, 'v'},
        {"debug", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "c:s:p:vdh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                strncpy(g_config.config_file, optarg, sizeof(g_config.config_file) - 1);
                break;
            case 's':
                strncpy(g_config.server_addr, optarg, sizeof(g_config.server_addr) - 1);
                break;
            case 'p':
                g_config.server_port = atoi(optarg);
                break;
            case 'v':
                g_config.verbose = true;
                break;
            case 'd':
                g_config.debug = true;
                break;
            case 'h':
                show_help(argv[0]);
                return 0;
            default:
                show_help(argv[0]);
                return 1;
        }
    }
    
    // 检查是否有命令
    if (optind >= argc) {
        fprintf(stderr, "错误: 缺少命令参数\n");
        show_help(argv[0]);
        return 1;
    }
    
    // 解析命令
    command_type_t cmd_type = parse_command(argv[optind]);
    if (cmd_type == CMD_HELP) {
        show_help(argv[0]);
        return 0;
    }
    
    if (cmd_type == CMD_UNKNOWN) {
        fprintf(stderr, "错误: 未知命令 '%s'\n", argv[optind]);
        show_help(argv[0]);
        return 1;
    }
    
    // 初始化客户端
    distfs_client_t *client = distfs_init(g_config.config_file[0] ? g_config.config_file : NULL);
    if (!client) {
        fprintf(stderr, "错误: 无法初始化DistFS客户端\n");
        return 1;
    }
    
    // 执行命令
    int result = 0;
    int cmd_argc = argc - optind - 1;
    char **cmd_argv = argv + optind + 1;
    
    switch (cmd_type) {
        case CMD_CREATE:
            result = cmd_create(client, cmd_argc, cmd_argv);
            break;
        case CMD_DELETE:
            result = cmd_delete(client, cmd_argc, cmd_argv);
            break;
        case CMD_MKDIR:
            result = cmd_mkdir(client, cmd_argc, cmd_argv);
            break;
        case CMD_RMDIR:
            result = cmd_rmdir(client, cmd_argc, cmd_argv);
            break;
        case CMD_LIST:
            result = cmd_list(client, cmd_argc, cmd_argv);
            break;
        case CMD_STAT:
            result = cmd_stat(client, cmd_argc, cmd_argv);
            break;
        case CMD_STATUS:
            result = cmd_status(client, cmd_argc, cmd_argv);
            break;
        default:
            fprintf(stderr, "错误: 命令未实现\n");
            result = 1;
            break;
    }
    
    // 清理客户端
    distfs_cleanup(client);
    
    return result;
}
