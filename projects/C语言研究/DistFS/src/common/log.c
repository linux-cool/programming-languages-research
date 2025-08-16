/*
 * DistFS 日志系统实现
 * 提供多级别、多输出的日志功能
 */

#include "distfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>

/* 日志级别定义 */
typedef enum {
    LOG_LEVEL_TRACE = 0,
    LOG_LEVEL_DEBUG = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_WARN = 3,
    LOG_LEVEL_ERROR = 4,
    LOG_LEVEL_FATAL = 5
} log_level_t;

/* 日志级别名称 */
static const char* log_level_names[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

/* 日志级别颜色 */
static const char* log_level_colors[] = {
    "\033[36m",  // TRACE - 青色
    "\033[34m",  // DEBUG - 蓝色
    "\033[32m",  // INFO - 绿色
    "\033[33m",  // WARN - 黄色
    "\033[31m",  // ERROR - 红色
    "\033[35m"   // FATAL - 紫色
};

#define COLOR_RESET "\033[0m"

/* 日志配置结构 */
typedef struct {
    FILE *file;
    char filename[DISTFS_MAX_PATH_LEN];
    log_level_t level;
    bool console_output;
    bool color_output;
    bool timestamp;
    bool thread_id;
    bool file_line;
    pthread_mutex_t mutex;
    bool initialized;
    uint64_t max_file_size;
    int max_backup_files;
    uint64_t current_file_size;
} log_config_t;

static log_config_t g_log_config = {
    .file = NULL,
    .filename = "",
    .level = LOG_LEVEL_INFO,
    .console_output = true,
    .color_output = true,
    .timestamp = true,
    .thread_id = false,
    .file_line = false,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .initialized = false,
    .max_file_size = 100 * 1024 * 1024,  // 100MB
    .max_backup_files = 5,
    .current_file_size = 0
};

/* 获取当前时间字符串 */
static void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/* 日志文件轮转 */
static int rotate_log_file(void) {
    if (!g_log_config.file || g_log_config.current_file_size < g_log_config.max_file_size) {
        return DISTFS_SUCCESS;
    }
    
    fclose(g_log_config.file);
    g_log_config.file = NULL;
    
    // 轮转备份文件
    for (int i = g_log_config.max_backup_files - 1; i > 0; i--) {
        char old_name[DISTFS_MAX_PATH_LEN];
        char new_name[DISTFS_MAX_PATH_LEN];
        
        if (i == 1) {
            snprintf(old_name, sizeof(old_name), "%s", g_log_config.filename);
        } else {
            snprintf(old_name, sizeof(old_name), "%s.%d", g_log_config.filename, i - 1);
        }
        
        snprintf(new_name, sizeof(new_name), "%s.%d", g_log_config.filename, i);
        
        rename(old_name, new_name);
    }
    
    // 重新打开日志文件
    g_log_config.file = fopen(g_log_config.filename, "a");
    if (!g_log_config.file) {
        return DISTFS_ERROR_PERMISSION_DENIED;
    }
    
    g_log_config.current_file_size = 0;
    return DISTFS_SUCCESS;
}

/* 初始化日志系统 */
int distfs_log_init(const char *filename, int level) {
    pthread_mutex_lock(&g_log_config.mutex);
    
    if (g_log_config.initialized) {
        pthread_mutex_unlock(&g_log_config.mutex);
        return DISTFS_SUCCESS;
    }
    
    g_log_config.level = (level >= 0 && level <= 5) ? level : LOG_LEVEL_INFO;
    
    if (filename && strlen(filename) > 0) {
        strncpy(g_log_config.filename, filename, sizeof(g_log_config.filename) - 1);
        g_log_config.filename[sizeof(g_log_config.filename) - 1] = '\0';
        
        // 创建日志目录
        char *dir = strdup(filename);
        if (dir) {
            char *last_slash = strrchr(dir, '/');
            if (last_slash) {
                *last_slash = '\0';
                distfs_mkdir_recursive(dir, 0755);
            }
            free(dir);
        }
        
        g_log_config.file = fopen(filename, "a");
        if (!g_log_config.file) {
            pthread_mutex_unlock(&g_log_config.mutex);
            return DISTFS_ERROR_PERMISSION_DENIED;
        }
        
        // 获取当前文件大小
        struct stat st;
        if (stat(filename, &st) == 0) {
            g_log_config.current_file_size = st.st_size;
        }
    }
    
    // 检查是否支持颜色输出
    g_log_config.color_output = isatty(STDERR_FILENO);
    
    g_log_config.initialized = true;
    
    pthread_mutex_unlock(&g_log_config.mutex);
    return DISTFS_SUCCESS;
}

/* 设置日志级别 */
void distfs_log_set_level(int level) {
    pthread_mutex_lock(&g_log_config.mutex);
    g_log_config.level = (level >= 0 && level <= 5) ? level : LOG_LEVEL_INFO;
    pthread_mutex_unlock(&g_log_config.mutex);
}

/* 设置控制台输出 */
void distfs_log_set_console(bool enable) {
    pthread_mutex_lock(&g_log_config.mutex);
    g_log_config.console_output = enable;
    pthread_mutex_unlock(&g_log_config.mutex);
}

/* 设置颜色输出 */
void distfs_log_set_color(bool enable) {
    pthread_mutex_lock(&g_log_config.mutex);
    g_log_config.color_output = enable;
    pthread_mutex_unlock(&g_log_config.mutex);
}

/* 设置时间戳 */
void distfs_log_set_timestamp(bool enable) {
    pthread_mutex_lock(&g_log_config.mutex);
    g_log_config.timestamp = enable;
    pthread_mutex_unlock(&g_log_config.mutex);
}

/* 设置线程ID */
void distfs_log_set_thread_id(bool enable) {
    pthread_mutex_lock(&g_log_config.mutex);
    g_log_config.thread_id = enable;
    pthread_mutex_unlock(&g_log_config.mutex);
}

/* 设置文件行号 */
void distfs_log_set_file_line(bool enable) {
    pthread_mutex_lock(&g_log_config.mutex);
    g_log_config.file_line = enable;
    pthread_mutex_unlock(&g_log_config.mutex);
}

/* 核心日志函数 */
void distfs_log_write(int level, const char *file, int line, const char *func, 
                      const char *format, ...) {
    if (!g_log_config.initialized || level < g_log_config.level) {
        return;
    }
    
    pthread_mutex_lock(&g_log_config.mutex);
    
    char timestamp[64] = "";
    if (g_log_config.timestamp) {
        get_timestamp(timestamp, sizeof(timestamp));
    }
    
    char thread_info[32] = "";
    if (g_log_config.thread_id) {
        snprintf(thread_info, sizeof(thread_info), "[%lu] ", pthread_self());
    }
    
    char file_info[256] = "";
    if (g_log_config.file_line && file) {
        const char *basename = strrchr(file, '/');
        basename = basename ? basename + 1 : file;
        snprintf(file_info, sizeof(file_info), "(%s:%d) ", basename, line);
    }
    
    char func_info[128] = "";
    if (func) {
        snprintf(func_info, sizeof(func_info), "%s() ", func);
    }
    
    // 格式化消息
    va_list args;
    va_start(args, format);
    char message[4096];
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    // 输出到控制台
    if (g_log_config.console_output) {
        const char *color = g_log_config.color_output ? log_level_colors[level] : "";
        const char *reset = g_log_config.color_output ? COLOR_RESET : "";
        
        fprintf(stderr, "%s[%s]%s %s%s%s%s%s\n",
                color, log_level_names[level], reset,
                timestamp, thread_info, file_info, func_info, message);
    }
    
    // 输出到文件
    if (g_log_config.file) {
        int written = fprintf(g_log_config.file, "[%s] %s%s%s%s%s\n",
                             log_level_names[level], timestamp, thread_info, 
                             file_info, func_info, message);
        
        if (written > 0) {
            g_log_config.current_file_size += written;
            fflush(g_log_config.file);
            
            // 检查是否需要轮转
            rotate_log_file();
        }
    }
    
    pthread_mutex_unlock(&g_log_config.mutex);
}

/* 清理日志系统 */
void distfs_log_cleanup(void) {
    pthread_mutex_lock(&g_log_config.mutex);
    
    if (g_log_config.file) {
        fclose(g_log_config.file);
        g_log_config.file = NULL;
    }
    
    g_log_config.initialized = false;
    
    pthread_mutex_unlock(&g_log_config.mutex);
}

/* 刷新日志缓冲区 */
void distfs_log_flush(void) {
    pthread_mutex_lock(&g_log_config.mutex);
    
    if (g_log_config.file) {
        fflush(g_log_config.file);
    }
    
    fflush(stderr);
    
    pthread_mutex_unlock(&g_log_config.mutex);
}

/* 日志宏定义 */
#define DISTFS_LOG_TRACE(format, ...) \
    distfs_log_write(LOG_LEVEL_TRACE, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

#define DISTFS_LOG_DEBUG(format, ...) \
    distfs_log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

#define DISTFS_LOG_INFO(format, ...) \
    distfs_log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

#define DISTFS_LOG_WARN(format, ...) \
    distfs_log_write(LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

#define DISTFS_LOG_ERROR(format, ...) \
    distfs_log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

#define DISTFS_LOG_FATAL(format, ...) \
    distfs_log_write(LOG_LEVEL_FATAL, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

/* 获取日志统计信息 */
int distfs_log_get_stats(uint64_t *file_size, int *level, bool *console_enabled) {
    pthread_mutex_lock(&g_log_config.mutex);
    
    if (file_size) {
        *file_size = g_log_config.current_file_size;
    }
    
    if (level) {
        *level = g_log_config.level;
    }
    
    if (console_enabled) {
        *console_enabled = g_log_config.console_output;
    }
    
    pthread_mutex_unlock(&g_log_config.mutex);
    return DISTFS_SUCCESS;
}
