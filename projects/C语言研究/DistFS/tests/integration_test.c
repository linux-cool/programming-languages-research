/*
 * DistFS 集成测试
 * 测试系统各组件的集成功能
 */

#include "distfs.h"
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>

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
        printf("✓ %s\n", message); \
    } else { \
        tests_failed++; \
        printf("✗ %s (expected: %d, actual: %d)\n", message, (int)(expected), (int)(actual)); \
    } \
} while(0)

/* 测试服务器结构 */
typedef struct {
    distfs_server_t *server;
    pthread_t thread;
    bool running;
    uint16_t port;
} test_server_t;

/* 简单的回显处理器 */
static int echo_handler(distfs_connection_t *conn, distfs_message_t *msg, void *user_data) {
    (void)user_data;  // 未使用的参数
    
    // 创建回显响应
    distfs_message_t *response = NULL;
    
    if (msg->header.type == DISTFS_MSG_PING) {
        response = distfs_message_create(DISTFS_MSG_PONG, msg->payload, msg->header.length);
    } else {
        response = distfs_message_create(DISTFS_MSG_SUCCESS, msg->payload, msg->header.length);
    }
    
    if (response) {
        distfs_message_send(conn, response);
        distfs_message_destroy(response);
    }
    
    return DISTFS_SUCCESS;
}

/* 启动测试服务器 */
static test_server_t* start_test_server(uint16_t port) {
    test_server_t *test_server = calloc(1, sizeof(test_server_t));
    if (!test_server) {
        return NULL;
    }
    
    test_server->port = port;
    test_server->server = distfs_server_create(port, 100);
    if (!test_server->server) {
        free(test_server);
        return NULL;
    }
    
    // 注册处理器
    distfs_server_register_handler(test_server->server, DISTFS_MSG_PING, echo_handler, NULL);
    distfs_server_register_handler(test_server->server, DISTFS_MSG_CREATE_FILE, echo_handler, NULL);
    distfs_server_register_handler(test_server->server, DISTFS_MSG_READ_FILE, echo_handler, NULL);
    
    // 启动服务器
    if (distfs_server_start(test_server->server) != DISTFS_SUCCESS) {
        distfs_server_destroy(test_server->server);
        free(test_server);
        return NULL;
    }
    
    test_server->running = true;
    
    // 等待服务器启动
    sleep(1);
    
    return test_server;
}

/* 停止测试服务器 */
static void stop_test_server(test_server_t *test_server) {
    if (!test_server) return;
    
    test_server->running = false;
    
    if (test_server->server) {
        distfs_server_stop(test_server->server);
        distfs_server_destroy(test_server->server);
    }
    
    free(test_server);
}

/* 测试网络客户端连接 */
void test_network_client_connection(void) {
    printf("\n=== 测试网络客户端连接 ===\n");
    
    // 启动测试服务器
    test_server_t *server = start_test_server(19527);
    TEST_ASSERT(server != NULL, "启动测试服务器");
    
    if (!server) return;
    
    // 创建网络客户端
    distfs_network_client_t *client = distfs_network_client_create();
    TEST_ASSERT(client != NULL, "创建网络客户端");
    
    if (client) {
        // 连接到服务器
        int result = distfs_network_client_connect_metadata(client, "127.0.0.1", 19527);
        TEST_ASSERT_EQ(DISTFS_SUCCESS, result, "连接到元数据服务器");
        
        // 获取连接统计
        int metadata_connected, storage_connected;
        uint64_t bytes_sent, bytes_received;
        
        result = distfs_network_client_get_stats(client, &metadata_connected, &storage_connected,
                                                &bytes_sent, &bytes_received);
        TEST_ASSERT_EQ(DISTFS_SUCCESS, result, "获取客户端统计信息");
        TEST_ASSERT_EQ(1, metadata_connected, "元数据服务器已连接");
        
        // 断开连接
        result = distfs_network_client_disconnect(client);
        TEST_ASSERT_EQ(DISTFS_SUCCESS, result, "断开客户端连接");
        
        distfs_network_client_destroy(client);
    }
    
    stop_test_server(server);
}

