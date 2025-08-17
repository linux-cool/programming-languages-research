/*
 * DistFS 配置管理模块
 * 提供配置文件解析和管理功能
 */

#include "distfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#define _GNU_SOURCE
#include <string.h>

/* 配置项结构 */
typedef struct config_item {
    char *key;
    char *value;
    struct config_item *next;
} config_item_t;

/* 配置管理结构 */
struct distfs_config_manager {
    config_item_t *items;
    pthread_mutex_t mutex;
    char config_file[DISTFS_MAX_PATH_LEN];
    time_t last_modified;
};

/* 全局配置管理器 */
static distfs_config_manager_t *g_config_manager = NULL;

/* 去除字符串首尾空白字符 */
static char* trim_whitespace(char *str) {
    char *end;
    
    // 去除前导空白
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) return str;
    
    // 去除尾随空白
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    
    end[1] = '\0';
    return str;
}

/* 创建配置项 */
static config_item_t* create_config_item(const char *key, const char *value) {
    config_item_t *item = malloc(sizeof(config_item_t));
    if (!item) return NULL;
    
    item->key = strdup(key);
    item->value = strdup(value);
    item->next = NULL;
    
    if (!item->key || !item->value) {
        free(item->key);
        free(item->value);
        free(item);
        return NULL;
    }
    
    return item;
}

/* 释放配置项 */
static void free_config_item(config_item_t *item) {
    if (item) {
        free(item->key);
        free(item->value);
        free(item);
    }
}

