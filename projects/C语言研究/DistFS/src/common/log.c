/*
 * DistFS 日志系统实现
 * 提供结构化日志记录功能
 */

#include "distfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdbool.h>

/* 日志级别名称 */
static const char* log_level_names[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

/* 日志级别颜色 */
static const char* log_level_colors[] = {
    "\033[94m", // TRACE - 蓝色
    "\033[36m", // DEBUG - 青色
    "\033[32m", // INFO - 绿色
    "\033[33m", // WARN - 黄色
    "\033[31m", // ERROR - 红色
    "\033[35m"  // FATAL - 紫色
};

#define COLOR_RESET "\033[0m"

/* 日志管理器结构 */
struct distfs_logger {
    FILE *file;
    int level;
    bool use_color;
    bool use_timestamp;
    char log_file[DISTFS_MAX_PATH_LEN];
    pthread_mutex_t mutex;
    uint64_t max_file_size;
    int max_backup_files;
    uint64_t current_size;
};

/* 全局日志管理器 */
static distfs_logger_t *g_logger = NULL;

/* 获取当前时间戳字符串 */
static void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/* 获取线程ID */
static unsigned long get_thread_id(void) {
    return (unsigned long)pthread_self();
}

/* 日志文件轮转 */
static int rotate_log_file(distfs_logger_t *logger) {
    if (!logger || !logger->file) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    fclose(logger->file);
    logger->file = NULL;
    
    // 备份现有日志文件
    for (int i = logger->max_backup_files - 1; i > 0; i--) {
        char old_name[DISTFS_MAX_PATH_LEN];
        char new_name[DISTFS_MAX_PATH_LEN];
        
        if (i == 1) {
            snprintf(old_name, sizeof(old_name), "%s", logger->log_file);
        } else {
            snprintf(old_name, sizeof(old_name), "%s.%d", logger->log_file, i - 1);
        }
        
        snprintf(new_name, sizeof(new_name), "%s.%d", logger->log_file, i);
        
        if (access(old_name, F_OK) == 0) {
            rename(old_name, new_name);
        }
    }
    
    // 重新打开日志文件
    logger->file = fopen(logger->log_file, "w");
    if (!logger->file) {
        return DISTFS_ERROR_FILE_OPEN_FAILED;
    }
    
    logger->current_size = 0;
    return DISTFS_SUCCESS;
}

/* 初始化日志系统 */
int distfs_log_init(const char *log_file, int level) {
    if (g_logger) {
        return DISTFS_ERROR_ALREADY_INITIALIZED;
    }
    
    g_logger = malloc(sizeof(distfs_logger_t));
    if (!g_logger) {
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    memset(g_logger, 0, sizeof(distfs_logger_t));
    
    g_logger->level = level;
    g_logger->use_color = isatty(STDERR_FILENO);
    g_logger->use_timestamp = true;
    g_logger->max_file_size = 100 * 1024 * 1024; // 100MB
    g_logger->max_backup_files = 5;
    g_logger->current_size = 0;
    
    if (pthread_mutex_init(&g_logger->mutex, NULL) != 0) {
        free(g_logger);
        g_logger = NULL;
        return DISTFS_ERROR_SYSTEM_ERROR;
    }
    
    if (log_file && strlen(log_file) > 0) {
        strncpy(g_logger->log_file, log_file, sizeof(g_logger->log_file) - 1);
        g_logger->file = fopen(log_file, "a");
        if (!g_logger->file) {
            pthread_mutex_destroy(&g_logger->mutex);
            free(g_logger);
            g_logger = NULL;
            return DISTFS_ERROR_FILE_OPEN_FAILED;
        }
        
        // 获取当前文件大小
        struct stat st;
        if (stat(log_file, &st) == 0) {
            g_logger->current_size = st.st_size;
        }
        
        g_logger->use_color = false; // 文件输出不使用颜色
    } else {
        g_logger->file = stderr;
    }
    
    return DISTFS_SUCCESS;
}

/* 清理日志系统 */
void distfs_log_cleanup(void) {
    if (!g_logger) return;
    
    pthread_mutex_lock(&g_logger->mutex);
    
    if (g_logger->file && g_logger->file != stderr) {
        fclose(g_logger->file);
    }
    
    pthread_mutex_unlock(&g_logger->mutex);
    pthread_mutex_destroy(&g_logger->mutex);
    
    free(g_logger);
    g_logger = NULL;
}

/* 设置日志级别 */
void distfs_log_set_level(int level) {
    if (g_logger && level >= DISTFS_LOG_TRACE && level <= DISTFS_LOG_FATAL) {
        g_logger->level = level;
    }
}

/* 获取日志级别 */
int distfs_log_get_level(void) {
    return g_logger ? g_logger->level : DISTFS_LOG_INFO;
}

/* 设置日志选项 */
void distfs_log_set_options(bool use_color, bool use_timestamp) {
    if (g_logger) {
        g_logger->use_color = use_color && isatty(fileno(g_logger->file));
        g_logger->use_timestamp = use_timestamp;
    }
}

/* 设置日志轮转参数 */
void distfs_log_set_rotation(uint64_t max_file_size, int max_backup_files) {
    if (g_logger) {
        g_logger->max_file_size = max_file_size;
        g_logger->max_backup_files = max_backup_files;
    }
}

/* 写入日志 */
void distfs_log_write(int level, const char *file, int line, const char *func, 
                      const char *format, ...) {
    if (!g_logger || level < g_logger->level || level > DISTFS_LOG_FATAL) {
        return;
    }
    
    pthread_mutex_lock(&g_logger->mutex);
    
    // 检查是否需要轮转日志文件
    if (g_logger->file != stderr && 
        g_logger->max_file_size > 0 && 
        g_logger->current_size > g_logger->max_file_size) {
        rotate_log_file(g_logger);
    }
    
    char timestamp[64] = "";
    if (g_logger->use_timestamp) {
        get_timestamp(timestamp, sizeof(timestamp));
    }
    
    const char *color_start = "";
    const char *color_end = "";
    if (g_logger->use_color) {
        color_start = log_level_colors[level];
        color_end = COLOR_RESET;
    }
    
    // 写入日志头部
    int header_len = 0;
    if (g_logger->use_timestamp) {
        header_len = fprintf(g_logger->file, "%s[%s] %s%-5s%s [%lu] %s:%d %s(): ",
                           timestamp[0] ? timestamp : "",
                           timestamp[0] ? " " : "",
                           color_start,
                           log_level_names[level],
                           color_end,
                           get_thread_id(),
                           file ? strrchr(file, '/') ? strrchr(file, '/') + 1 : file : "unknown",
                           line,
                           func ? func : "unknown");
    } else {
        header_len = fprintf(g_logger->file, "%s%-5s%s [%lu] %s:%d %s(): ",
                           color_start,
                           log_level_names[level],
                           color_end,
                           get_thread_id(),
                           file ? strrchr(file, '/') ? strrchr(file, '/') + 1 : file : "unknown",
                           line,
                           func ? func : "unknown");
    }
    
    // 写入日志内容
    va_list args;
    va_start(args, format);
    int content_len = vfprintf(g_logger->file, format, args);
    va_end(args);
    
    fprintf(g_logger->file, "\n");
    fflush(g_logger->file);
    
    // 更新文件大小
    if (g_logger->file != stderr) {
        g_logger->current_size += header_len + content_len + 1;
    }
    
    pthread_mutex_unlock(&g_logger->mutex);
}

/* 写入十六进制数据 */
void distfs_log_hex(int level, const char *file, int line, const char *func,
                    const char *prefix, const void *data, size_t size) {
    if (!g_logger || level < g_logger->level || !data || size == 0) {
        return;
    }
    
    const unsigned char *bytes = (const unsigned char *)data;
    char hex_buffer[1024];
    char ascii_buffer[64];
    
    distfs_log_write(level, file, line, func, "%s (%zu bytes):", 
                     prefix ? prefix : "HEX", size);
    
    for (size_t i = 0; i < size; i += 16) {
        // 构建十六进制字符串
        int hex_pos = 0;
        int ascii_pos = 0;
        
        hex_pos += snprintf(hex_buffer + hex_pos, sizeof(hex_buffer) - hex_pos,
                           "%08zx: ", i);
        
        for (size_t j = 0; j < 16 && i + j < size; j++) {
            hex_pos += snprintf(hex_buffer + hex_pos, sizeof(hex_buffer) - hex_pos,
                               "%02x ", bytes[i + j]);
            
            char c = bytes[i + j];
            ascii_buffer[ascii_pos++] = (c >= 32 && c <= 126) ? c : '.';
        }
        
        // 填充空格
        for (size_t j = size - i; j < 16; j++) {
            hex_pos += snprintf(hex_buffer + hex_pos, sizeof(hex_buffer) - hex_pos, "   ");
        }
        
        ascii_buffer[ascii_pos] = '\0';
        
        distfs_log_write(level, file, line, func, "%s |%s|", hex_buffer, ascii_buffer);
    }
}

/* 检查日志级别是否启用 */
bool distfs_log_is_enabled(int level) {
    return g_logger && level >= g_logger->level;
}
