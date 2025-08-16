/*
 * 现代C++错误处理模式深度研究
 * 探索异常安全、错误码、std::expected等错误处理策略
 */
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <optional>
#include <variant>
#include <functional>
#include <memory>
#include <string>
#include <fstream>
#include <vector>

// 研究1: RAII和异常安全
namespace raii_study {
    // 异常安全的资源管理
    class FileManager {
    private:
        std::unique_ptr<std::FILE, decltype(&std::fclose)> file;
        
    public:
        FileManager(const std::string& filename, const std::string& mode)
            : file(std::fopen(filename.c_str(), mode.c_str()), &std::fclose) {
            if (!file) {
                throw std::runtime_error("无法打开文件: " + filename);
            }
        }
        
        void write(const std::string& data) {
            if (std::fputs(data.c_str(), file.get()) == EOF) {
                throw std::runtime_error("写入文件失败");
            }
        }
        
        std::string read() {
            std::string result;
            char buffer[1024];
            while (std::fgets(buffer, sizeof(buffer), file.get())) {
                result += buffer;
            }
            return result;
        }
        
        // 自动析构时关闭文件
    };
    
    // 异常安全的容器操作
    template<typename T>
    class SafeVector {
    private:
        std::vector<T> data;
        
    public:
        void safe_push_back(const T& value) {
            try {
                data.push_back(value);
            } catch (...) {
                // 强异常安全保证：操作失败时状态不变
                throw;
            }
        }
        
        void safe_insert(size_t pos, const T& value) {
            if (pos > data.size()) {
                throw std::out_of_range("插入位置超出范围");
            }
            
            auto old_size = data.size();
            try {
                data.insert(data.begin() + pos, value);
            } catch (...) {
                // 确保容器状态一致
                data.resize(old_size);
                throw;
            }
        }
        
        const std::vector<T>& get_data() const { return data; }
        size_t size() const { return data.size(); }
    };
    
    void test_raii() {
        std::cout << "\n=== RAII和异常安全测试 ===\n";
        
        try {
            FileManager fm("test_raii.txt", "w");
            fm.write("Hello, RAII!\n");
            fm.write("异常安全编程\n");
            std::cout << "文件写入成功\n";
        } catch (const std::exception& e) {
            std::cout << "文件操作异常: " << e.what() << "\n";
        }
        
        SafeVector<int> safe_vec;
        try {
            safe_vec.safe_push_back(1);
            safe_vec.safe_push_back(2);
            safe_vec.safe_push_back(3);
            safe_vec.safe_insert(1, 10);
            
            std::cout << "安全容器操作成功，大小: " << safe_vec.size() << "\n";
        } catch (const std::exception& e) {
            std::cout << "容器操作异常: " << e.what() << "\n";
        }
    }
}

// 研究2: 错误码模式
namespace error_code_study {
    enum class FileError {
        Success = 0,
        FileNotFound,
        PermissionDenied,
        DiskFull,
        InvalidFormat
    };
    
    // 自定义错误类别
    class FileErrorCategory : public std::error_category {
    public:
        const char* name() const noexcept override {
            return "file_error";
        }
        
        std::string message(int ev) const override {
            switch (static_cast<FileError>(ev)) {
                case FileError::Success: return "成功";
                case FileError::FileNotFound: return "文件未找到";
                case FileError::PermissionDenied: return "权限被拒绝";
                case FileError::DiskFull: return "磁盘空间不足";
                case FileError::InvalidFormat: return "文件格式无效";
                default: return "未知错误";
            }
        }
    };
    
    const FileErrorCategory& file_error_category() {
        static FileErrorCategory instance;
        return instance;
    }
    
    std::error_code make_error_code(FileError e) {
        return {static_cast<int>(e), file_error_category()};
    }
    
    // 使用错误码的文件操作
    std::pair<std::string, std::error_code> read_file(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return {"", make_error_code(FileError::FileNotFound)};
        }
        
        std::string content;
        std::string line;
        while (std::getline(file, line)) {
            content += line + "\n";
        }
        