/* 查找配置项 */
static config_item_t* find_config_item(config_item_t *head, const char *key) {
    config_item_t *current = head;
    while (current) {
        if (strcmp(current->key, key) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/* 初始化配置管理器 */
int distfs_config_init(const char *config_file) {
    if (g_config_manager) {
        return DISTFS_ERROR_ALREADY_INITIALIZED;
    }
    
    g_config_manager = malloc(sizeof(distfs_config_manager_t));
    if (!g_config_manager) {
        return DISTFS_ERROR_NO_MEMORY;
    }
    
    memset(g_config_manager, 0, sizeof(distfs_config_manager_t));
    
    if (pthread_mutex_init(&g_config_manager->mutex, NULL) != 0) {
        free(g_config_manager);
        g_config_manager = NULL;
        return DISTFS_ERROR_SYSTEM_ERROR;
    }
    
    if (config_file) {
        strncpy(g_config_manager->config_file, config_file, 
                sizeof(g_config_manager->config_file) - 1);
        return distfs_config_load(config_file);
    }
    
    return DISTFS_SUCCESS;
}

/* 清理配置管理器 */
void distfs_config_cleanup(void) {
    if (!g_config_manager) return;
    
    pthread_mutex_lock(&g_config_manager->mutex);
    
    config_item_t *current = g_config_manager->items;
    while (current) {
        config_item_t *next = current->next;
        free_config_item(current);
        current = next;
    }
    
    pthread_mutex_unlock(&g_config_manager->mutex);
    pthread_mutex_destroy(&g_config_manager->mutex);
    
    free(g_config_manager);
    g_config_manager = NULL;
}

/* 加载配置文件 */
int distfs_config_load(const char *config_file) {
    if (!g_config_manager || !config_file) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    FILE *fp = fopen(config_file, "r");
    if (!fp) {
        return DISTFS_ERROR_FILE_NOT_FOUND;
    }
    
    pthread_mutex_lock(&g_config_manager->mutex);
    
    // 清除现有配置
    config_item_t *current = g_config_manager->items;
    while (current) {
        config_item_t *next = current->next;
        free_config_item(current);
        current = next;
    }
    g_config_manager->items = NULL;
    
    char line[1024];
    int line_number = 0;
    int result = DISTFS_SUCCESS;
    
    while (fgets(line, sizeof(line), fp)) {
        line_number++;
        
        // 去除换行符
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        // 去除首尾空白
        char *trimmed = trim_whitespace(line);
        
        // 跳过空行和注释
        if (*trimmed == '\0' || *trimmed == '#') {
            continue;
        }
        
        // 查找等号分隔符
        char *equals = strchr(trimmed, '=');
        if (!equals) {
            fprintf(stderr, "Invalid config line %d: %s\n", line_number, trimmed);
            continue;
        }
        
        *equals = '\0';
        char *key = trim_whitespace(trimmed);
        char *value = trim_whitespace(equals + 1);
        
        if (*key == '\0') {
            fprintf(stderr, "Empty key on line %d\n", line_number);
            continue;
        }
        
        // 创建配置项
        config_item_t *item = create_config_item(key, value);
        if (!item) {
            result = DISTFS_ERROR_NO_MEMORY;
            break;
        }
        
        // 添加到链表头部
        item->next = g_config_manager->items;
        g_config_manager->items = item;
    }
    
    // 获取文件修改时间
    struct stat st;
    if (stat(config_file, &st) == 0) {
        g_config_manager->last_modified = st.st_mtime;
    }
    
    pthread_mutex_unlock(&g_config_manager->mutex);
    fclose(fp);
    
    return result;
}

/* 获取字符串配置值 */
const char* distfs_config_get_string(const char *key, const char *default_value) {
    if (!g_config_manager || !key) {
        return default_value;
    }
    
    pthread_mutex_lock(&g_config_manager->mutex);
    
    config_item_t *item = find_config_item(g_config_manager->items, key);
    const char *value = item ? item->value : default_value;
    
    pthread_mutex_unlock(&g_config_manager->mutex);
    
    return value;
}

/* 获取整数配置值 */
int distfs_config_get_int(const char *key, int default_value) {
    const char *str_value = distfs_config_get_string(key, NULL);
    if (!str_value) {
        return default_value;
    }
    
    char *endptr;
    long value = strtol(str_value, &endptr, 10);
    
    if (*endptr != '\0' || value < INT_MIN || value > INT_MAX) {
        return default_value;
    }
    
    return (int)value;
}

/* 获取布尔配置值 */
bool distfs_config_get_bool(const char *key, bool default_value) {
    const char *str_value = distfs_config_get_string(key, NULL);
    if (!str_value) {
        return default_value;
    }
    
    if (strcasecmp(str_value, "true") == 0 ||
        strcasecmp(str_value, "yes") == 0 ||
        strcasecmp(str_value, "1") == 0) {
        return true;
    }
    
    if (strcasecmp(str_value, "false") == 0 ||
        strcasecmp(str_value, "no") == 0 ||
        strcasecmp(str_value, "0") == 0) {
        return false;
    }
    
    return default_value;
}

/* 设置配置值 */
int distfs_config_set(const char *key, const char *value) {
    if (!g_config_manager || !key || !value) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_config_manager->mutex);
    
    config_item_t *item = find_config_item(g_config_manager->items, key);
    if (item) {
        // 更新现有配置项
        char *new_value = strdup(value);
        if (!new_value) {
            pthread_mutex_unlock(&g_config_manager->mutex);
            return DISTFS_ERROR_NO_MEMORY;
        }
        free(item->value);
        item->value = new_value;
    } else {
        // 创建新配置项
        item = create_config_item(key, value);
        if (!item) {
            pthread_mutex_unlock(&g_config_manager->mutex);
            return DISTFS_ERROR_NO_MEMORY;
        }
        item->next = g_config_manager->items;
        g_config_manager->items = item;
    }
    
    pthread_mutex_unlock(&g_config_manager->mutex);
    return DISTFS_SUCCESS;
}

/* 检查配置文件是否已修改 */
bool distfs_config_is_modified(void) {
    if (!g_config_manager || g_config_manager->config_file[0] == '\0') {
        return false;
    }
    
    struct stat st;
    if (stat(g_config_manager->config_file, &st) != 0) {
        return false;
    }
    
    return st.st_mtime > g_config_manager->last_modified;
}

/* 重新加载配置文件 */
int distfs_config_reload(void) {
    if (!g_config_manager || g_config_manager->config_file[0] == '\0') {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    return distfs_config_load(g_config_manager->config_file);
}
