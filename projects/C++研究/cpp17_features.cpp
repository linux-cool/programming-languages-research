/*
 * C++17特性深度研究
 * 探索C++17的新特性和现代编程模式
 */
#include <iostream>
#include <optional>
#include <variant>
#include <any>
#include <string_view>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <map>
#include <tuple>
#include <memory>
#include <execution>
#include <numeric>

namespace fs = std::filesystem;

// 研究1: 结构化绑定 (Structured Bindings)
namespace structured_bindings {
    std::tuple<int, std::string, double> get_student_info() {
        return {123, "Alice", 95.5};
    }
    
    std::map<std::string, int> get_scores() {
        return {{"Math", 95}, {"Physics", 88}, {"Chemistry", 92}};
    }
    
    void test_structured_bindings() {
        std::cout << "\n=== 结构化绑定测试 ===\n";
        
        // 从tuple解构
        auto [id, name, score] = get_student_info();
        std::cout << "学生信息: ID=" << id << ", 姓名=" << name << ", 分数=" << score << "\n";
        
        // 从pair解构
        std::pair<std::string, int> subject_score{"English", 90};
        auto [subject, points] = subject_score;
        std::cout << "科目: " << subject << ", 分数: " << points << "\n";
        
        // 从map迭代器解构
        auto scores = get_scores();
        for (const auto& [subj, pts] : scores) {
            std::cout << subj << ": " << pts << " ";
        }
        std::cout << "\n";
        
        // 从数组解构
        int arr[] = {1, 2, 3};
        auto [a, b, c] = arr;
        std::cout << "数组元素: " << a << ", " << b << ", " << c << "\n";
    }
}

// 研究2: std::optional - 可选值
namespace optional_study {
    std::optional<int> safe_divide(int a, int b) {
        if (b == 0) {
            return std::nullopt;
        }
        return a / b;
    }
    
    std::optional<std::string> find_user(int id) {
        static std::map<int, std::string> users = {
            {1, "Alice"}, {2, "Bob"}, {3, "Charlie"}
        };
        
        auto it = users.find(id);
        if (it != users.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    void test_optional() {
        std::cout << "\n=== std::optional测试 ===\n";
        
        // 安全除法
        auto result1 = safe_divide(10, 2);
        if (result1) {
            std::cout << "10 / 2 = " << *result1 << "\n";
        }
        
        auto result2 = safe_divide(10, 0);
        if (!result2) {
            std::cout << "除零错误，返回空值\n";
        }
        
        // 使用value_or提供默认值
        std::cout << "除零结果(默认-1): " << result2.value_or(-1) << "\n";
        
        // 查找用户
        auto user1 = find_user(1);
        auto user2 = find_user(999);
        
        std::cout << "用户1: " << user1.value_or("未找到") << "\n";
        std::cout << "用户999: " << user2.value_or("未找到") << "\n";
        
        // 链式操作
        auto transformed = find_user(2).transform([](const std::string& name) {
            return "Hello, " + name + "!";
        });
        
        if (transformed) {
            std::cout << "转换结果: " << *transformed << "\n";
        }
    }
}

// 研究3: std::variant - 类型安全的联合体
namespace variant_study {
    using Value = std::variant<int, double, std::string>;
    
    struct ValueVisitor {
        void operator()(int i) const {
            std::cout << "整数: " << i << "\n";
        }
        
        void operator()(double d) const {
            std::cout << "浮点数: " << d << "\n";
        }
        
        void operator()(const std::string& s) const {
            std::cout << "字符串: " << s << "\n";
        }
    };
    
    void test_variant() {
        std::cout << "\n=== std::variant测试 ===\n";
        
        std::vector<Value> values = {
            42,
            3.14,
            std::string("Hello"),
            100,
            2.71
        };
        
        for (const auto& value : values) {
            // 使用visitor模式
            std::visit(ValueVisitor{}, value);
            
            // 使用lambda visitor
            std::visit([](const auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, int>) {
                    std::cout << "  -> 这是一个整数\n";
                } else if constexpr (std::is_same_v<T, double>) {
                    std::cout << "  -> 这是一个浮点数\n";
                } else if constexpr (std::is_same_v<T, std::string>) {
                    std::cout << "  -> 这是一个字符串\n";
                }
            }, value);
        }
        
        // 检查当前类型
        Value v = std::string("test");
        if (std::holds_alternative<std::string>(v)) {
            std::cout << "v包含字符串: " << std::get<std::string>(v) << "\n";
        }
    }
}

// 研究4: std::string_view - 字符串视图
namespace string_view_study {
    void process_string(std::string_view sv) {
        std::cout << "处理字符串: '" << sv << "' (长度: " << sv.length() << ")\n";
    }
    
