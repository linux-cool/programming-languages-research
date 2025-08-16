/*
 * DistFS 基础功能测试
 * 测试分布式文件系统的基本功能
 */

#include "distfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>

/* 测试结果统计 */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* 测试宏定义 */
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

#define TEST_ASSERT_EQ(expected, actual, message) do { \
    tests_run++; \
    if ((expected) == (actual)) { \
        tests_passed++; \
        printf("✓ %s (expected: %d, actual: %d)\n", message, (int)(expected), (int)(actual)); \
    } else { \
        tests_failed++; \
        printf("✗ %s (expected: %d, actual: %d)\n", message, (int)(expected), (int)(actual)); \
    } \
} while(0)

#define TEST_ASSERT_STR_EQ(expected, actual, message) do { \
    tests_run++; \
    if (strcmp((expected), (actual)) == 0) { \
        tests_passed++; \
        printf("✓ %s\n", message); \
    } else { \
        tests_failed++; \
        printf("✗ %s (expected: '%s', actual: '%s')\n", message, (expected), (actual)); \
    } \
} while(0)

/* 测试工具函数 */
void test_utils(void) {
    printf("\n=== 测试工具函数 ===\n");
    
    // 测试错误码转换
    const char *error_str = distfs_strerror(DISTFS_SUCCESS);
    TEST_ASSERT_STR_EQ("Success", error_str, "错误码转换 - 成功");
    
    error_str = distfs_strerror(DISTFS_ERROR_FILE_NOT_FOUND);
    TEST_ASSERT_STR_EQ("File not found", error_str, "错误码转换 - 文件未找到");
    
    // 测试时间戳
    uint64_t timestamp1 = distfs_get_timestamp();
    usleep(1000);  // 等待1ms
    uint64_t timestamp2 = distfs_get_timestamp();
    TEST_ASSERT(timestamp2 > timestamp1, "时间戳递增");
    
    // 测试校验和
    const char *test_data = "Hello, DistFS!";
    uint32_t checksum1 = distfs_calculate_checksum(test_data, strlen(test_data));
    uint32_t checksum2 = distfs_calculate_checksum(test_data, strlen(test_data));
    TEST_ASSERT_EQ(checksum1, checksum2, "校验和一致性");
    
    const char *test_data2 = "Hello, DistFS?";
    uint32_t checksum3 = distfs_calculate_checksum(test_data2, strlen(test_data2));
    TEST_ASSERT(checksum1 != checksum3, "不同数据校验和不同");
}

/* 测试网络消息 */
void test_network_message(void) {
    printf("\n=== 测试网络消息 ===\n");
    
    // 测试消息创建
    const char *payload = "Test message payload";
    distfs_message_t *msg = distfs_message_create(DISTFS_MSG_PING, 
                                                  payload, strlen(payload));
    TEST_ASSERT(msg != NULL, "消息创建成功");
    
    if (msg) {
        // 测试消息头
        TEST_ASSERT_EQ(0x44495354, msg->header.magic, "消息魔数正确");
        TEST_ASSERT_EQ(DISTFS_PROTOCOL_VERSION, msg->header.version, "协议版本正确");
        TEST_ASSERT_EQ(DISTFS_MSG_PING, msg->header.type, "消息类型正确");
        TEST_ASSERT_EQ(strlen(payload), msg->header.length, "消息长度正确");
        
        // 测试payload
        TEST_ASSERT(msg->payload != NULL, "Payload不为空");
        if (msg->payload) {
            TEST_ASSERT(memcmp(msg->payload, payload, strlen(payload)) == 0, "Payload内容正确");
        }
        
        // 测试消息验证
        int validation_result = distfs_validate_message(msg);
        TEST_ASSERT_EQ(DISTFS_SUCCESS, validation_result, "消息验证通过");
        
        // 测试消息类型转换
        const char *type_str = distfs_msg_type_to_string(DISTFS_MSG_PING);
        TEST_ASSERT_STR_EQ("PING", type_str, "消息类型转换正确");
        
        distfs_message_destroy(msg);
    }
    
    // 测试空消息
    distfs_message_t *empty_msg = distfs_message_create(DISTFS_MSG_PONG, NULL, 0);
    TEST_ASSERT(empty_msg != NULL, "空消息创建成功");
    
    if (empty_msg) {
        TEST_ASSERT_EQ(0, empty_msg->header.length, "空消息长度为0");
        TEST_ASSERT(empty_msg->payload == NULL, "空消息payload为NULL");
        
        int validation_result = distfs_validate_message(empty_msg);
        TEST_ASSERT_EQ(DISTFS_SUCCESS, validation_result, "空消息验证通过");
        
        distfs_message_destroy(empty_msg);
    }
}

