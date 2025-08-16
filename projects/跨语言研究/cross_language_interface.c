/*
 * 跨语言接口深度研究
 * C与Python/Rust/JavaScript的FFI集成与性能对比
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <dlfcn.h>

// 研究1: C-Python C扩展接口
#ifdef PYTHON_CEXT
#include <Python.h>

// C扩展模块示例
static PyObject* cext_add(PyObject* self, PyObject* args) {
    int a, b;
    if (!PyArg_ParseTuple(args, "ii", &a, &b)) {
        return NULL;
    }
    return PyLong_FromLong(a + b);
}

static PyObject* cext_array_sum(PyObject* self, PyObject* args) {
    PyObject* list_obj;
    if (!PyArg_ParseTuple(args, "O", &list_obj)) {
        return NULL;
    }
    
    Py_ssize_t len = PyList_Size(list_obj);
    long total = 0;
    
    for (Py_ssize_t i = 0; i < len; i++) {
        PyObject* item = PyList_GetItem(list_obj, i);
        total += PyLong_AsLong(item);
    }
    
    return PyLong_FromLong(total);
}

// 方法定义
static PyMethodDef CExtMethods[] = {
    {"add", cext_add, METH_VARARGS, "Add two integers"},
    {"array_sum", cext_array_sum, METH_VARARGS, "Sum array elements"},
    {NULL, NULL, 0, NULL}
};

// 模块定义
static struct PyModuleDef cextmodule = {
    PyModuleDef_HEAD_INIT,
    "cext",
    "C extension example module",
    -1,
    CExtMethods
};

// 模块初始化
PyMODINIT_FUNC PyInit_cext(void) {
    return PyModule_Create(&cextmodule);
}
#endif

// 研究2: C-Rust FFI接口
#ifdef RUST_FFI
// Rust FFI声明
extern double rust_matmul(const double* a, const double* b, double* c, int n);
extern int rust_string_len(const char* str);
extern void rust_callback_example(void (*callback)(int));

// 回调函数示例
void rust_callback_handler(int value) {
    printf("Rust called back with: %d\n", value);
}

#endif

// 研究3: C-JavaScript (Node.js) FFI
#ifdef NODE_FFI
#include <node_api.h>

// Node.js N-API实现
napi_value NodeAdd(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    
    int32_t a, b;
    napi_get_value_int32(env, args[0], &a);
    napi_get_value_int32(env, args[1], &b);
    
    napi_value result;
    napi_create_int32(env, a + b, &result);
    return result;
}

// 模块初始化
napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor desc = {"add", 0, NodeAdd, 0, 0, 0, napi_default, 0};
    napi_define_properties(env, exports, 1, &desc);
    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
#endif

// 研究4: C-C++混合编程
#ifdef __cplusplus
extern "C" {
#endif

// C接口函数
int cpp_vector_add(const int* a, const int* b, int* c, int n);
double cpp_matrix_determinant(const double* matrix, int n);
char* cpp_string_reverse(const char* str);

#ifdef __cplusplus
}
#endif

// 研究5: 动态库加载与调用
typedef struct {
    void* handle;
    char* error;
} DynamicLib;

DynamicLib* dlopen_safe(const char* filename) {
    DynamicLib* lib = malloc(sizeof(DynamicLib));
    if (!lib) return NULL;
    
    lib->handle = dlopen(filename, RTLD_LAZY);
    if (!lib->handle) {
        lib->error = strdup(dlerror());
        return lib;
    }
    
    lib->error = NULL;
    return lib;
}

void* dlsym_safe(DynamicLib* lib, const char* symbol) {
    if (!lib || !lib->handle) return NULL;
    
    dlerror(); // 清除之前的错误
    void* sym = dlsym(lib->handle, symbol);
    char* error = dlerror();
    
    if (error) {
        free(lib->error);
        lib->error = strdup(error);
        return NULL;
    }
    
    return sym;
}

void dlclose_safe(DynamicLib* lib) {
    if (lib) {
        if (lib->handle) {
            dlclose(lib->handle);
        }
        free(lib->error);
        free(lib);
    }
}

// 研究6: 性能基准测试函数
int* generate_array(int size) {
    int* array = malloc(size * sizeof(int));
    if (!array) return NULL;
    
    for (int i = 0; i < size; i++) {
        array[i] = rand() % 1000;
    }
    return array;
}

long long c_array_sum(const int* array, int size) {
    long long sum = 0;
    for (int i = 0; i < size; i++) {
        sum += array[i];
    }
    return sum;
}

void c_array_multiply(const int* a, const int* b, int* c, int size) {
    for (int i = 0; i < size; i++) {
        c[i] = a[i] * b[i];
    }
}

// 研究7: 内存管理接口
void* c_malloc(size_t size) {
    return malloc(size);
}

void c_free(void* ptr) {
    free(ptr);
}

void* c_realloc(void* ptr, size_t size) {
    return realloc(ptr, size);
}

// 研究8: 字符串处理接口
char* c_string_concat(const char* str1, const char* str2) {
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);
    char* result = malloc(len1 + len2 + 1);
    
    if (result) {
        strcpy(result, str1);
        strcat(result, str2);
    }
    return result;
}

int c_string_length(const char* str) {
    return strlen(str);
}

// 研究9: 错误处理接口
const char* c_get_last_error() {
    static char error_buffer[256];
    return error_buffer;
}

void c_set_error(const char* message) {
    // 实际实现中会设置全局错误状态
}

// 研究10: 性能测试框架
typedef struct {
    const char* name;
    double time_ms;
    long long result;
} BenchmarkResult;

double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

BenchmarkResult benchmark_function(const char* name, 
                                 long long (*func)(const int*, int), 
                                 const int* array, int size) {
    BenchmarkResult result;
    result.name = name;
    
    double start = get_time_ms();
    result.result = func(array, size);
    double end = get_time_ms();
    
    result.time_ms = end - start;
    return result;
}

// 研究11: 线程安全接口
#ifdef _REENTRANT
#include <pthread.h>

static pthread_mutex_t ffi_mutex = PTHREAD_MUTEX_INITIALIZER;

void c_lock() {
    pthread_mutex_lock(&ffi_mutex);
}

void c_unlock() {
    pthread_mutex_unlock(&ffi_mutex);
}

int c_trylock() {
    return pthread_mutex_trylock(&ffi_mutex);
}
#endif

// 研究12: 平台特定的优化
#ifdef __x86_64__
#include <immintrin.h>

void c_simd_add(const int* a, const int* b, int* c, int size) {
    int i = 0;
    for (; i <= size - 8; i += 8) {
        __m256i va = _mm256_loadu_si256((__m256i const*)&a[i]);
        __m256i vb = _mm256_loadu_si256((__m256i const*)&b[i]);
        __m256i vc = _mm256_add_epi32(va, vb);
        _mm256_storeu_si256((__m256i*)&c[i], vc);
    }
    
    for (; i < size; i++) {
        c[i] = a[i] + b[i];
    }
}
#endif

// 主测试函数
int main() {
    printf("=== 跨语言接口深度研究 ===\n\n");
    
    printf("支持的跨语言接口:\n");
    printf("1. C-Python C扩展接口\n");
    printf("2. C-Rust FFI接口\n");
    printf("3. C-JavaScript (Node.js) N-API\n");
    printf("4. C-C++混合编程\n");
    printf("5. 动态库加载与调用\n");
    printf("6. 内存管理接口\n");
    printf("7. 字符串处理接口\n");
    printf("8. 错误处理接口\n");
    printf("9. 线程安全接口\n");
    printf("10. 平台特定优化\n\n");
    
    // 性能基准测试
    const int ARRAY_SIZE = 1000000;
    int* array = generate_array(ARRAY_SIZE);
    
    if (!array) {
        printf("内存分配失败\n");
        return 1;
    }
    
    printf("性能基准测试 (数组大小: %d):\n", ARRAY_SIZE);
    printf("================================\n");
    
    BenchmarkResult result = benchmark_function("C数组求和", c_array_sum, array, ARRAY_SIZE);
    printf("%-20s: %8.3f ms, 结果: %lld\n", result.name, result.time_ms, result.result);
    
    // SIMD优化测试
    int* a = generate_array(ARRAY_SIZE);
    int* b = generate_array(ARRAY_SIZE);
    int* c = malloc(ARRAY_SIZE * sizeof(int));
    
    if (a && b && c) {
        double start = get_time_ms();
        c_simd_add(a, b, c, ARRAY_SIZE);
        double end = get_time_ms();
        printf("%-20s: %8.3f ms\n", "SIMD加法优化", end - start);
        
        free(a);
        free(b);
        free(c);
    }
    
    free(array);
    
    printf("\n=== 跨语言接口使用建议 ===\n");
    printf("1. 选择合适的FFI技术: C扩展 vs Cython vs ctypes\n");
    printf("2. 注意内存管理: 谁分配谁释放\n");
    printf("3. 处理类型转换和边界检查\n");
    printf("4. 提供清晰的错误处理机制\n");
    printf("5. 考虑线程安全问题\n");
    printf("6. 使用性能分析工具优化热点代码\n");
    printf("7. 维护向后兼容性\n");
    printf("8. 提供完整的文档和示例\n");
    
    return 0;
}