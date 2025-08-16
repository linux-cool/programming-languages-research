/*
 * C++20特性深度研究
 * 探索C++20的革命性新特性：概念、协程、模块、范围等
 */
#include <iostream>
#include <vector>
#include <algorithm>
#include <ranges>
#include <concepts>
#include <coroutine>
#include <memory>
#include <string>
#include <format>
#include <span>
#include <bit>
#include <numbers>
#include <chrono>
#include <compare>

// 研究1: 概念 (Concepts) - 更强大的类型约束
namespace concepts_study {
    // 基本概念定义
    template<typename T>
    concept Numeric = std::integral<T> || std::floating_point<T>;
    
    template<typename T>
    concept Printable = requires(T t) {
        std::cout << t;
    };
    
    template<typename T>
    concept Container = requires(T t) {
        t.begin();
        t.end();
        t.size();
    };
    
    template<typename T>
    concept Addable = requires(T a, T b) {
        { a + b } -> std::same_as<T>;
    };
    
    // 使用概念的函数模板
    template<Numeric T>
    T multiply(T a, T b) {
        return a * b;
    }
    
    template<Printable T>
    void print_value(const T& value) {
        std::cout << "值: " << value << "\n";
    }
    
    template<Container C>
    void print_container(const C& container) {
        std::cout << "容器元素: ";
        for (const auto& item : container) {
            std::cout << item << " ";
        }
        std::cout << "(大小: " << container.size() << ")\n";
    }
    
    // 概念的组合
    template<typename T>
    concept NumericContainer = Container<T> && requires(T t) {
        requires Numeric<typename T::value_type>;
    };
    
    template<NumericContainer C>
    auto sum_container(const C& container) {
        typename C::value_type sum{};
        for (const auto& item : container) {
            sum += item;
        }
        return sum;
    }
    
    void test_concepts() {
        std::cout << "\n=== 概念测试 ===\n";
        
        // 数值类型测试
        std::cout << "multiply(5, 3) = " << multiply(5, 3) << "\n";
        std::cout << "multiply(2.5, 4.0) = " << multiply(2.5, 4.0) << "\n";
        
        // 可打印类型测试
        print_value(42);
        print_value(std::string("Hello"));
        print_value(3.14);
        
        // 容器测试
        std::vector<int> vec = {1, 2, 3, 4, 5};
        print_container(vec);
        
        // 数值容器测试
        std::cout << "容器求和: " << sum_container(vec) << "\n";
    }
}

// 研究2: 范围 (Ranges) - 函数式编程风格
namespace ranges_study {
    void test_ranges() {
        std::cout << "\n=== 范围测试 ===\n";
        
        std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        
        // 传统方式 vs 范围方式
        std::cout << "原始数据: ";
        for (int n : numbers) std::cout << n << " ";
        std::cout << "\n";
        
        // 过滤偶数，平方，取前3个
        auto result = numbers 
            | std::views::filter([](int n) { return n % 2 == 0; })
            | std::views::transform([](int n) { return n * n; })
            | std::views::take(3);
        
        std::cout << "偶数平方前3个: ";
        for (int n : result) {
            std::cout << n << " ";
        }
        std::cout << "\n";
        
        // 范围算法
        std::vector<std::string> words = {"hello", "world", "cpp", "ranges", "awesome"};
        
        // 查找长度大于4的单词
        auto long_words = words | std::views::filter([](const std::string& s) {
            return s.length() > 4;
        });
        
        std::cout << "长单词: ";
        for (const auto& word : long_words) {
            std::cout << word << " ";
        }
        std::cout << "\n";
        
        // 范围排序
        std::vector<int> unsorted = {5, 2, 8, 1, 9, 3};
        std::ranges::sort(unsorted);
        std::cout << "排序后: ";
        std::ranges::copy(unsorted, std::ostream_iterator<int>(std::cout, " "));
        std::cout << "\n";
        
        // 范围查找
        auto it = std::ranges::find(unsorted, 8);
        if (it != unsorted.end()) {
            std::cout << "找到元素8在位置: " << std::distance(unsorted.begin(), it) << "\n";
        }
    }
}

// 研究3: 协程 (Coroutines) - 异步编程新范式
namespace coroutines_study {
    // 简单的生成器协程
    struct Generator {
        struct promise_type {
            int current_value;
            
            Generator get_return_object() {
                return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
            }
            
