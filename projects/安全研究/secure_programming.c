/*
 * C语言安全编程深度研究
 * 内存安全、输入验证与防御性编程
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

// 研究1: 内存安全与缓冲区溢出防护
#define MAX_BUFFER_SIZE 1024
#define SAFE_STRING_MAX 256

typedef struct {
    char data[MAX_BUFFER_SIZE];
    size_t length;
    size_t capacity;
} SafeBuffer;

// 安全字符串结构
typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} SafeString;

// 安全字符串初始化
SafeString* safe_string_create(size_t capacity) {
    SafeString* str = (SafeString*)malloc(sizeof(SafeString));
    if (!str) return NULL;
    
    str->data = (char*)malloc(capacity);
    if (!str->data) {
        free(str);
        return NULL;
    }
    
    str->data[0] = '\0';
    str->length = 0;
    str->capacity = capacity;
    return str;
}

// 安全字符串销毁
void safe_string_destroy(SafeString* str) {
    if (str) {
        if (str->data) {
            // 清除敏感数据
            memset(str->data, 0, str->capacity);
            free(str->data);
        }
        free(str);
    }
}

// 安全字符串复制
bool safe_string_copy(SafeString* dest, const char* src) {
    if (!dest || !src) return false;
    
    size_t src_len = strnlen(src, dest->capacity);
    if (src_len >= dest->capacity) {
        return false; // 溢出风险
    }
    
    memcpy(dest->data, src, src_len);
    dest->data[src_len] = '\0';
    dest->length = src_len;
    return true;
}

// 安全字符串拼接
bool safe_string_append(SafeString* dest, const char* src) {
    if (!dest || !src) return false;
    
    size_t src_len = strnlen(src, dest->capacity - dest->length);
    if (dest->length + src_len >= dest->capacity) {
        return false; // 溢出风险
    }
    
    memcpy(dest->data + dest->length, src, src_len);
    dest->data[dest->length + src_len] = '\0';
    dest->length += src_len;
    return true;
}

// 研究2: 输入验证与数据净化
bool validate_integer(const char* input, int* result) {
    if (!input || !result) return false;
    
    char* endptr = NULL;
    errno = 0;
    
    long val = strtol(input, &endptr, 10);
    
    // 检查转换结果
    if (errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) {
        return false; // 溢出
    }
    
    if (endptr == input || *endptr != '\0') {
        return false; // 无效输入
    }
    
    if (val < INT_MIN || val > INT_MAX) {
        return false; // 超出int范围
    }
    
    *result = (int)val;
    return true;
}

bool validate_unsigned_integer(const char* input, unsigned int* result) {
    if (!input || !result) return false;
    
    char* endptr = NULL;
    errno = 0;
    
    unsigned long val = strtoul(input, &endptr, 10);
    
    if (errno == ERANGE && val == ULONG_MAX) {
        return false; // 溢出
    }
    
    if (endptr == input || *endptr != '\0') {
        return false; // 无效输入
    }
    
    if (val > UINT_MAX) {
        return false; // 超出范围
    }
    
    *result = (unsigned int)val;
    return true;
}

// 验证文件路径
bool validate_file_path(const char* path) {
    if (!path) return false;
    
    // 检查路径长度
    size_t len = strnlen(path, PATH_MAX);
    if (len == 0 || len >= PATH_MAX) {
        return false;
    }
    
    // 检查非法字符
    if (strstr(path, "..") != NULL || strstr(path, "//") != NULL) {
        return false;
    }
    
    // 检查绝对路径
    if (path[0] != '/') {
        return false;
    }
    
    return true;
}

// 验证用户名
bool validate_username(const char* username) {
    if (!username) return false;
    
    size_t len = strnlen(username, 32);
    if (len == 0 || len >= 32) {
        return false;
    }
    
    for (size_t i = 0; i < len; i++) {
        char c = username[i];
        if (!isalnum(c) && c != '_' && c != '-') {
            return false;
        }
    }
    
    return true;
}

// 研究3: 安全的文件操作
bool safe_file_read(const char* filename, SafeBuffer* buffer) {
    if (!filename || !buffer) return false;
    
    // 验证文件路径
    if (!validate_file_path(filename)) {
        return false;
    }
    
    // 安全打开文件
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        return false;
    }
    
    // 获取文件大小
    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return false;
    }
    
    if (st.st_size >= buffer->capacity) {
        close(fd);
        return false; // 文件太大
    }
    
    // 读取文件内容
    ssize_t bytes_read = read(fd, buffer->data, buffer->capacity - 1);
    close(fd);
    
    if (bytes_read == -1) {
        return false;
    }
    
    buffer->data[bytes_read] = '\0';
    buffer->length = bytes_read;
    return true;
}

bool safe_file_write(const char* filename, const char* content, mode_t mode) {
    if (!filename || !content) return false;
    
    // 验证文件路径
    if (!validate_file_path(filename)) {
        return false;
    }
    
    // 安全打开文件
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd == -1) {
        return false;
    }
    
    size_t content_len = strnlen(content, MAX_BUFFER_SIZE);
    ssize_t bytes_written = write(fd, content, content_len);
    close(fd);
    
    return bytes_written == content_len;
}

// 研究4: 安全的内存分配与释放
void* safe_malloc(size_t size) {
    if (size == 0 || size > SIZE_MAX / 2) {
        return NULL;
    }
    
    void* ptr = malloc(size);
    if (!ptr) {
        return NULL;
    }
    
    // 清零内存
    memset(ptr, 0, size);
    return ptr;
}

void* safe_realloc(void* ptr, size_t new_size) {
    if (new_size == 0 || new_size > SIZE_MAX / 2) {
        return NULL;
    }
    
    void* new_ptr = realloc(ptr, new_size);
    return new_ptr;
}

// 安全的数组访问
bool safe_array_access(void* array, size_t array_size, size_t index) {
    if (!array || index >= array_size) {
        return false;
    }
    return true;
}

// 研究5: 格式化字符串安全
int safe_snprintf(char* buffer, size_t buffer_size, const char* format, ...) {
    if (!buffer || buffer_size == 0 || !format) {
        return -1;
    }
    
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, buffer_size, format, args);
    va_end(args);
    
    // 确保字符串以null结尾
    if (result >= 0 && (size_t)result < buffer_size) {
        buffer[result] = '\0';
    } else if (buffer_size > 0) {
        buffer[buffer_size - 1] = '\0';
    }
    
    return result;
}

// 研究6: 加密与哈希函数（简化版）
uint32_t simple_hash(const char* str) {
    if (!str) return 0;
    
    uint32_t hash = 5381;
    int c;
    
    while ((c = (unsigned char)*str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    return hash;
}

// 研究7: 安全的随机数生成
bool secure_random_bytes(unsigned char* buffer, size_t length) {
    if (!buffer || length == 0) return false;
    
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd == -1) {
        return false;
    }
    
    ssize_t bytes_read = read(fd, buffer, length);
    close(fd);
    
    return bytes_read == length;
}

// 研究8: 安全日志记录
void secure_log(const char* message) {
    if (!message) return;
    
    // 限制日志消息长度
    size_t msg_len = strnlen(message, 512);
    
    FILE* log_file = fopen("/tmp/security.log", "a");
    if (!log_file) return;
    
    fprintf(log_file, "[%ld] %s\n", time(NULL), message);
    fclose(log_file);
}

// 研究9: 缓冲区溢出防护
bool bounds_check_memcpy(void* dest, const void* src, size_t dest_size, size_t src_size) {
    if (!dest || !src) return false;
    if (src_size > dest_size) return false;
    
    memcpy(dest, src, src_size);
    return true;
}

// 研究10: 整数溢出检测
bool safe_add_int(int a, int b, int* result) {
    if (!result) return false;
    
    if ((b > 0 && a > INT_MAX - b) || (b < 0 && a < INT_MIN - b)) {
        return false; // 溢出
    }
    
    *result = a + b;
    return true;
}

bool safe_multiply_int(int a, int b, int* result) {
    if (!result) return false;
    
    if (a > 0) {
        if (b > 0 && a > INT_MAX / b) return false;
        if (b < 0 && a > INT_MIN / b) return false;
    } else {
        if (b > 0 && a < INT_MIN / b) return false;
        if (b < 0 && a < INT_MAX / b) return false;
    }
    
    *result = a * b;
    return true;
}

// 研究11: 目录遍历防护
bool safe_path_join(char* dest, size_t dest_size, const char* base, const char* path) {
    if (!dest || !base || !path) return false;
    
    // 检查基础路径
    if (!validate_file_path(base)) return false;
    
    // 检查相对路径
    if (strstr(path, "..") != NULL || strstr(path, "/") != NULL) {
        return false;
    }
    
    int result = snprintf(dest, dest_size, "%s/%s", base, path);
    return result > 0 && (size_t)result < dest_size;
}

// 研究12: 时间戳验证
bool validate_timestamp(time_t timestamp) {
    time_t now = time(NULL);
    
    // 检查时间戳是否在合理范围内
    if (timestamp < 0 || timestamp > now + 86400) { // 不超过24小时
        return false;
    }
    
    return true;
}

// 研究13: 权限检查
bool check_file_permissions(const char* filename) {
    if (!filename) return false;
    
    struct stat st;
    if (stat(filename, &st) == -1) {
        return false;
    }
    
    // 检查是否为常规文件
    if (!S_ISREG(st.st_mode)) {
        return false;
    }
    
    // 检查文件权限
    if (st.st_mode & S_IWOTH) {
        return false; // 其他用户可写
    }
    
    return true;
}

// 研究14: 安全的命令执行
bool safe_system_command(const char* command) {
    if (!command) return false;
    
    // 检查命令长度
    size_t len = strnlen(command, 1024);
    if (len == 0 || len >= 1024) {
        return false;
    }
    
    // 检查危险字符
    const char* dangerous_chars = ";&|`$()\n\r";
    for (size_t i = 0; i < len; i++) {
        if (strchr(dangerous_chars, command[i]) != NULL) {
            return false;
        }
    }
    
    return true;
}

// 研究15: 敏感数据清理
void secure_zero_memory(void* ptr, size_t size) {
    if (!ptr || size == 0) return;
    
    volatile unsigned char* p = ptr;
    while (size--) {
        *p++ = 0;
    }
}

// 测试函数
void test_security_functions() {
    printf("=== C语言安全编程深度研究 ===\n\n");
    
    // 测试安全字符串操作
    printf("[测试1] 安全字符串操作\n");
    SafeString* str = safe_string_create(100);
    if (str) {
        safe_string_copy(str, "Hello, World!");
        printf("安全字符串: %s\n", str->data);
        safe_string_destroy(str);
    }
    
    // 测试输入验证
    printf("\n[测试2] 输入验证\n");
    int num;
    if (validate_integer("123", &num)) {
        printf("验证整数: %d\n", num);
    }
    
    if (!validate_integer("abc", &num)) {
        printf("无效整数被拒绝\n");
    }
    
    // 测试文件操作
    printf("\n[测试3] 安全文件操作\n");
    SafeBuffer buffer;
    buffer.capacity = MAX_BUFFER_SIZE;
    
    const char* test_content = "This is a test file content.\n";
    if (safe_file_write("/tmp/test_secure.txt", test_content, 0600)) {
        printf("文件写入成功\n");
    }
    
    if (safe_file_read("/tmp/test_secure.txt", &buffer)) {
        printf("文件读取成功: %s", buffer.data);
    }
    
    // 测试整数溢出检测
    printf("\n[测试4] 整数溢出检测\n");
    int result;
    if (safe_add_int(INT_MAX, 1, &result)) {
        printf("加法成功: %d\n", result);
    } else {
        printf("检测到整数溢出\n");
    }
    
    if (safe_multiply_int(INT_MAX, 2, &result)) {
        printf("乘法成功: %d\n", result);
    } else {
        printf("检测到整数溢出\n");
    }
    
    // 测试内存安全
    printf("\n[测试5] 内存安全操作\n");
    int* arr = (int*)safe_malloc(10 * sizeof(int));
    if (arr) {
        for (int i = 0; i < 10; i++) {
            arr[i] = i;
        }
        
        // 测试边界检查
        if (safe_array_access(arr, 10, 5)) {
            printf("数组访问安全: arr[5] = %d\n", arr[5]);
        }
        
        // 测试越界检测
        if (!safe_array_access(arr, 10, 15)) {
            printf("越界访问被拒绝\n");
        }
        
        free(arr);
    }
    
    // 测试随机数生成
    printf("\n[测试6] 安全随机数生成\n");
    unsigned char random_bytes[16];
    if (secure_random_bytes(random_bytes, sizeof(random_bytes))) {
        printf("随机数生成成功: ");
        for (size_t i = 0; i < sizeof(random_bytes); i++) {
            printf("%02x", random_bytes[i]);
        }
        printf("\n");
    }
    
    // 清理测试文件
    unlink("/tmp/test_secure.txt");
    
    printf("\n=== 安全编程研究结论 ===\n");
    printf("1. 始终验证输入数据\n");
    printf("2. 使用边界检查防止缓冲区溢出\n");
    printf("3. 正确清理敏感数据\n");
    printf("4. 检测并处理整数溢出\n");
    printf("5. 验证文件路径和权限\n");
    printf("6. 使用安全的随机数源\n");
    printf("7. 实现安全的内存管理\n");
    printf("8. 记录安全事件\n");
}

int main() {
    test_security_functions();
    return 0;
}