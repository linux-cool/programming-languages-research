/*
 * C++智能指针深度研究
 * 现代C++内存管理与RAII模式
 */
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <chrono>

// 研究1: unique_ptr的独占所有权语义
class Resource {
public:
    Resource(const std::string& name) : name(name) {
        std::cout << "Resource created: " << name << std::endl;
    }
    
    ~Resource() {
        std::cout << "Resource destroyed: " << name << std::endl;
    }
    
    void use() {
        std::cout << "Using resource: " << name << std::endl;
    }
    
private:
    std::string name;
};

// unique_ptr工厂函数
std::unique_ptr<Resource> createResource(const std::string& name) {
    return std::make_unique<Resource>(name);
}

// 研究2: shared_ptr的共享所有权和循环引用问题
class Node : public std::enable_shared_from_this<Node> {
public:
    Node(int value) : value(value) {
        std::cout << "Node created: " << value << std::endl;
    }
    
    ~Node() {
        std::cout << "Node destroyed: " << value << std::endl;
    }
    
    void setChild(std::shared_ptr<Node> child) {
        this->child = child;
    }
    
    void setParent(std::shared_ptr<Node> parent) {
        this->parent = parent;
    }
    
    void breakCycle() {
        parent.reset(); // 打破循环引用
    }
    
    int getValue() const { return value; }
    
private:
    int value;
    std::shared_ptr<Node> child;
    std::weak_ptr<Node> parent; // 使用weak_ptr避免循环引用
};

// 研究3: 自定义删除器
struct FileDeleter {
    void operator()(FILE* file) const {
        if (file) {
            fclose(file);
            std::cout << "File closed by custom deleter" << std::endl;
        }
    }
};

// 研究4: 智能指针的性能测试
class PerformanceTest {
public:
    static void testRawPointerPerformance() {
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<Resource*> resources;
        for (int i = 0; i < 10000; ++i) {
            resources.push_back(new Resource("raw_" + std::to_string(i)));
        }
        
        for (auto* res : resources) {
            res->use();
        }
        
        for (auto* res : resources) {
            delete res;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "Raw pointer time: " << duration.count() << " microseconds" << std::endl;
    }
    
    static void testUniquePtrPerformance() {
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::unique_ptr<Resource>> resources;
        for (int i = 0; i < 10000; ++i) {
            resources.push_back(std::make_unique<Resource>("unique_" + std::to_string(i)));
        }
        
        for (const auto& res : resources) {
            res->use();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "Unique_ptr time: " << duration.count() << " microseconds" << std::endl;
    }
};

// 研究5: 智能指针与异常安全
class ExceptionSafeRAII {
public:
    ExceptionSafeRAII() : resource(std::make_unique<Resource>("RAII_Resource")) {
        std::cout << "RAII wrapper created" << std::endl;
    }
    
    void riskyOperation() {
        throw std::runtime_error("Something went wrong");
    }
    
    ~ExceptionSafeRAII() {
        std::cout << "RAII wrapper destroyed - resource automatically cleaned up" << std::endl;
    }
    
private:
    std::unique_ptr<Resource> resource;
};

// 研究6: 智能指针数组管理
void testArrayManagement() {
    // 管理动态数组
    std::unique_ptr<int[]> intArray(new int[10]);
    for (int i = 0; i < 10; ++i) {
        intArray[i] = i * i;
    }
    
    std::cout << "Array elements: ";
    for (int i = 0; i < 10; ++i) {
        std::cout << intArray[i] << " ";
    }
    std::cout << std::endl;
}

// 主测试函数
int main() {
    std::cout << "=== C++智能指针深度研究 ===" << std::endl;
    
    // 测试1: unique_ptr的基本用法
    std::cout << "\n[测试1] unique_ptr基本用法:" << std::endl;
    {
        auto res1 = createResource("unique_demo");
        res1->use();
    }
    
    // 测试2: shared_ptr和循环引用
    std::cout << "\n[测试2] shared_ptr循环引用处理:" << std::endl;
    {
        auto parent = std::make_shared<Node>(1);
        auto child = std::make_shared<Node>(2);
        
        parent->setChild(child);
        child->setParent(parent); // 使用weak_ptr避免循环
        
        std::cout << "Parent use count: " << parent.use_count() << std::endl;
        std::cout << "Child use count: " << child.use_count() << std::endl;
    }
    
    // 测试3: 自定义删除器
    std::cout << "\n[测试3] 自定义删除器示例:" << std::endl;
    {
        std::unique_ptr<FILE, FileDeleter> file(fopen("test.txt", "w"));
        if (file) {
            fputs("Test content", file.get());
        }
    }
    
    // 测试4: 性能测试
    std::cout << "\n[测试4] 性能对比测试:" << std::endl;
    PerformanceTest::testRawPointerPerformance();
    PerformanceTest::testUniquePtrPerformance();
    
    // 测试5: 异常安全
    std::cout << "\n[测试5] 异常安全测试:" << std::endl;
    try {
        ExceptionSafeRAII raii;
        raii.riskyOperation();
    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << std::endl;
    }
    
    // 测试6: 数组管理
    std::cout << "\n[测试6] 数组管理测试:" << std::endl;
    testArrayManagement();
    
    std::cout << "\n=== 研究结论 ===" << std::endl;
    std::cout << "1. unique_ptr提供零开销抽象，性能接近原始指针" << std::endl;
    std::cout << "2. shared_ptr引用计数开销在共享场景中可接受" << std::endl;
    std::cout << "3. weak_ptr有效解决循环引用问题" << std::endl;
    std::cout << "4. 自定义删除器使智能指针适用于各种资源管理" << std::endl;
    std::cout << "5. RAII模式提供异常安全保障" << std::endl;
    
    return 0;
}