        return {content, make_error_code(FileError::Success)};
    }
    
    std::error_code write_file(const std::string& filename, const std::string& content) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return make_error_code(FileError::PermissionDenied);
        }
        
        file << content;
        if (file.fail()) {
            return make_error_code(FileError::DiskFull);
        }
        
        return make_error_code(FileError::Success);
    }
    
    void test_error_codes() {
        std::cout << "\n=== 错误码模式测试 ===\n";
        
        // 写入文件
        auto write_result = write_file("test_error.txt", "Hello, Error Codes!\n");
        if (write_result) {
            std::cout << "写入失败: " << write_result.message() << "\n";
        } else {
            std::cout << "文件写入成功\n";
        }
        
        // 读取文件
        auto [content, read_result] = read_file("test_error.txt");
        if (read_result) {
            std::cout << "读取失败: " << read_result.message() << "\n";
        } else {
            std::cout << "文件读取成功: " << content.substr(0, 20) << "...\n";
        }
        
        // 读取不存在的文件
        auto [_, error] = read_file("nonexistent.txt");
        if (error) {
            std::cout << "预期错误: " << error.message() << "\n";
        }
    }
}

// 研究3: Result类型模式 (类似Rust的Result)
namespace result_study {
    template<typename T, typename E>
    class Result {
    private:
        std::variant<T, E> data;
        
    public:
        Result(T value) : data(std::move(value)) {}
        Result(E error) : data(std::move(error)) {}
        
        bool is_ok() const {
            return std::holds_alternative<T>(data);
        }
        
        bool is_err() const {
            return std::holds_alternative<E>(data);
        }
        
        const T& unwrap() const {
            if (is_err()) {
                throw std::runtime_error("尝试unwrap错误的Result");
            }
            return std::get<T>(data);
        }
        
        T unwrap_or(T default_value) const {
            if (is_ok()) {
                return std::get<T>(data);
            }
            return default_value;
        }
        
        const E& error() const {
            if (is_ok()) {
                throw std::runtime_error("尝试获取成功Result的错误");
            }
            return std::get<E>(data);
        }
        
        template<typename F>
        auto map(F func) const -> Result<decltype(func(std::get<T>(data))), E> {
            if (is_ok()) {
                return Result<decltype(func(std::get<T>(data))), E>(func(std::get<T>(data)));
            }
            return Result<decltype(func(std::get<T>(data))), E>(std::get<E>(data));
        }
        
        template<typename F>
        auto and_then(F func) const -> decltype(func(std::get<T>(data))) {
            if (is_ok()) {
                return func(std::get<T>(data));
            }
            return decltype(func(std::get<T>(data)))(std::get<E>(data));
        }
    };
    
    using StringResult = Result<std::string, std::string>;
    using IntResult = Result<int, std::string>;
    
    StringResult divide_strings(const std::string& a, const std::string& b) {
        if (b.empty()) {
            return StringResult("除数不能为空");
        }
        
        try {
            double num_a = std::stod(a);
            double num_b = std::stod(b);
            
            if (num_b == 0.0) {
                return StringResult("除零错误");
            }
            
            return StringResult(std::to_string(num_a / num_b));
        } catch (const std::exception&) {
            return StringResult("数字格式错误");
        }
    }
    
    IntResult parse_int(const std::string& str) {
        try {
            int value = std::stoi(str);
            return IntResult(value);
        } catch (const std::exception&) {
            return IntResult("无法解析为整数: " + str);
        }
    }
    
    void test_result_pattern() {
        std::cout << "\n=== Result模式测试 ===\n";
        
        // 成功案例
        auto result1 = divide_strings("10", "2");
        if (result1.is_ok()) {
            std::cout << "除法结果: " << result1.unwrap() << "\n";
        }
        
        // 错误案例
        auto result2 = divide_strings("10", "0");
        if (result2.is_err()) {
            std::cout << "除法错误: " << result2.error() << "\n";
        }
        
        // 链式操作
        auto result3 = parse_int("42")
            .map([](int x) { return x * 2; })
            .map([](int x) { return std::to_string(x); });
        
        if (result3.is_ok()) {
            std::cout << "链式操作结果: " << result3.unwrap() << "\n";
        }
        
        // 使用unwrap_or提供默认值
        auto result4 = parse_int("invalid");
        int value = result4.unwrap_or(-1);
        std::cout << "解析失败，使用默认值: " << value << "\n";
    }
}

// 研究4: 异常规范和noexcept
namespace noexcept_study {
    // noexcept函数
    int safe_add(int a, int b) noexcept {
        // 保证不抛出异常
        return a + b;
    }
    
