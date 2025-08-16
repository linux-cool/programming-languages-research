/*
 * C++模板元编程深度研究
 * 编译期计算与类型系统操作
 */
#include <iostream>
#include <type_traits>
#include <vector>
#include <array>
#include <chrono>

// 研究1: 编译期数学计算 - 斐波那契数列
namespace compile_time_math {
    // 递归模板计算斐波那契数
    template<int N>
    struct Fibonacci {
        static constexpr long long value = Fibonacci<N-1>::value + Fibonacci<N-2>::value;
    };
    
    // 模板特化终止递归
    template<>
    struct Fibonacci<0> {
        static constexpr long long value = 0;
    };
    
    template<>
    struct Fibonacci<1> {
        static constexpr long long value = 1;
    };
    
    // 编译期阶乘计算
    template<int N>
    struct Factorial {
        static constexpr long long value = N * Factorial<N-1>::value;
    };
    
    template<>
    struct Factorial<0> {
        static constexpr long long value = 1;
    };
}

// 研究2: 类型特征萃取与SFINAE
namespace type_traits {
    // 检查类型是否有size()方法
    template<typename T, typename = void>
    struct has_size : std::false_type {};
    
    template<typename T>
    struct has_size<T, std::void_t<decltype(std::declval<T>().size())>> : std::true_type {};
    
    // 检查类型是否为迭代器
    template<typename T>
    struct is_iterator {
    private:
        template<typename U>
        static auto test(int) -> decltype(
            ++std::declval<U&>(),
            *std::declval<U>(),
            std::true_type{}
        );
        
        template<typename>
        static std::false_type test(...);
    public:
        static constexpr bool value = decltype(test<T>(0))::value;
    };
    
    // 编译期类型选择
    template<bool Condition, typename TrueType, typename FalseType>
    struct conditional {
        using type = TrueType;
    };
    
    template<typename TrueType, typename FalseType>
    struct conditional<false, TrueType, FalseType> {
        using type = FalseType;
    };
}

// 研究3: 策略模式与类型萃取
namespace policy_based_design {
    // 排序策略
    struct QuickSort {
        template<typename Iterator>
        static void sort(Iterator first, Iterator last) {
            std::cout << "Using QuickSort policy" << std::endl;
            // 实际实现会调用快速排序
        }
    };
    
    struct MergeSort {
        template<typename Iterator>
        static void sort(Iterator first, Iterator last) {
            std::cout << "Using MergeSort policy" << std::endl;
            // 实际实现会调用归并排序
        }
    };
    
    // 策略化的排序器
    template<typename T, typename SortPolicy = QuickSort>
    class Sorter {
    public:
        void sort(std::vector<T>& data) {
            SortPolicy::sort(data.begin(), data.end());
        }
    };
}

// 研究4: 编译期数据结构 - 类型列表
namespace type_lists {
    // 空类型列表
    struct empty_list {};
    
    // 类型列表节点
    template<typename Head, typename Tail = empty_list>
    struct type_list {
        using head = Head;
        using tail = Tail;
    };
    
    // 计算类型列表长度
    template<typename List>
    struct length;
    
    template<>
    struct length<empty_list> {
        static constexpr size_t value = 0;
    };
    
    template<typename Head, typename Tail>
    struct length<type_list<Head, Tail>> {
        static constexpr size_t value = 1 + length<Tail>::value;
    };
    
    // 获取类型列表中的第N个类型
    template<size_t N, typename List>
    struct get;
    
    template<typename Head, typename Tail>
    struct get<0, type_list<Head, Tail>> {
        using type = Head;
    };
    
    template<size_t N, typename Head, typename Tail>
    struct get<N, type_list<Head, Tail>> {
        using type = typename get<N-1, Tail>::type;
    };
}

// 研究5: 表达式模板 - 高性能数值计算
namespace expression_templates {
    template<typename Derived>
    class Expression {
    public:
        const Derived& derived() const {
            return static_cast<const Derived&>(*this);
        }
    };
    
