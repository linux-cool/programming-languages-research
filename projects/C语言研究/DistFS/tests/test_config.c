/*
 * DistFS 配置管理测试
 * 测试配置文件解析和参数管理功能
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

/* 创建测试配置文件 */
static int create_test_config_file(const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        return DISTFS_ERROR_PERMISSION_DENIED;
    }
    
    fprintf(fp, "# DistFS Test Configuration\n");
    fprintf(fp, "\n");
    fprintf(fp, "# Network settings\n");
    fprintf(fp, "listen_port = 9527\n");
    fprintf(fp, "max_connections = 1000\n");
    fprintf(fp, "timeout = 60\n");
    fprintf(fp, "\n");
    fprintf(fp, "# Storage settings\n");
    fprintf(fp, "data_dir = /tmp/distfs/test\n");
    fprintf(fp, "block_size = 67108864\n");
    fprintf(fp, "replica_count = 3\n");
    fprintf(fp, "\n");
    fprintf(fp, "# Boolean settings\n");
    fprintf(fp, "enable_compression = true\n");
    fprintf(fp, "enable_encryption = false\n");
    fprintf(fp, "debug_mode = yes\n");
    fprintf(fp, "verbose_logging = no\n");
    fprintf(fp, "\n");
    fprintf(fp, "# Floating point settings\n");
    fprintf(fp, "load_factor = 0.75\n");
    fprintf(fp, "cache_hit_ratio = 0.95\n");
    fprintf(fp, "\n");
    fprintf(fp, "# Size settings with units\n");
    fprintf(fp, "memory_limit = 512M\n");
    fprintf(fp, "disk_quota = 10G\n");
    fprintf(fp, "buffer_size = 64K\n");
    fprintf(fp, "\n");
    fprintf(fp, "# Time settings with units\n");
    fprintf(fp, "heartbeat_interval = 30s\n");
    fprintf(fp, "session_timeout = 5m\n");
    fprintf(fp, "backup_interval = 1h\n");
    fprintf(fp, "retention_period = 7d\n");
    
    fclose(fp);
    return DISTFS_SUCCESS;
}

/* 测试配置文件加载 */
void test_config_loading(void) {
    printf("\n=== 测试配置文件加载 ===\n");
    
    const char *test_config = "/tmp/test_distfs.conf";
    
    // 创建测试配置文件
    int result = create_test_config_file(test_config);
    TEST_ASSERT_EQ(DISTFS_SUCCESS, result, "创建测试配置文件");
    
    // 加载配置文件
    result = distfs_config_load(test_config);
    TEST_ASSERT_EQ(DISTFS_SUCCESS, result, "加载配置文件");
    
    // 检查配置是否已加载
    bool loaded = distfs_config_is_loaded();
    TEST_ASSERT(loaded, "配置已加载标志");
    
    // 检查配置文件路径
    const char *config_file = distfs_config_get_file();
    TEST_ASSERT_STR_EQ(test_config, config_file, "配置文件路径");
    
    // 清理测试文件
    unlink(test_config);
}

/* 测试字符串配置获取 */
void test_string_config(void) {
    printf("\n=== 测试字符串配置 ===\n");
    
    const char *test_config = "/tmp/test_distfs.conf";
    create_test_config_file(test_config);
    distfs_config_load(test_config);
    
    // 测试存在的配置项
    const char *data_dir = distfs_config_get_string("data_dir", "/default");
    TEST_ASSERT_STR_EQ("/tmp/distfs/test", data_dir, "获取数据目录配置");
    
    // 测试不存在的配置项
    const char *unknown = distfs_config_get_string("unknown_key", "default_value");
    TEST_ASSERT_STR_EQ("default_value", unknown, "获取不存在配置项的默认值");
    
    // 测试空键
    const char *empty_key = distfs_config_get_string("", "default");
    TEST_ASSERT_STR_EQ("default", empty_key, "空键返回默认值");
    
    unlink(test_config);
    distfs_config_cleanup();
}