    std::string_view extract_filename(std::string_view path) {
        auto pos = path.find_last_of('/');
        if (pos != std::string_view::npos) {
            return path.substr(pos + 1);
        }
        return path;
    }
    
    void test_string_view() {
        std::cout << "\n=== std::string_view测试 ===\n";
        
        // 从不同来源创建string_view
        const char* cstr = "C字符串";
        std::string str = "std::string";
        
        process_string(cstr);
        process_string(str);
        process_string("字面量字符串");
        
        // 字符串操作
        std::string_view path = "/home/user/document.txt";
        auto filename = extract_filename(path);
        std::cout << "文件名: " << filename << "\n";
        
        // 子字符串操作
        std::string_view text = "Hello, World!";
        auto hello = text.substr(0, 5);
        auto world = text.substr(7, 5);
        std::cout << "子字符串: '" << hello << "' 和 '" << world << "'\n";
        
        // 查找操作
        if (text.find("World") != std::string_view::npos) {
            std::cout << "找到了'World'\n";
        }
    }
}

// 研究5: 折叠表达式 (Fold Expressions)
namespace fold_expressions {
    // 求和
    template<typename... Args>
    auto sum(Args... args) {
        return (args + ...);
    }
    
    // 逻辑与
    template<typename... Args>
    bool all_true(Args... args) {
        return (args && ...);
    }
    
    // 打印所有参数
    template<typename... Args>
    void print_all(Args... args) {
        ((std::cout << args << " "), ...);
        std::cout << "\n";
    }
    
    // 检查所有参数是否相等
    template<typename T, typename... Args>
    bool all_equal(T first, Args... args) {
        return ((first == args) && ...);
    }
    
    void test_fold_expressions() {
        std::cout << "\n=== 折叠表达式测试 ===\n";
        
        std::cout << "sum(1, 2, 3, 4, 5) = " << sum(1, 2, 3, 4, 5) << "\n";
        std::cout << "all_true(true, true, false) = " << all_true(true, true, false) << "\n";
        std::cout << "all_true(true, true, true) = " << all_true(true, true, true) << "\n";
        
        std::cout << "打印所有参数: ";
        print_all(1, "hello", 3.14, 'x');
        
        std::cout << "all_equal(1, 1, 1) = " << all_equal(1, 1, 1) << "\n";
        std::cout << "all_equal(1, 1, 2) = " << all_equal(1, 1, 2) << "\n";
    }
}

// 研究6: if constexpr - 编译期条件
namespace constexpr_if {
    template<typename T>
    void process_type(T value) {
        if constexpr (std::is_integral_v<T>) {
            std::cout << "处理整数类型: " << value << "\n";
        } else if constexpr (std::is_floating_point_v<T>) {
            std::cout << "处理浮点类型: " << value << "\n";
        } else if constexpr (std::is_same_v<T, std::string>) {
            std::cout << "处理字符串类型: " << value << "\n";
        } else {
            std::cout << "处理其他类型\n";
        }
    }
    
    template<typename Container>
    void print_container(const Container& container) {
        if constexpr (std::is_same_v<Container, std::string>) {
            std::cout << "字符串内容: " << container << "\n";
        } else {
            std::cout << "容器元素: ";
            for (const auto& item : container) {
                std::cout << item << " ";
            }
            std::cout << "\n";
        }
    }
    
