/*
 * DistFS 集成测试
 * 测试各个模块的集成功能
 */

#include "distfs.h"
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>

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

/* 测试网络消息创建和销毁 */
static void test_network_message() {
    printf("\n=== 测试网络消息 ===\n");
    
    const char *test_data = "Hello, DistFS!";
    size_t data_len = strlen(test_data) + 1;
    
    // 创建消息
    distfs_message_t *msg = distfs_message_create(DISTFS_MSG_PING, test_data, data_len);
    TEST_ASSERT(msg != NULL, "消息创建成功");
    TEST_ASSERT(msg->header.magic == DISTFS_PROTOCOL_MAGIC, "消息魔数正确");
    TEST_ASSERT(msg->header.type == DISTFS_MSG_PING, "消息类型正确");
    TEST_ASSERT(msg->header.length == data_len, "消息长度正确");
    TEST_ASSERT(msg->payload != NULL, "消息载荷不为空");
    TEST_ASSERT(strcmp((char*)msg->payload, test_data) == 0, "消息载荷内容正确");
    
    // 验证消息
    int result = distfs_validate_message(msg);
    TEST_ASSERT(result == DISTFS_SUCCESS, "消息验证成功");
    
    // 销毁消息
    distfs_message_destroy(msg);
    printf("消息销毁完成\n");
}

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
    printf("内存统计: 分配次数=%lu, 释放次数=%lu\n", stats.allocations, stats.deallocations);
    
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
    char selected_nodes[3][64];
    int node_count = distfs_hash_ring_get_nodes(ring, "test_key", selected_nodes, 3);
    TEST_ASSERT(node_count == 3, "获取节点列表成功");
    
    printf("选中的节点:\n");
    for (int i = 0; i < node_count; i++) {
        printf("  %d: %s\n", i + 1, selected_nodes[i]);
    }
    
    // 移除节点
    result = distfs_hash_ring_remove_node(ring, "node2");
    TEST_ASSERT(result == DISTFS_SUCCESS, "移除节点2成功");
    
    // 再次查找节点
    node_count = distfs_hash_ring_get_nodes(ring, "test_key", selected_nodes, 3);
    TEST_ASSERT(node_count == 2, "移除节点后获取节点列表成功");
    
    // 销毁哈希环
    distfs_hash_ring_destroy(ring);
    printf("哈希环销毁完成\n");
}

/* 测试缓存功能 */
static void test_cache_functionality() {
    printf("\n=== 测试缓存功能 ===\n");
    
    // 创建缓存
    distfs_cache_t *cache = distfs_cache_create(1024 * 1024, 100, 60); // 1MB, 100项, 60秒TTL
    TEST_ASSERT(cache != NULL, "缓存创建成功");
    
    // 插入数据
    const char *key1 = "test_key_1";
    const char *data1 = "test_data_1";
    int result = distfs_cache_put(cache, key1, data1, strlen(data1) + 1);
    TEST_ASSERT(result == DISTFS_SUCCESS, "缓存插入成功");
    
    // 获取数据
    void *retrieved_data;
    size_t retrieved_size;
    result = distfs_cache_get(cache, key1, &retrieved_data, &retrieved_size);
    TEST_ASSERT(result == DISTFS_SUCCESS, "缓存获取成功");
    TEST_ASSERT(strcmp((char*)retrieved_data, data1) == 0, "缓存数据正确");
    free(retrieved_data);
    
    // 插入更多数据
    for (int i = 0; i < 10; i++) {
        char key[32], data[64];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(data, sizeof(data), "data_%d", i);
        distfs_cache_put(cache, key, data, strlen(data) + 1);
    }
    
    // 获取统计信息
    distfs_cache_stats_t stats;
    result = distfs_cache_get_stats(cache, &stats);
    TEST_ASSERT(result == DISTFS_SUCCESS, "获取缓存统计成功");
    printf("缓存统计: 命中=%lu, 未命中=%lu, 命中率=%.2f%%\n", 
           stats.hits, stats.misses, stats.hit_rate * 100);
    
    // 清理缓存
    distfs_cache_destroy(cache);
    printf("缓存销毁完成\n");
}

/* 测试配置系统 */
static void test_config_system() {
    printf("\n=== 测试配置系统 ===\n");
    
    // 初始化配置系统
    int result = distfs_config_init(NULL);
    TEST_ASSERT(result == DISTFS_SUCCESS, "配置系统初始化成功");
    
    // 设置配置项
    result = distfs_config_set_string("test.string", "hello world");
    TEST_ASSERT(result == DISTFS_SUCCESS, "设置字符串配置成功");
    
    result = distfs_config_set_int("test.int", 12345);
    TEST_ASSERT(result == DISTFS_SUCCESS, "设置整数配置成功");
    
    result = distfs_config_set_bool("test.bool", true);
    TEST_ASSERT(result == DISTFS_SUCCESS, "设置布尔配置成功");
    
    // 获取配置项
    const char *str_value = distfs_config_get_string("test.string", "default");
    TEST_ASSERT(strcmp(str_value, "hello world") == 0, "获取字符串配置成功");
    
    int int_value = distfs_config_get_int("test.int", 0);
    TEST_ASSERT(int_value == 12345, "获取整数配置成功");
    
    bool bool_value = distfs_config_get_bool("test.bool", false);
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
    distfs_log_hex_dump(DISTFS_LOG_DEBUG, "测试数据", test_data, strlen(test_data));
    
    // 清理日志系统
    distfs_log_cleanup();
    printf("日志系统清理完成\n");
}

/* 主函数 */
int main(void) {
    printf("DistFS 集成测试开始\n");
    printf("==================\n");
    
    // 运行各项测试
    test_network_message();
    test_memory_management();
    test_hash_algorithms();
    test_consistent_hash_ring();
    test_cache_functionality();
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