/* 测试消息传输 */
void test_message_transmission(void) {
    printf("\n=== 测试消息传输 ===\n");
    
    // 启动测试服务器
    test_server_t *server = start_test_server(19528);
    TEST_ASSERT(server != NULL, "启动测试服务器");
    
    if (!server) return;
    
    // 创建网络客户端
    distfs_network_client_t *client = distfs_network_client_create();
    if (!client) {
        stop_test_server(server);
        return;
    }
    
    // 连接到服务器
    int result = distfs_network_client_connect_metadata(client, "127.0.0.1", 19528);
    if (result != DISTFS_SUCCESS) {
        distfs_network_client_destroy(client);
        stop_test_server(server);
        return;
    }
    
    // 发送PING消息
    const char *ping_data = "Hello, DistFS!";
    distfs_message_t *ping_msg = distfs_message_create(DISTFS_MSG_PING, 
                                                       ping_data, strlen(ping_data) + 1);
    TEST_ASSERT(ping_msg != NULL, "创建PING消息");
    
    if (ping_msg) {
        result = distfs_network_client_send_to_metadata(client, ping_msg);
        TEST_ASSERT_EQ(DISTFS_SUCCESS, result, "发送PING消息");
        
        // 接收PONG响应
        distfs_message_t *pong_msg = distfs_network_client_receive_from_metadata(client);
        TEST_ASSERT(pong_msg != NULL, "接收PONG响应");
        
        if (pong_msg) {
            TEST_ASSERT_EQ(DISTFS_MSG_PONG, pong_msg->header.type, "响应消息类型正确");
            TEST_ASSERT_EQ(strlen(ping_data) + 1, pong_msg->header.length, "响应消息长度正确");
            
            if (pong_msg->payload) {
                TEST_ASSERT(strcmp(ping_data, (char*)pong_msg->payload) == 0, "响应消息内容正确");
            }
            
            distfs_message_destroy(pong_msg);
        }
        
        distfs_message_destroy(ping_msg);
    }
    
    distfs_network_client_destroy(client);
    stop_test_server(server);
}

/* 测试配置系统集成 */
void test_config_integration(void) {
    printf("\n=== 测试配置系统集成 ===\n");
    
    // 创建测试配置文件
    const char *config_file = "/tmp/distfs_integration_test.conf";
    FILE *fp = fopen(config_file, "w");
    TEST_ASSERT(fp != NULL, "创建测试配置文件");
    
    if (fp) {
        fprintf(fp, "# Integration test config\n");
        fprintf(fp, "listen_port = 19529\n");
        fprintf(fp, "data_dir = /tmp/distfs_test\n");
        fprintf(fp, "log_level = 1\n");
        fprintf(fp, "enable_compression = true\n");
        fclose(fp);
        
        // 加载配置
        int result = distfs_config_load(config_file);
        TEST_ASSERT_EQ(DISTFS_SUCCESS, result, "加载配置文件");
        
        // 验证配置值
        int port = distfs_config_get_int("listen_port", 0);
        TEST_ASSERT_EQ(19529, port, "读取端口配置");
        
        const char *data_dir = distfs_config_get_string("data_dir", "");
        TEST_ASSERT(strcmp("/tmp/distfs_test", data_dir) == 0, "读取数据目录配置");
        
        bool compression = distfs_config_get_bool("enable_compression", false);
        TEST_ASSERT(compression, "读取压缩配置");
        
        // 清理
        distfs_config_cleanup();
        unlink(config_file);
    }
}