    void test_constexpr_if() {
        std::cout << "\n=== if constexpr测试 ===\n";
        
        process_type(42);
        process_type(3.14);
        process_type(std::string("hello"));
        process_type('c');
        
        std::vector<int> vec = {1, 2, 3, 4, 5};
        std::string str = "Hello";
        
        print_container(vec);
        print_container(str);
    }
}

// 研究7: 类模板参数推导 (CTAD)
namespace ctad_study {
    template<typename T>
    class Container {
    public:
        Container(std::initializer_list<T> list) : data(list) {}
        
        void print() const {
            for (const auto& item : data) {
                std::cout << item << " ";
            }
            std::cout << "\n";
        }
        
    private:
        std::vector<T> data;
    };
    
    void test_ctad() {
        std::cout << "\n=== 类模板参数推导测试 ===\n";
        
        // C++17之前需要显式指定类型
        // Container<int> c1{1, 2, 3, 4, 5};
        
        // C++17可以自动推导
        Container c1{1, 2, 3, 4, 5};  // 推导为Container<int>
        Container c2{1.1, 2.2, 3.3};  // 推导为Container<double>
        Container c3{"hello", "world"}; // 推导为Container<const char*>
        
        std::cout << "整数容器: ";
        c1.print();
        
        std::cout << "浮点容器: ";
        c2.print();
        
        std::cout << "字符串容器: ";
        c3.print();
        
        // 标准库的CTAD
        std::vector v1{1, 2, 3, 4, 5};  // std::vector<int>
        std::pair p1{42, "hello"};      // std::pair<int, const char*>
        
        std::cout << "推导的vector大小: " << v1.size() << "\n";
        std::cout << "推导的pair: (" << p1.first << ", " << p1.second << ")\n";
    }
}

// 研究8: 并行算法
namespace parallel_algorithms {
    void test_parallel_algorithms() {
        std::cout << "\n=== 并行算法测试 ===\n";
        
        std::vector<int> data(1000000);
        std::iota(data.begin(), data.end(), 1);
        
        // 并行求和
        auto start = std::chrono::high_resolution_clock::now();
        auto sum_seq = std::reduce(std::execution::seq, data.begin(), data.end());
        auto end = std::chrono::high_resolution_clock::now();
        auto seq_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        start = std::chrono::high_resolution_clock::now();
        auto sum_par = std::reduce(std::execution::par, data.begin(), data.end());
        end = std::chrono::high_resolution_clock::now();
        auto par_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "顺序求和结果: " << sum_seq << " (时间: " << seq_time.count() << "μs)\n";
        std::cout << "并行求和结果: " << sum_par << " (时间: " << par_time.count() << "μs)\n";
        
        // 并行排序
        std::vector<int> data_copy = data;
        std::random_shuffle(data_copy.begin(), data_copy.end());
        
        start = std::chrono::high_resolution_clock::now();
        std::sort(std::execution::par, data_copy.begin(), data_copy.end());
        end = std::chrono::high_resolution_clock::now();
        auto sort_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "并行排序时间: " << sort_time.count() << "μs\n";
        std::cout << "排序验证: " << (std::is_sorted(data_copy.begin(), data_copy.end()) ? "成功" : "失败") << "\n";
    }
}

int main() {
    std::cout << "=== C++17特性深度研究 ===\n";
    
    structured_bindings::test_structured_bindings();
    optional_study::test_optional();
    variant_study::test_variant();
    string_view_study::test_string_view();
    fold_expressions::test_fold_expressions();
    constexpr_if::test_constexpr_if();
    ctad_study::test_ctad();
    parallel_algorithms::test_parallel_algorithms();
    
    std::cout << "\n=== 研究结论 ===\n";
    std::cout << "1. 结构化绑定简化了复杂数据结构的解构\n";
    std::cout << "2. std::optional提供了类型安全的可选值处理\n";
    std::cout << "3. std::variant是类型安全的联合体替代方案\n";
    std::cout << "4. std::string_view避免了不必要的字符串拷贝\n";
    std::cout << "5. 折叠表达式简化了变参模板编程\n";
    std::cout << "6. if constexpr实现了真正的编译期条件\n";
    std::cout << "7. CTAD减少了模板参数的显式指定\n";
    std::cout << "8. 并行算法充分利用了多核处理器性能\n";
    
    return 0;
}
