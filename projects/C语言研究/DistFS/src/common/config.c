/*
 * DistFS 配置管理模块实现
 * 提供配置文件解析和参数管理功能
 */

#include "distfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* 配置项结构 */
typedef struct config_item {
    char key[256];
    char value[1024];
    struct config_item *next;
} config_item_t;

/* 配置管理器结构 */
typedef struct {
    config_item_t *items;
    char config_file[DISTFS_MAX_PATH_LEN];
    bool loaded;
} config_manager_t;

static config_manager_t g_config_manager = {0};

/* 去除字符串首尾空白字符 */
static char* trim_whitespace(char *str) {
    if (!str) return NULL;
    
    // 去除前导空白
    while (isspace(*str)) str++;
    
    if (*str == 0) return str;
    
    // 去除尾随空白
    char *end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) end--;
    
    *(end + 1) = '\0';
    return str;
}

/* 解析配置行 */
static int parse_config_line(const char *line, char *key, char *value) {
    if (!line || !key || !value) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    // 跳过注释和空行
    const char *trimmed = line;
    while (isspace(*trimmed)) trimmed++;
    
    if (*trimmed == '#' || *trimmed == '\0') {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    // 查找等号
    const char *eq = strchr(trimmed, '=');
    if (!eq) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    // 提取key
    size_t key_len = eq - trimmed;
    if (key_len >= 256) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    strncpy(key, trimmed, key_len);
    key[key_len] = '\0';
    trim_whitespace(key);
    
    // 提取value
    const char *val_start = eq + 1;
    strncpy(value, val_start, 1023);
    value[1023] = '\0';
    trim_whitespace(value);
    
    return DISTFS_SUCCESS;
}

/* 添加配置项 */
static int add_config_item(const char *key, const char *value) {
    if (!key || !value) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    config_item_t *item = malloc(sizeof(config_item_t));
    if (!item) {
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    strncpy(item->key, key, sizeof(item->key) - 1);
    item->key[sizeof(item->key) - 1] = '\0';
    
    strncpy(item->value, value, sizeof(item->value) - 1);
    item->value[sizeof(item->value) - 1] = '\0';
    
    item->next = g_config_manager.items;
    g_config_manager.items = item;
    
    return DISTFS_SUCCESS;
}

/* 查找配置项 */
static const char* find_config_item(const char *key) {
    if (!key) return NULL;
    
    config_item_t *item = g_config_manager.items;
    while (item) {
        if (strcmp(item->key, key) == 0) {
            return item->value;
        }
        item = item->next;
    }
    
    return NULL;
}

/* 加载配置文件 */
int distfs_config_load(const char *config_file) {
    if (!config_file) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    FILE *fp = fopen(config_file, "r");
    if (!fp) {
        return DISTFS_ERROR_FILE_NOT_FOUND;
    }
    
    // 清理现有配置
    distfs_config_cleanup();
    
    strncpy(g_config_manager.config_file, config_file, 
            sizeof(g_config_manager.config_file) - 1);
    g_config_manager.config_file[sizeof(g_config_manager.config_file) - 1] = '\0';
    
    char line[2048];
    int line_number = 0;
    int result = DISTFS_SUCCESS;
    
    while (fgets(line, sizeof(line), fp)) {
        line_number++;
        
        char key[256], value[1024];
        if (parse_config_line(line, key, value) == DISTFS_SUCCESS) {
            if (add_config_item(key, value) != DISTFS_SUCCESS) {
                result = DISTFS_ERROR_NO_MEMORY;
                break;
            }
        }
    }
    
    fclose(fp);
    
    if (result == DISTFS_SUCCESS) {
        g_config_manager.loaded = true;
    }
    
    return result;
}

/* 获取字符串配置 */
const char* distfs_config_get_string(const char *key, const char *default_value) {
    if (!key) return default_value;
    
    const char *value = find_config_item(key);
    return value ? value : default_value;
}

/* 获取整数配置 */
int distfs_config_get_int(const char *key, int default_value) {
    const char *value = find_config_item(key);
    if (!value) return default_value;
    
    char *endptr;
    long result = strtol(value, &endptr, 10);
    
    if (*endptr != '\0' || result < INT_MIN || result > INT_MAX) {
        return default_value;
    }
    
    return (int)result;
}

/* 获取布尔配置 */
bool distfs_config_get_bool(const char *key, bool default_value) {
    const char *value = find_config_item(key);
    if (!value) return default_value;
    
    if (strcasecmp(value, "true") == 0 || 
        strcasecmp(value, "yes") == 0 || 
        strcasecmp(value, "1") == 0) {
        return true;
    }
    
    if (strcasecmp(value, "false") == 0 || 
        strcasecmp(value, "no") == 0 || 
        strcasecmp(value, "0") == 0) {
        return false;
    }
    
    return default_value;
}

/* 获取浮点数配置 */
double distfs_config_get_double(const char *key, double default_value) {
    const char *value = find_config_item(key);
    if (!value) return default_value;
    
    char *endptr;
    double result = strtod(value, &endptr);
    
    if (*endptr != '\0') {
        return default_value;
    }
    
    return result;
}

/* 设置配置项 */
int distfs_config_set(const char *key, const char *value) {
    if (!key || !value) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    // 查找现有项
    config_item_t *item = g_config_manager.items;
    while (item) {
        if (strcmp(item->key, key) == 0) {
            // 更新现有项
            strncpy(item->value, value, sizeof(item->value) - 1);
            item->value[sizeof(item->value) - 1] = '\0';
            return DISTFS_SUCCESS;
        }
        item = item->next;
    }
    
    // 添加新项
    return add_config_item(key, value);
}

/* 保存配置到文件 */
int distfs_config_save(const char *config_file) {
    const char *file = config_file ? config_file : g_config_manager.config_file;
    if (!file || strlen(file) == 0) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    FILE *fp = fopen(file, "w");
    if (!fp) {
        return DISTFS_ERROR_PERMISSION_DENIED;
    }
    
    fprintf(fp, "# DistFS Configuration File\n");
    fprintf(fp, "# Generated automatically\n\n");
    
    config_item_t *item = g_config_manager.items;
    while (item) {
        fprintf(fp, "%s = %s\n", item->key, item->value);
        item = item->next;
    }
    
    fclose(fp);
    return DISTFS_SUCCESS;
}

/* 列出所有配置项 */
int distfs_config_list(void (*callback)(const char *key, const char *value, void *user_data), 
                       void *user_data) {
    if (!callback) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    config_item_t *item = g_config_manager.items;
    while (item) {
        callback(item->key, item->value, user_data);
        item = item->next;
    }
    
    return DISTFS_SUCCESS;
}

/* 检查配置是否已加载 */
bool distfs_config_is_loaded(void) {
    return g_config_manager.loaded;
}

/* 获取配置文件路径 */
const char* distfs_config_get_file(void) {
    return g_config_manager.config_file;
}

/* 清理配置 */
void distfs_config_cleanup(void) {
    config_item_t *item = g_config_manager.items;
    while (item) {
        config_item_t *next = item->next;
        free(item);
        item = next;
    }
    
    g_config_manager.items = NULL;
    g_config_manager.loaded = false;
    g_config_manager.config_file[0] = '\0';
}

/* 解析大小字符串 (支持K, M, G后缀) */
uint64_t distfs_config_parse_size(const char *size_str) {
    if (!size_str) return 0;
    
    char *endptr;
    double value = strtod(size_str, &endptr);
    
    if (value < 0) return 0;
    
    uint64_t multiplier = 1;
    if (*endptr) {
        switch (toupper(*endptr)) {
            case 'K': multiplier = 1024; break;
            case 'M': multiplier = 1024 * 1024; break;
            case 'G': multiplier = 1024 * 1024 * 1024; break;
            case 'T': multiplier = 1024ULL * 1024 * 1024 * 1024; break;
            default: return 0;  // 无效后缀
        }
    }
    
    return (uint64_t)(value * multiplier);
}

/* 解析时间字符串 (支持s, m, h, d后缀) */
uint64_t distfs_config_parse_time(const char *time_str) {
    if (!time_str) return 0;
    
    char *endptr;
    double value = strtod(time_str, &endptr);
    
    if (value < 0) return 0;
    
    uint64_t multiplier = 1;  // 默认秒
    if (*endptr) {
        switch (tolower(*endptr)) {
            case 's': multiplier = 1; break;
            case 'm': multiplier = 60; break;
            case 'h': multiplier = 3600; break;
            case 'd': multiplier = 86400; break;
            default: return 0;  // 无效后缀
        }
    }
    
    return (uint64_t)(value * multiplier);
}
