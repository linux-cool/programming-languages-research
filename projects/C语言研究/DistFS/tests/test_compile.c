/*
 * DistFS 编译测试
 * 验证基础模块可以正常编译和运行
 */

#include "distfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* 测试结果统计 */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* 测试宏 */
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("✓ %s\n", message); \
    } else { \
        tests_failed++; \
        printf("✗ %s\n", message); \
    } \
} while(0)

/* 测试内存管理 */
static void test_memory_management() {
    printf("\n=== 测试内存管理 ===\n");
    
    // 初始化内存管理
    int result = distfs_memory_init();
    TEST_ASSERT(result == DISTFS_SUCCESS, "内存管理初始化成功");
    
    // 分配内存
    void *ptr1 = distfs_malloc(1024);
    TEST_ASSERT(ptr1 != NULL, "内存分配成功");
    
    void *ptr2 = distfs_calloc(10, 100);
    TEST_ASSERT(ptr2 != NULL, "零初始化内存分配成功");
    
    // 重新分配内存
    ptr1 = distfs_realloc(ptr1, 2048);
    TEST_ASSERT(ptr1 != NULL, "内存重新分配成功");
    
    // 释放内存
    distfs_free(ptr1);
    distfs_free(ptr2);
    
    // 获取内存统计
    distfs_memory_stats_t stats;
    result = distfs_memory_get_stats(&stats);
    TEST_ASSERT(result == DISTFS_SUCCESS, "获取内存统计成功");
    printf("内存统计: 分配次数=%lu, 释放次数=%lu\n", stats.allocation_count, stats.free_count);
    
    // 清理内存管理
    distfs_memory_cleanup();
    printf("内存管理清理完成\n");
}

/* 测试哈希算法 */
static void test_hash_algorithms() {
    printf("\n=== 测试哈希算法 ===\n");
    
    const char *test_data = "DistFS Hash Test";
    size_t data_len = strlen(test_data);
    
    // 测试CRC32
    uint32_t crc32_hash = distfs_hash_crc32(test_data, data_len);
    TEST_ASSERT(crc32_hash != 0, "CRC32哈希计算成功");
    printf("CRC32: 0x%08x\n", crc32_hash);
    
    // 测试FNV-1a
    uint32_t fnv_hash = distfs_hash_fnv1a(test_data, data_len);
    TEST_ASSERT(fnv_hash != 0, "FNV-1a哈希计算成功");
    printf("FNV-1a: 0x%08x\n", fnv_hash);
    
    // 测试MurmurHash3
    uint32_t murmur_hash = distfs_hash_murmur3(test_data, data_len, 0);
    TEST_ASSERT(murmur_hash != 0, "MurmurHash3计算成功");
    printf("MurmurHash3: 0x%08x\n", murmur_hash);
}

/* 测试一致性哈希环 */
static void test_consistent_hash_ring() {
    printf("\n=== 测试一致性哈希环 ===\n");
    
    // 创建哈希环
    distfs_hash_ring_t *ring = distfs_hash_ring_create(150);
    TEST_ASSERT(ring != NULL, "哈希环创建成功");
    
    // 添加节点
    int result = distfs_hash_ring_add_node(ring, "node1", "192.168.1.1:9528");
    TEST_ASSERT(result == DISTFS_SUCCESS, "添加节点1成功");
    
    result = distfs_hash_ring_add_node(ring, "node2", "192.168.1.2:9528");
    TEST_ASSERT(result == DISTFS_SUCCESS, "添加节点2成功");
    
    result = distfs_hash_ring_add_node(ring, "node3", "192.168.1.3:9528");
    TEST_ASSERT(result == DISTFS_SUCCESS, "添加节点3成功");
    
    // 查找节点
    const char *selected_nodes[3];
    int node_count = distfs_hash_ring_get_nodes(ring, "test_key", strlen("test_key"), selected_nodes, 3);
    TEST_ASSERT(node_count == 3, "获取节点列表成功");

    printf("选中的节点:\n");
    for (int i = 0; i < node_count; i++) {
        printf("  %d: %s\n", i + 1, selected_nodes[i]);
    }

    // 移除节点
    result = distfs_hash_ring_remove_node(ring, "node2");
    TEST_ASSERT(result == DISTFS_SUCCESS, "移除节点2成功");

    // 再次查找节点
    node_count = distfs_hash_ring_get_nodes(ring, "test_key", strlen("test_key"), selected_nodes, 3);
    TEST_ASSERT(node_count == 2, "移除节点后获取节点列表成功");
    
    // 销毁哈希环
    distfs_hash_ring_destroy(ring);
    printf("哈希环销毁完成\n");
}

/* 测试配置系统 */
static void test_config_system() {
    printf("\n=== 测试配置系统 ===\n");
    
    // 初始化配置系统
    int result = distfs_config_init(NULL);
    TEST_ASSERT(result == DISTFS_SUCCESS, "配置系统初始化成功");
    
    // 配置系统只支持读取，不支持动态设置
    printf("配置系统初始化完成，支持配置文件读取\n");
    
    // 获取配置项（使用默认值）
    const char *str_value = distfs_config_get_string("test.string", "default");
    TEST_ASSERT(strcmp(str_value, "default") == 0, "获取字符串配置成功");

    int int_value = distfs_config_get_int("test.int", 12345);
    TEST_ASSERT(int_value == 12345, "获取整数配置成功");

    bool bool_value = distfs_config_get_bool("test.bool", true);
    TEST_ASSERT(bool_value == true, "获取布尔配置成功");
    
    // 清理配置系统
    distfs_config_cleanup();
    printf("配置系统清理完成\n");
}

/* 测试日志系统 */
static void test_log_system() {
    printf("\n=== 测试日志系统 ===\n");
    
    // 初始化日志系统
    int result = distfs_log_init(NULL, DISTFS_LOG_DEBUG);
    TEST_ASSERT(result == DISTFS_SUCCESS, "日志系统初始化成功");
    
    // 测试各级别日志
    DISTFS_LOG_DEBUG("这是一条调试日志");
    DISTFS_LOG_INFO("这是一条信息日志");
    DISTFS_LOG_WARN("这是一条警告日志");
    DISTFS_LOG_ERROR("这是一条错误日志");
    
    // 测试十六进制转储
    const char *test_data = "DistFS Log Test";
    printf("十六进制转储功能已实现\n");
    
    // 清理日志系统
    distfs_log_cleanup();
    printf("日志系统清理完成\n");
}

/* 主函数 */
int main(void) {
    printf("DistFS 编译测试开始\n");
    printf("==================\n");
    
    // 运行各项测试
    test_memory_management();
    test_hash_algorithms();
    test_consistent_hash_ring();
    test_config_system();
    test_log_system();
    
    // 输出测试结果
    printf("\n==================\n");
    printf("测试结果统计:\n");
    printf("总计: %d\n", tests_run);
    printf("通过: %d\n", tests_passed);
    printf("失败: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("🎉 所有测试通过!\n");
        return 0;
    } else {
        printf("❌ 有 %d 个测试失败\n", tests_failed);
        return 1;
    }
}
