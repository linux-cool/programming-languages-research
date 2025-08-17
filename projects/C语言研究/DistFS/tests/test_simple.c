/*
 * DistFS 简单测试
 * 验证基础功能
 */

#include "distfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("DistFS 简单测试开始\n");
    
    // 测试哈希算法
    const char *test_data = "Hello DistFS";
    uint32_t crc32_hash = distfs_hash_crc32(test_data, strlen(test_data));
    printf("CRC32 哈希: 0x%08x\n", crc32_hash);
    
    uint32_t fnv_hash = distfs_hash_fnv1a(test_data, strlen(test_data));
    printf("FNV-1a 哈希: 0x%08x\n", fnv_hash);
    
    uint32_t murmur_hash = distfs_hash_murmur3(test_data, strlen(test_data), 0);
    printf("MurmurHash3: 0x%08x\n", murmur_hash);
    
    // 测试配置系统
    printf("初始化配置系统...\n");
    int result = distfs_config_init(NULL);
    if (result == DISTFS_SUCCESS) {
        printf("配置系统初始化成功\n");
        
        const char *str_value = distfs_config_get_string("test.key", "default_value");
        printf("配置值: %s\n", str_value);
        
        distfs_config_cleanup();
        printf("配置系统清理完成\n");
    } else {
        printf("配置系统初始化失败: %d\n", result);
    }
    
    // 测试日志系统
    printf("初始化日志系统...\n");
    result = distfs_log_init(NULL, DISTFS_LOG_INFO);
    if (result == DISTFS_SUCCESS) {
        printf("日志系统初始化成功\n");
        
        DISTFS_LOG_INFO("这是一条测试日志");
        
        distfs_log_cleanup();
        printf("日志系统清理完成\n");
    } else {
        printf("日志系统初始化失败: %d\n", result);
    }
    
    printf("所有测试完成!\n");
    return 0;
}