    template<typename T>
    class Vector : public Expression<Vector<T>> {
    public:
        explicit Vector(size_t size) : data(size) {}
        
        Vector(const std::initializer_list<T>& init) : data(init) {}
        
        T operator[](size_t index) const { return data[index]; }
        T& operator[](size_t index) { return data[index]; }
        
        size_t size() const { return data.size(); }
        
    private:
        std::vector<T> data;
    };
    
    template<typename LHS, typename RHS, typename Op>
    class BinaryExpression : public Expression<BinaryExpression<LHS, RHS, Op>> {
    public:
        BinaryExpression(const LHS& lhs, const RHS& rhs, Op op)
            : lhs(lhs), rhs(rhs), op(op) {}
        
        auto operator[](size_t index) const -> decltype(op(lhs[index], rhs[index])) {
            return op(lhs[index], rhs[index]);
        }
        
        size_t size() const { return lhs.size(); }
        
    private:
        const LHS& lhs;
        const RHS& rhs;
        Op op;
    };
    
    // 加法操作
    struct AddOp {
        template<typename T, typename U>
        auto operator()(T a, U b) const -> decltype(a + b) {
            return a + b;
        }
    };
    
    // 重载+运算符
    template<typename LHS, typename RHS>
    auto operator+(const Expression<LHS>& lhs, const Expression<RHS>& rhs) {
        return BinaryExpression<LHS, RHS, AddOp>(
            lhs.derived(), rhs.derived(), AddOp{}
        );
    }
}

// 研究6: 变参模板与折叠表达式
namespace variadic_templates {
    // 编译期求和
    template<typename... Args>
    auto sum(Args... args) {
        return (args + ... + 0);
    }
    
    // 编译期打印参数包
    template<typename... Args>
    void print_all(Args... args) {
        (std::cout << ... << args) << std::endl;
    }
    
    // 类型安全的printf
    template<typename T, typename... Args>
    void safe_printf(const char* format, T value, Args... args) {
        while (*format) {
            if (*format == '%') {
                std::cout << value;
                safe_printf(format + 1, args...);
                return;
            } else {
                std::cout << *format;
            }
            ++format;
        }
    }
    
    template<>
    void safe_printf(const char* format) {
        while (*format) {
            std::cout << *format;
            ++format;
        }
    }
}

// 研究7: 概念(Concepts) - C++20特性
#if __cplusplus >= 202002L
namespace cpp20_concepts {
    // 定义概念
    template<typename T>
    concept Addable = requires(T a, T b) {
        { a + b } -> std::convertible_to<T>;
    };
    
    template<typename T>
    concept Printable = requires(T a) {
        { std::cout << a } -> std::same_as<std::ostream&>;
    };
    
    // 使用概念的模板函数
    template<Addable T>
    T add(T a, T b) {
        return a + b;
    }
    
    template<Printable T>
    void print(const T& value) {
        std::cout << value << std::endl;
    }
}
#endif

// 研究8: 编译期字符串处理
namespace compile_time_strings {
    template<size_t N>
    struct fixed_string {
        constexpr fixed_string(const char(&str)[N]) {
            for (size_t i = 0; i < N; ++i) {
                data[i] = str[i];
            }
        }
        
        char data[N];
        static constexpr size_t size = N - 1;
    };
    
    template<fixed_string Str>
    struct string_processor {
        static constexpr const char* value = Str.data;
    };
}

// 性能测试框架
class TemplatePerformanceTest {
public:
    static void testFibonacci() {
        auto start = std::chrono::high_resolution_clock::now();
        
        // 编译期计算
        constexpr auto fib10 = compile_time_math::Fibonacci<10>::value;
        constexpr auto fib20 = compile_time_math::Fibonacci<20>::value;
        constexpr auto fact5 = compile_time_math::Factorial<5>::value;
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        
        std::cout << "Fibonacci(10): " << fib10 << std::endl;
        std::cout << "Fibonacci(20): " << fib20 << std::endl;
        std::cout << "Factorial(5): " << fact5 << std::endl;
        std::cout << "Compile-time calculation time: " << duration.count() << " ns" << std::endl;
    }
    