            std::suspend_always initial_suspend() { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void unhandled_exception() {}
            
            std::suspend_always yield_value(int value) {
                current_value = value;
                return {};
            }
            
            void return_void() {}
        };
        
        std::coroutine_handle<promise_type> h;
        
        Generator(std::coroutine_handle<promise_type> handle) : h(handle) {}
        
        ~Generator() {
            if (h) h.destroy();
        }
        
        // 移动构造和赋值
        Generator(Generator&& other) noexcept : h(other.h) {
            other.h = {};
        }
        
        Generator& operator=(Generator&& other) noexcept {
            if (this != &other) {
                if (h) h.destroy();
                h = other.h;
                other.h = {};
            }
            return *this;
        }
        
        // 禁用拷贝
        Generator(const Generator&) = delete;
        Generator& operator=(const Generator&) = delete;
        
        bool next() {
            h.resume();
            return !h.done();
        }
        
        int value() {
            return h.promise().current_value;
        }
    };
    
    Generator fibonacci_generator(int count) {
        int a = 0, b = 1;
        for (int i = 0; i < count; ++i) {
            co_yield a;
            int temp = a + b;
            a = b;
            b = temp;
        }
    }
    
    Generator range_generator(int start, int end) {
        for (int i = start; i < end; ++i) {
            co_yield i;
        }
    }
    
    void test_coroutines() {
        std::cout << "\n=== 协程测试 ===\n";
        
        // 斐波那契生成器
        std::cout << "斐波那契数列前10项: ";
        auto fib_gen = fibonacci_generator(10);
        while (fib_gen.next()) {
            std::cout << fib_gen.value() << " ";
        }
        std::cout << "\n";
        
        // 范围生成器
        std::cout << "范围[5, 10): ";
        auto range_gen = range_generator(5, 10);
        while (range_gen.next()) {
            std::cout << range_gen.value() << " ";
        }
        std::cout << "\n";
    }
}

// 研究4: std::format - 类型安全的格式化
namespace format_study {
    void test_format() {
        std::cout << "\n=== 格式化测试 ===\n";
        
        int age = 25;
        std::string name = "Alice";
        double score = 95.5;
        
        // 基本格式化
        auto msg1 = std::format("姓名: {}, 年龄: {}, 分数: {:.1f}", name, age, score);
        std::cout << msg1 << "\n";
        
        // 位置参数
        auto msg2 = std::format("分数: {2:.2f}, 姓名: {0}, 年龄: {1}", name, age, score);
        std::cout << msg2 << "\n";
        
        // 数字格式化
        int num = 42;
        std::cout << std::format("十进制: {}, 十六进制: {:x}, 八进制: {:o}, 二进制: {:b}\n", 
                                 num, num, num, num);
        
        // 浮点数格式化
        double pi = 3.14159265359;
        std::cout << std::format("π = {:.2f}, {:.5f}, {:e}\n", pi, pi, pi);
        
        // 对齐和填充
        std::cout << std::format("左对齐: '{:<10}', 右对齐: '{:>10}', 居中: '{:^10}'\n", 
                                 "test", "test", "test");
        
        std::cout << std::format("填充字符: '{:*<10}', '{:*>10}', '{:*^10}'\n", 
                                 "test", "test", "test");
    }
}

// 研究5: std::span - 连续内存的视图
namespace span_study {
    void process_data(std::span<int> data) {
        std::cout << "处理数据 (大小: " << data.size() << "): ";
        for (int value : data) {
            std::cout << value << " ";
        }
        std::cout << "\n";
    }
    