/* 测试文件元数据 */
void test_file_metadata(void) {
    printf("\n=== 测试文件元数据 ===\n");
    
    // 创建测试元数据
    distfs_file_metadata_t metadata;
    memset(&metadata, 0, sizeof(metadata));
    
    metadata.inode = 12345;
    strcpy(metadata.name, "test_file.txt");
    metadata.type = DISTFS_FILE_TYPE_REGULAR;
    metadata.size = 1024;
    metadata.mode = 0644;
    metadata.uid = 1000;
    metadata.gid = 1000;
    metadata.atime = distfs_get_timestamp_sec();
    metadata.mtime = metadata.atime;
    metadata.ctime = metadata.atime;
    metadata.nlinks = 1;
    metadata.block_count = 1;
    metadata.blocks[0] = 67890;
    
    // 计算校验和
    metadata.checksum = distfs_calculate_checksum(&metadata, 
                                                  sizeof(metadata) - sizeof(metadata.checksum));
    
    // 验证元数据
    TEST_ASSERT_EQ(12345, metadata.inode, "inode正确");
    TEST_ASSERT_STR_EQ("test_file.txt", metadata.name, "文件名正确");
    TEST_ASSERT_EQ(DISTFS_FILE_TYPE_REGULAR, metadata.type, "文件类型正确");
    TEST_ASSERT_EQ(1024, metadata.size, "文件大小正确");
    TEST_ASSERT_EQ(0644, metadata.mode, "文件权限正确");
    TEST_ASSERT_EQ(1, metadata.block_count, "数据块数量正确");
    TEST_ASSERT_EQ(67890, metadata.blocks[0], "数据块ID正确");
    TEST_ASSERT(metadata.checksum != 0, "校验和不为0");
}

/* 测试节点信息 */
void test_node_info(void) {
    printf("\n=== 测试节点信息 ===\n");
    
    // 创建测试节点信息
    distfs_node_info_t node_info;
    memset(&node_info, 0, sizeof(node_info));
    
    node_info.node_id = 1001;
    node_info.type = DISTFS_NODE_STORAGE;
    node_info.status = DISTFS_NODE_STATUS_ONLINE;
    strcpy(node_info.addr.ip, "192.168.1.100");
    node_info.addr.port = 9528;
    node_info.capacity = 1024 * 1024 * 1024;  // 1GB
    node_info.used = 512 * 1024 * 1024;       // 512MB
    node_info.last_heartbeat = distfs_get_timestamp_sec();
    strcpy(node_info.version, "1.0.0");
    
    // 验证节点信息
    TEST_ASSERT_EQ(1001, node_info.node_id, "节点ID正确");
    TEST_ASSERT_EQ(DISTFS_NODE_STORAGE, node_info.type, "节点类型正确");
    TEST_ASSERT_EQ(DISTFS_NODE_STATUS_ONLINE, node_info.status, "节点状态正确");
    TEST_ASSERT_STR_EQ("192.168.1.100", node_info.addr.ip, "节点IP正确");
    TEST_ASSERT_EQ(9528, node_info.addr.port, "节点端口正确");
    TEST_ASSERT_EQ(1024 * 1024 * 1024, node_info.capacity, "节点容量正确");
    TEST_ASSERT_EQ(512 * 1024 * 1024, node_info.used, "节点使用量正确");
    TEST_ASSERT_STR_EQ("1.0.0", node_info.version, "节点版本正确");
    
    // 计算使用率
    double usage_ratio = (double)node_info.used / node_info.capacity;
    TEST_ASSERT(usage_ratio == 0.5, "节点使用率计算正确");
}

/* 测试集群状态 */
void test_cluster_status(void) {
    printf("\n=== 测试集群状态 ===\n");
    
    // 创建测试集群状态
    distfs_cluster_status_t cluster_status;
    memset(&cluster_status, 0, sizeof(cluster_status));
    
    cluster_status.total_nodes = 5;
    cluster_status.online_nodes = 4;
    cluster_status.metadata_nodes = 1;
    cluster_status.storage_nodes = 3;
    cluster_status.total_capacity = 5 * 1024 * 1024 * 1024ULL;  // 5GB
    cluster_status.used_capacity = 2 * 1024 * 1024 * 1024ULL;   // 2GB
    cluster_status.total_files = 1000;
    cluster_status.load_factor = 0.4;
    
    // 验证集群状态
    TEST_ASSERT_EQ(5, cluster_status.total_nodes, "总节点数正确");
    TEST_ASSERT_EQ(4, cluster_status.online_nodes, "在线节点数正确");
    TEST_ASSERT_EQ(1, cluster_status.metadata_nodes, "元数据节点数正确");
    TEST_ASSERT_EQ(3, cluster_status.storage_nodes, "存储节点数正确");
    TEST_ASSERT_EQ(5 * 1024 * 1024 * 1024ULL, cluster_status.total_capacity, "总容量正确");
    TEST_ASSERT_EQ(2 * 1024 * 1024 * 1024ULL, cluster_status.used_capacity, "已用容量正确");
    TEST_ASSERT_EQ(1000, cluster_status.total_files, "总文件数正确");
    TEST_ASSERT(cluster_status.load_factor == 0.4, "负载因子正确");
}

/* 主测试函数 */
int main(void) {
    printf("DistFS 基础功能测试开始\n");
    printf("========================\n");
    
    // 运行各项测试
    test_utils();
    test_network_message();
    test_file_metadata();
    test_node_info();
    test_cluster_status();
    
    // 输出测试结果
    printf("\n========================\n");
    printf("测试结果统计:\n");
    printf("总测试数: %d\n", tests_run);
    printf("通过测试: %d\n", tests_passed);
    printf("失败测试: %d\n", tests_failed);
    printf("成功率: %.1f%%\n", tests_run > 0 ? (double)tests_passed / tests_run * 100 : 0);
    
    if (tests_failed == 0) {
        printf("\n🎉 所有测试通过!\n");
        return 0;
    } else {
        printf("\n❌ 有 %d 个测试失败\n", tests_failed);
        return 1;
    }
}