    static void testTypeList() {
        using my_list = type_lists::type_list<int, double, char, float>;
        
        std::cout << "Type list length: " << type_lists::length<my_list>::value << std::endl;
        
        using third_type = type_lists::get<2, my_list>::type;
        std::cout << "Third type is char: " << std::is_same<third_type, char>::value << std::endl;
    }
    
    static void testExpressionTemplates() {
        using namespace expression_templates;
        
        Vector<double> v1 = {1.0, 2.0, 3.0, 4.0};
        Vector<double> v2 = {5.0, 6.0, 7.0, 8.0};
        
        // 表达式模板避免临时对象
        auto result = v1 + v2;
        
        std::cout << "Expression template result: ";
        for (size_t i = 0; i < result.size(); ++i) {
            std::cout << result[i] << " ";
        }
        std::cout << std::endl;
    }
};

// 主测试函数
int main() {
    std::cout << "=== C++模板元编程深度研究 ===" << std::endl;
    
    // 测试1: 编译期数学计算
    std::cout << "\n[测试1] 编译期数学计算:" << std::endl;
    TemplatePerformanceTest::testFibonacci();
    
    // 测试2: 类型特征萃取
    std::cout << "\n[测试2] 类型特征萃取:" << std::endl;
    std::cout << "std::vector has size(): " << type_traits::has_size<std::vector<int>>::value << std::endl;
    std::cout << "int* is iterator: " << type_traits::is_iterator<int*>::value << std::endl;
    
    // 测试3: 策略模式
    std::cout << "\n[测试3] 策略模式:" << std::endl;
    {
        using namespace policy_based_design;
        std::vector<int> data = {3, 1, 4, 1, 5, 9, 2, 6};
        
        Sorter<int, QuickSort> quick_sorter;
        quick_sorter.sort(data);
        
        Sorter<int, MergeSort> merge_sorter;
        merge_sorter.sort(data);
    }
    
    // 测试4: 类型列表操作
    std::cout << "\n[测试4] 类型列表操作:" << std::endl;
    TemplatePerformanceTest::testTypeList();
    
    // 测试5: 变参模板
    std::cout << "\n[测试5] 变参模板:" << std::endl;
    {
        using namespace variadic_templates;
        std::cout << "Sum: " << sum(1, 2, 3, 4, 5) << std::endl;
        print_all("Hello", " ", "World", "!", 123);
        safe_printf("Value: %, String: %", 42, "hello");
    }
    
    // 测试6: 表达式模板
    std::cout << "\n[测试6] 表达式模板:" << std::endl;
    TemplatePerformanceTest::testExpressionTemplates();
    
#if __cplusplus >= 202002L
    // 测试7: C++20概念
    std::cout << "\n[测试7] C++20概念:" << std::endl;
    {
        using namespace cpp20_concepts;
        std::cout << "Add result: " << add(5, 3) << std::endl;
        print("Hello from concepts!");
    }
#endif
    
    // 测试8: 编译期字符串
    std::cout << "\n[测试8] 编译期字符串处理:" << std::endl;
    {
        using namespace compile_time_strings;
        constexpr fixed_string str("Hello Template Metaprogramming!");
        std::cout << "Compile-time string: " << str.data << std::endl;
        std::cout << "String length: " << str.size << std::endl;
    }
    
    std::cout << "\n=== 研究结论 ===" << std::endl;
    std::cout << "1. 模板元编程实现零运行时开销" << std::endl;
    std::cout << "2. 编译期计算提升程序性能" << std::endl;
    std::cout << "3. 类型萃取提供强大的类型操作能力" << std::endl;
    std::cout << "4. 表达式模板优化数值计算性能" << std::endl;
    std::cout << "5. 变参模板提供灵活的接口设计" << std::endl;
    std::cout << "6. C++20概念提供类型约束机制" << std::endl;
    
    return 0;
}