/* 测试整数配置获取 */
void test_integer_config(void) {
    printf("\n=== 测试整数配置 ===\n");
    
    const char *test_config = "/tmp/test_distfs.conf";
    create_test_config_file(test_config);
    distfs_config_load(test_config);
    
    // 测试存在的配置项
    int port = distfs_config_get_int("listen_port", 8080);
    TEST_ASSERT_EQ(9527, port, "获取端口配置");
    
    int max_conn = distfs_config_get_int("max_connections", 100);
    TEST_ASSERT_EQ(1000, max_conn, "获取最大连接数配置");
    
    // 测试不存在的配置项
    int unknown = distfs_config_get_int("unknown_int", 42);
    TEST_ASSERT_EQ(42, unknown, "获取不存在整数配置的默认值");
    
    // 测试无效整数值
    distfs_config_set("invalid_int", "not_a_number");
    int invalid = distfs_config_get_int("invalid_int", 100);
    TEST_ASSERT_EQ(100, invalid, "无效整数值返回默认值");
    
    unlink(test_config);
    distfs_config_cleanup();
}

/* 测试布尔配置获取 */
void test_boolean_config(void) {
    printf("\n=== 测试布尔配置 ===\n");
    
    const char *test_config = "/tmp/test_distfs.conf";
    create_test_config_file(test_config);
    distfs_config_load(test_config);
    
    // 测试true值
    bool compression = distfs_config_get_bool("enable_compression", false);
    TEST_ASSERT(compression, "获取压缩启用配置 (true)");
    
    bool debug = distfs_config_get_bool("debug_mode", false);
    TEST_ASSERT(debug, "获取调试模式配置 (yes)");
    
    // 测试false值
    bool encryption = distfs_config_get_bool("enable_encryption", true);
    TEST_ASSERT(!encryption, "获取加密启用配置 (false)");
    
    bool verbose = distfs_config_get_bool("verbose_logging", true);
    TEST_ASSERT(!verbose, "获取详细日志配置 (no)");
    
    // 测试不存在的配置项
    bool unknown = distfs_config_get_bool("unknown_bool", true);
    TEST_ASSERT(unknown, "获取不存在布尔配置的默认值");
    
    unlink(test_config);
    distfs_config_cleanup();
}

/* 测试浮点数配置获取 */
void test_double_config(void) {
    printf("\n=== 测试浮点数配置 ===\n");
    
    const char *test_config = "/tmp/test_distfs.conf";
    create_test_config_file(test_config);
    distfs_config_load(test_config);
    
    // 测试存在的配置项
    double load_factor = distfs_config_get_double("load_factor", 0.5);
    TEST_ASSERT(load_factor == 0.75, "获取负载因子配置");
    
    double hit_ratio = distfs_config_get_double("cache_hit_ratio", 0.8);
    TEST_ASSERT(hit_ratio == 0.95, "获取缓存命中率配置");
    
    // 测试不存在的配置项
    double unknown = distfs_config_get_double("unknown_double", 3.14);
    TEST_ASSERT(unknown == 3.14, "获取不存在浮点配置的默认值");
    
    unlink(test_config);
    distfs_config_cleanup();
}

/* 测试配置设置 */
void test_config_setting(void) {
    printf("\n=== 测试配置设置 ===\n");
    
    // 设置新配置项
    int result = distfs_config_set("new_key", "new_value");
    TEST_ASSERT_EQ(DISTFS_SUCCESS, result, "设置新配置项");
    
    const char *value = distfs_config_get_string("new_key", "default");
    TEST_ASSERT_STR_EQ("new_value", value, "获取新设置的配置项");
    
    // 更新现有配置项
    result = distfs_config_set("new_key", "updated_value");
    TEST_ASSERT_EQ(DISTFS_SUCCESS, result, "更新现有配置项");
    
    value = distfs_config_get_string("new_key", "default");
    TEST_ASSERT_STR_EQ("updated_value", value, "获取更新后的配置项");
    
    // 测试无效参数
    result = distfs_config_set(NULL, "value");
    TEST_ASSERT_EQ(DISTFS_ERROR_INVALID_PARAM, result, "设置空键返回错误");
    
    result = distfs_config_set("key", NULL);
    TEST_ASSERT_EQ(DISTFS_ERROR_INVALID_PARAM, result, "设置空值返回错误");
    
    distfs_config_cleanup();
}