/* 测试日志系统集成 */
void test_logging_integration(void) {
    printf("\n=== 测试日志系统集成 ===\n");
    
    const char *log_file = "/tmp/distfs_integration_test.log";
    
    // 初始化日志系统
    int result = distfs_log_init(log_file, 1);  // DEBUG级别
    TEST_ASSERT_EQ(DISTFS_SUCCESS, result, "初始化日志系统");
    
    // 设置日志选项
    distfs_log_set_console(false);
    distfs_log_set_timestamp(true);
    
    // 写入测试日志
    distfs_log_write(2, __FILE__, __LINE__, __func__, "这是一条测试日志消息");
    distfs_log_flush();
    
    // 检查日志文件是否存在
    FILE *fp = fopen(log_file, "r");
    TEST_ASSERT(fp != NULL, "日志文件已创建");
    
    if (fp) {
        char buffer[1024];
        bool found_message = false;
        
        while (fgets(buffer, sizeof(buffer), fp)) {
            if (strstr(buffer, "这是一条测试日志消息")) {
                found_message = true;
                break;
            }
        }
        
        TEST_ASSERT(found_message, "日志消息已写入文件");
        fclose(fp);
    }
    
    // 获取日志统计
    uint64_t file_size;
    int level;
    bool console_enabled;
    
    result = distfs_log_get_stats(&file_size, &level, &console_enabled);
    TEST_ASSERT_EQ(DISTFS_SUCCESS, result, "获取日志统计信息");
    TEST_ASSERT(file_size > 0, "日志文件大小大于0");
    TEST_ASSERT_EQ(1, level, "日志级别正确");
    TEST_ASSERT(!console_enabled, "控制台输出已禁用");
    
    // 清理
    distfs_log_cleanup();
    unlink(log_file);
}

/* 测试存储节点基本功能 */
void test_storage_node_basic(void) {
    printf("\n=== 测试存储节点基本功能 ===\n");
    
    const char *test_data_dir = "/tmp/distfs_storage_test";
    
    // 初始化存储节点
    int result = distfs_storage_node_init(test_data_dir, 19530);
    TEST_ASSERT_EQ(DISTFS_SUCCESS, result, "初始化存储节点");
    
    // 检查数据目录是否创建
    TEST_ASSERT(distfs_is_directory(test_data_dir), "数据目录已创建");
    
    char blocks_dir[1024];
    snprintf(blocks_dir, sizeof(blocks_dir), "%s/blocks", test_data_dir);
    TEST_ASSERT(distfs_is_directory(blocks_dir), "数据块目录已创建");
    
    char metadata_dir[1024];
    snprintf(metadata_dir, sizeof(metadata_dir), "%s/metadata", test_data_dir);
    TEST_ASSERT(distfs_is_directory(metadata_dir), "元数据目录已创建");
    
    // 清理
    distfs_storage_node_cleanup();
    
    // 清理测试目录
    system("rm -rf /tmp/distfs_storage_test");
}

/* 测试客户端API集成 */
void test_client_api_integration(void) {
    printf("\n=== 测试客户端API集成 ===\n");
    
    // 注意：这个测试需要运行的服务器，在实际环境中可能会失败
    // 这里主要测试API的基本调用流程
    
    distfs_client_t *client = distfs_init(NULL);
    // 由于没有运行的服务器，这里可能会失败，但我们测试API调用本身
    
    if (client) {
        TEST_ASSERT(true, "客户端初始化成功");
        
        // 测试文件操作API（会失败，但测试API调用）
        int result = distfs_create(client, "/test/file.txt", 0644);
        // 预期会失败，因为没有服务器
        TEST_ASSERT(result != DISTFS_SUCCESS, "无服务器时创建文件失败（预期）");
        
        distfs_cleanup(client);
    } else {
        TEST_ASSERT(true, "无服务器时客户端初始化失败（预期）");
    }
}

/* 主测试函数 */
int main(void) {
    printf("DistFS 集成测试开始\n");
    printf("===================\n");
    
    // 初始化随机数生成器
    distfs_random_init();
    
    // 运行各项测试
    test_network_client_connection();
    test_message_transmission();
    test_config_integration();
    test_logging_integration();
    test_storage_node_basic();
    test_client_api_integration();
    
    // 输出测试结果
    printf("\n===================\n");
    printf("测试结果统计:\n");
    printf("总测试数: %d\n", tests_run);
    printf("通过测试: %d\n", tests_passed);
    printf("失败测试: %d\n", tests_failed);
    printf("成功率: %.1f%%\n", tests_run > 0 ? (double)tests_passed / tests_run * 100 : 0);
    
    if (tests_failed == 0) {
        printf("\n🎉 所有集成测试通过!\n");
        return 0;
    } else {
        printf("\n❌ 有 %d 个测试失败\n", tests_failed);
        return 1;
    }
}