    void test_span() {
        std::cout << "\n=== span测试 ===\n";
        
        // 从数组创建span
        int arr[] = {1, 2, 3, 4, 5};
        std::span<int> span1(arr);
        process_data(span1);
        
        // 从vector创建span
        std::vector<int> vec = {10, 20, 30, 40, 50};
        std::span<int> span2(vec);
        process_data(span2);
        
        // 子span
        auto sub_span = span2.subspan(1, 3);  // 从索引1开始，长度为3
        std::cout << "子span: ";
        for (int value : sub_span) {
            std::cout << value << " ";
        }
        std::cout << "\n";
        
        // 修改数据
        span2[0] = 100;
        std::cout << "修改后的vector第一个元素: " << vec[0] << "\n";
    }
}

// 研究6: 位操作库 (bit)
namespace bit_study {
    void test_bit_operations() {
        std::cout << "\n=== 位操作测试 ===\n";
        
        uint32_t value = 0b11010110;
        
        std::cout << std::format("原始值: {:08b} ({})\n", value, value);
        std::cout << "前导零个数: " << std::countl_zero(value) << "\n";
        std::cout << "前导一个数: " << std::countl_one(value) << "\n";
        std::cout << "尾随零个数: " << std::countr_zero(value) << "\n";
        std::cout << "尾随一个数: " << std::countr_one(value) << "\n";
        std::cout << "置位个数: " << std::popcount(value) << "\n";
        
        // 字节序操作
        uint32_t big_endian = 0x12345678;
        uint32_t little_endian = std::byteswap(big_endian);
        std::cout << std::format("大端: 0x{:08x}, 小端: 0x{:08x}\n", big_endian, little_endian);
        
        // 2的幂检查
        for (uint32_t i : {1, 2, 3, 4, 8, 15, 16}) {
            std::cout << std::format("{} 是2的幂: {}\n", i, std::has_single_bit(i));
        }
    }
}

// 研究7: 数学常数 (numbers)
namespace numbers_study {
    void test_math_constants() {
        std::cout << "\n=== 数学常数测试 ===\n";
        
        std::cout << std::format("π = {:.10f}\n", std::numbers::pi);
        std::cout << std::format("e = {:.10f}\n", std::numbers::e);
        std::cout << std::format("√2 = {:.10f}\n", std::numbers::sqrt2);
        std::cout << std::format("√3 = {:.10f}\n", std::numbers::sqrt3);
        std::cout << std::format("ln(2) = {:.10f}\n", std::numbers::ln2);
        std::cout << std::format("ln(10) = {:.10f}\n", std::numbers::ln10);
        std::cout << std::format("φ (黄金比例) = {:.10f}\n", std::numbers::phi);
        
        // 使用常数进行计算
        double radius = 5.0;
        double area = std::numbers::pi * radius * radius;
        double circumference = 2 * std::numbers::pi * radius;
        
        std::cout << std::format("半径{}的圆: 面积={:.2f}, 周长={:.2f}\n", 
                                 radius, area, circumference);
    }
}

// 研究8: 三路比较 (spaceship operator)
namespace comparison_study {
    struct Point {
        int x, y;
        
        // 三路比较运算符
        auto operator<=>(const Point& other) const = default;
        bool operator==(const Point& other) const = default;
    };
    
    void test_three_way_comparison() {
        std::cout << "\n=== 三路比较测试 ===\n";
        
        Point p1{1, 2};
        Point p2{1, 2};
        Point p3{2, 3};
        
        std::cout << "p1 == p2: " << (p1 == p2) << "\n";
        std::cout << "p1 != p3: " << (p1 != p3) << "\n";
        std::cout << "p1 < p3: " << (p1 < p3) << "\n";
        std::cout << "p3 > p1: " << (p3 > p1) << "\n";
        
        // 字符串比较
        std::string s1 = "apple";
        std::string s2 = "banana";
        
        auto result = s1 <=> s2;
        if (result < 0) {
            std::cout << "'" << s1 << "' < '" << s2 << "'\n";
        } else if (result > 0) {
            std::cout << "'" << s1 << "' > '" << s2 << "'\n";
        } else {
            std::cout << "'" << s1 << "' == '" << s2 << "'\n";
        }
    }
}

int main() {
    std::cout << "=== C++20特性深度研究 ===\n";
    
    concepts_study::test_concepts();
    ranges_study::test_ranges();
    coroutines_study::test_coroutines();
    format_study::test_format();
    span_study::test_span();
    bit_study::test_bit_operations();
    numbers_study::test_math_constants();
    comparison_study::test_three_way_comparison();
    
    std::cout << "\n=== 研究结论 ===\n";
    std::cout << "1. 概念提供了更清晰的模板约束机制\n";
    std::cout << "2. 范围库实现了函数式编程风格\n";
    std::cout << "3. 协程为异步编程提供了新的抽象\n";
    std::cout << "4. std::format提供了类型安全的格式化\n";
    std::cout << "5. std::span提供了高效的内存视图\n";
    std::cout << "6. 位操作库标准化了常用位运算\n";
    std::cout << "7. 数学常数库提供了精确的数学常量\n";
    std::cout << "8. 三路比较简化了比较运算符的实现\n";
    
    return 0;
}