/* 测试大小解析 */
void test_size_parsing(void) {
    printf("\n=== 测试大小解析 ===\n");
    
    // 测试不同单位
    uint64_t size_k = distfs_config_parse_size("64K");
    TEST_ASSERT_EQ(64 * 1024, size_k, "解析KB大小");
    
    uint64_t size_m = distfs_config_parse_size("512M");
    TEST_ASSERT_EQ(512 * 1024 * 1024, size_m, "解析MB大小");
    
    uint64_t size_g = distfs_config_parse_size("10G");
    TEST_ASSERT_EQ(10ULL * 1024 * 1024 * 1024, size_g, "解析GB大小");
    
    // 测试无单位
    uint64_t size_plain = distfs_config_parse_size("1024");
    TEST_ASSERT_EQ(1024, size_plain, "解析无单位大小");
    
    // 测试浮点数
    uint64_t size_float = distfs_config_parse_size("1.5M");
    TEST_ASSERT_EQ((uint64_t)(1.5 * 1024 * 1024), size_float, "解析浮点数大小");
    
    // 测试无效输入
    uint64_t size_invalid = distfs_config_parse_size("invalid");
    TEST_ASSERT_EQ(0, size_invalid, "无效大小字符串返回0");
    
    uint64_t size_null = distfs_config_parse_size(NULL);
    TEST_ASSERT_EQ(0, size_null, "空指针返回0");
}

/* 测试时间解析 */
void test_time_parsing(void) {
    printf("\n=== 测试时间解析 ===\n");
    
    // 测试不同单位
    uint64_t time_s = distfs_config_parse_time("30s");
    TEST_ASSERT_EQ(30, time_s, "解析秒时间");
    
    uint64_t time_m = distfs_config_parse_time("5m");
    TEST_ASSERT_EQ(5 * 60, time_m, "解析分钟时间");
    
    uint64_t time_h = distfs_config_parse_time("1h");
    TEST_ASSERT_EQ(3600, time_h, "解析小时时间");
    
    uint64_t time_d = distfs_config_parse_time("7d");
    TEST_ASSERT_EQ(7 * 24 * 3600, time_d, "解析天时间");
    
    // 测试无单位（默认秒）
    uint64_t time_plain = distfs_config_parse_time("120");
    TEST_ASSERT_EQ(120, time_plain, "解析无单位时间");
    
    // 测试浮点数
    uint64_t time_float = distfs_config_parse_time("1.5h");
    TEST_ASSERT_EQ((uint64_t)(1.5 * 3600), time_float, "解析浮点数时间");
    
    // 测试无效输入
    uint64_t time_invalid = distfs_config_parse_time("invalid");
    TEST_ASSERT_EQ(0, time_invalid, "无效时间字符串返回0");
}

/* 配置列表回调函数 */
static int config_count = 0;
static void config_list_callback(const char *key, const char *value, void *user_data) {
    (void)user_data;  // 未使用的参数
    
    if (key && value) {
        config_count++;
        printf("  %s = %s\n", key, value);
    }
}

/* 测试配置列表 */
void test_config_listing(void) {
    printf("\n=== 测试配置列表 ===\n");
    
    // 设置一些配置项
    distfs_config_set("key1", "value1");
    distfs_config_set("key2", "value2");
    distfs_config_set("key3", "value3");
    
    config_count = 0;
    int result = distfs_config_list(config_list_callback, NULL);
    TEST_ASSERT_EQ(DISTFS_SUCCESS, result, "列出配置项");
    TEST_ASSERT_EQ(3, config_count, "配置项数量正确");
    
    distfs_config_cleanup();
}

/* 主测试函数 */
int main(void) {
    printf("DistFS 配置管理测试开始\n");
    printf("========================\n");
    
    // 运行各项测试
    test_config_loading();
    test_string_config();
    test_integer_config();
    test_boolean_config();
    test_double_config();
    test_config_setting();
    test_size_parsing();
    test_time_parsing();
    test_config_listing();
    
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