    // 条件noexcept
    template<typename T>
    void safe_swap(T& a, T& b) noexcept(std::is_nothrow_move_constructible_v<T> && 
                                        std::is_nothrow_move_assignable_v<T>) {
        T temp = std::move(a);
        a = std::move(b);
        b = std::move(temp);
    }
    
    // 检查函数是否为noexcept
    void test_noexcept_specifications() {
        std::cout << "\n=== noexcept规范测试 ===\n";
        
        std::cout << "safe_add是noexcept: " << std::boolalpha << noexcept(safe_add(1, 2)) << "\n";
        
        int x = 10, y = 20;
        std::cout << "交换前: x=" << x << ", y=" << y << "\n";
        safe_swap(x, y);
        std::cout << "交换后: x=" << x << ", y=" << y << "\n";
        
        std::cout << "int的safe_swap是noexcept: " << noexcept(safe_swap(x, y)) << "\n";
        
        // 检查标准库函数的noexcept属性
        std::vector<int> vec;
        std::cout << "vector::size()是noexcept: " << noexcept(vec.size()) << "\n";
        std::cout << "vector::at()是noexcept: " << noexcept(vec.at(0)) << "\n";
    }
}

// 研究5: 自定义异常层次
namespace custom_exceptions {
    // 基础异常类
    class BaseException : public std::exception {
    protected:
        std::string message;
        
    public:
        BaseException(const std::string& msg) : message(msg) {}
        
        const char* what() const noexcept override {
            return message.c_str();
        }
    };
    
    // 具体异常类
    class ValidationException : public BaseException {
    public:
        ValidationException(const std::string& field, const std::string& reason)
            : BaseException("验证失败 - " + field + ": " + reason) {}
    };
    
    class NetworkException : public BaseException {
    public:
        NetworkException(int error_code, const std::string& details)
            : BaseException("网络错误 [" + std::to_string(error_code) + "]: " + details) {}
    };
    
    class DatabaseException : public BaseException {
    public:
        DatabaseException(const std::string& query, const std::string& error)
            : BaseException("数据库错误 - 查询: " + query + ", 错误: " + error) {}
    };
    
    // 使用自定义异常的函数
    void validate_email(const std::string& email) {
        if (email.empty()) {
            throw ValidationException("email", "不能为空");
        }
        if (email.find('@') == std::string::npos) {
            throw ValidationException("email", "格式无效");
        }
    }
    
    void simulate_network_operation() {
        // 模拟网络错误
        throw NetworkException(404, "服务器未找到");
    }
    
    void test_custom_exceptions() {
        std::cout << "\n=== 自定义异常测试 ===\n";
        
        // 测试验证异常
        try {
            validate_email("");
        } catch (const ValidationException& e) {
            std::cout << "捕获验证异常: " << e.what() << "\n";
        }
        
        try {
            validate_email("invalid-email");
        } catch (const ValidationException& e) {
            std::cout << "捕获验证异常: " << e.what() << "\n";
        }
        
        // 测试网络异常
        try {
            simulate_network_operation();
        } catch (const NetworkException& e) {
            std::cout << "捕获网络异常: " << e.what() << "\n";
        }
        
        // 多层异常处理
        try {
            validate_email("valid@email.com");
            std::cout << "邮箱验证通过\n";
        } catch (const BaseException& e) {
            std::cout << "捕获基础异常: " << e.what() << "\n";
        } catch (const std::exception& e) {
            std::cout << "捕获标准异常: " << e.what() << "\n";
        }
    }
}

int main() {
    std::cout << "=== 现代C++错误处理模式深度研究 ===\n";
    
    raii_study::test_raii();
    error_code_study::test_error_codes();
    result_study::test_result_pattern();
    noexcept_study::test_noexcept_specifications();
    custom_exceptions::test_custom_exceptions();
    
    std::cout << "\n=== 研究结论 ===\n";
    std::cout << "1. RAII提供了自动的资源管理和异常安全\n";
    std::cout << "2. 错误码模式适合系统级编程和性能敏感场景\n";
    std::cout << "3. Result模式提供了函数式的错误处理方式\n";
    std::cout << "4. noexcept规范提高了代码的可预测性和性能\n";
    std::cout << "5. 自定义异常层次提供了结构化的错误信息\n";
    std::cout << "6. 选择合适的错误处理策略是现代C++的关键\n";
    
    return 0;
}
