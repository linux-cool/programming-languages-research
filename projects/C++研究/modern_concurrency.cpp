/*
 * 现代C++并发编程深度研究
 * 探索C++11/14/17/20的并发特性和设计模式
 */
#include <iostream>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <async>
#include <vector>
#include <queue>
#include <memory>
#include <functional>
#include <chrono>
#include <random>
#include <barrier>
#include <latch>
#include <semaphore>
#include <stop_token>

// 研究1: 线程安全的单例模式
namespace singleton_study {
    class ThreadSafeSingleton {
    private:
        static std::once_flag initialized;
        static std::unique_ptr<ThreadSafeSingleton> instance;
        
        ThreadSafeSingleton() = default;
        
    public:
        static ThreadSafeSingleton& getInstance() {
            std::call_once(initialized, []() {
                instance = std::make_unique<ThreadSafeSingleton>();
            });
            return *instance;
        }
        
        void doSomething() {
            std::cout << "单例实例执行操作 (线程ID: " 
                      << std::this_thread::get_id() << ")\n";
        }
        
        // 禁用拷贝和移动
        ThreadSafeSingleton(const ThreadSafeSingleton&) = delete;
        ThreadSafeSingleton& operator=(const ThreadSafeSingleton&) = delete;
    };
    
    std::once_flag ThreadSafeSingleton::initialized;
    std::unique_ptr<ThreadSafeSingleton> ThreadSafeSingleton::instance;
    
    void test_singleton() {
        std::cout << "\n=== 线程安全单例测试 ===\n";
        
        std::vector<std::thread> threads;
        
        for (int i = 0; i < 5; ++i) {
            threads.emplace_back([]() {
                auto& singleton = ThreadSafeSingleton::getInstance();
                singleton.doSomething();
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
    }
}

// 研究2: 生产者-消费者模式
namespace producer_consumer {
    template<typename T>
    class ThreadSafeQueue {
    private:
        mutable std::mutex mtx;
        std::queue<T> queue;
        std::condition_variable condition;
        
    public:
        void push(T item) {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push(std::move(item));
            condition.notify_one();
        }
        
        bool try_pop(T& item) {
            std::lock_guard<std::mutex> lock(mtx);
            if (queue.empty()) {
                return false;
            }
            item = std::move(queue.front());
            queue.pop();
            return true;
        }
        
        void wait_and_pop(T& item) {
            std::unique_lock<std::mutex> lock(mtx);
            condition.wait(lock, [this] { return !queue.empty(); });
            item = std::move(queue.front());
            queue.pop();
        }
        
        bool empty() const {
            std::lock_guard<std::mutex> lock(mtx);
            return queue.empty();
        }
        
        size_t size() const {
            std::lock_guard<std::mutex> lock(mtx);
            return queue.size();
        }
    };
    
    void test_producer_consumer() {
        std::cout << "\n=== 生产者-消费者测试 ===\n";
        
        ThreadSafeQueue<int> queue;
        std::atomic<bool> done{false};
        
        // 生产者线程
        std::thread producer([&queue, &done]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 100);
            
            for (int i = 0; i < 10; ++i) {
                int value = dis(gen);
                queue.push(value);
                std::cout << "生产: " << value << "\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            done = true;
        });
        
        // 消费者线程
        std::thread consumer([&queue, &done]() {
            int item;
            while (!done || !queue.empty()) {
                if (queue.try_pop(item)) {
                    std::cout << "消费: " << item << "\n";
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            }
        });
        
        producer.join();
        consumer.join();
    }
}

// 研究3: 读写锁模式
namespace reader_writer {
    class SharedData {
    private:
        mutable std::shared_mutex mtx;
        std::vector<int> data;
        
    public:
        SharedData() : data{1, 2, 3, 4, 5} {}
        
        // 读操作 - 共享锁
        std::vector<int> read() const {
            std::shared_lock<std::shared_mutex> lock(mtx);
            std::cout << "读取数据 (线程: " << std::this_thread::get_id() << ")\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return data;
        }
        
        // 写操作 - 独占锁
        void write(int value) {
            std::unique_lock<std::shared_mutex> lock(mtx);
            std::cout << "写入数据: " << value << " (线程: " << std::this_thread::get_id() << ")\n";
            data.push_back(value);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        size_t size() const {
            std::shared_lock<std::shared_mutex> lock(mtx);
            return data.size();
        }
    };
    
    void test_reader_writer() {
        std::cout << "\n=== 读写锁测试 ===\n";
        
        SharedData shared_data;
        std::vector<std::thread> threads;
        
        // 创建多个读线程
        for (int i = 0; i < 3; ++i) {
            threads.emplace_back([&shared_data, i]() {
                for (int j = 0; j < 2; ++j) {
                    auto data = shared_data.read();
                    std::cout << "读线程" << i << "读取到" << data.size() << "个元素\n";
                }
            });
        }
        
        // 创建写线程
        threads.emplace_back([&shared_data]() {
            for (int i = 10; i < 13; ++i) {
                shared_data.write(i);
            }
        });
        
        for (auto& t : threads) {
            t.join();
        }
        
        std::cout << "最终数据大小: " << shared_data.size() << "\n";
    }
}

// 研究4: 原子操作和无锁编程
namespace atomic_study {
    class LockFreeCounter {
    private:
        std::atomic<int> counter{0};
        
    public:
        void increment() {
            counter.fetch_add(1, std::memory_order_relaxed);
        }
        
        void decrement() {
            counter.fetch_sub(1, std::memory_order_relaxed);
        }
        
        int get() const {
            return counter.load(std::memory_order_relaxed);
        }
        
        // 比较并交换
        bool compare_and_set(int expected, int desired) {
            return counter.compare_exchange_weak(expected, desired, 
                                               std::memory_order_release,
                                               std::memory_order_relaxed);
        }
    };
    
    // 无锁栈
    template<typename T>
    class LockFreeStack {
    private:
        struct Node {
            T data;
            Node* next;
            Node(T data) : data(std::move(data)), next(nullptr) {}
        };
        
        std::atomic<Node*> head{nullptr};
        
    public:
        void push(T item) {
            Node* new_node = new Node(std::move(item));
            new_node->next = head.load();
            while (!head.compare_exchange_weak(new_node->next, new_node));
        }
        
        bool pop(T& result) {
            Node* old_head = head.load();
            while (old_head && !head.compare_exchange_weak(old_head, old_head->next));
            
            if (old_head) {
                result = std::move(old_head->data);
                delete old_head;
                return true;
            }
            return false;
        }
        
        ~LockFreeStack() {
            T dummy;
            while (pop(dummy));
        }
    };
    
    void test_atomic_operations() {
        std::cout << "\n=== 原子操作测试 ===\n";
        
        LockFreeCounter counter;
        std::vector<std::thread> threads;
        
        // 多线程递增
        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([&counter]() {
                for (int j = 0; j < 1000; ++j) {
                    counter.increment();
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        std::cout << "最终计数器值: " << counter.get() << "\n";
        
        // 测试无锁栈
        LockFreeStack<int> stack;
        
        // 推入数据
        for (int i = 1; i <= 5; ++i) {
            stack.push(i);
        }
        
        // 弹出数据
        int value;
        std::cout << "无锁栈弹出: ";
        while (stack.pop(value)) {
            std::cout << value << " ";
        }
        std::cout << "\n";
    }
}

// 研究5: 异步编程和Future/Promise
namespace async_study {
    // 异步计算斐波那契数
    int fibonacci(int n) {
        if (n <= 1) return n;
        return fibonacci(n - 1) + fibonacci(n - 2);
    }
    
    void test_async_programming() {
        std::cout << "\n=== 异步编程测试 ===\n";
        
        // 使用std::async
        auto future1 = std::async(std::launch::async, fibonacci, 35);
        auto future2 = std::async(std::launch::async, fibonacci, 36);
        auto future3 = std::async(std::launch::async, fibonacci, 37);
        
        std::cout << "异步计算斐波那契数...\n";
        
        // 获取结果
        std::cout << "fibonacci(35) = " << future1.get() << "\n";
        std::cout << "fibonacci(36) = " << future2.get() << "\n";
        std::cout << "fibonacci(37) = " << future3.get() << "\n";
        
        // 使用Promise/Future进行线程间通信
        std::promise<std::string> promise;
        std::future<std::string> future = promise.get_future();
        
        std::thread worker([&promise]() {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            promise.set_value("工作完成!");
        });
        
        std::cout << "等待工作线程完成...\n";
        std::cout << "结果: " << future.get() << "\n";
        
        worker.join();
    }
}

// 研究6: C++20同步原语
#if __cplusplus >= 202002L
namespace cpp20_sync {
    void test_barrier() {
        std::cout << "\n=== C++20 Barrier测试 ===\n";
        
        const int num_threads = 3;
        std::barrier sync_point(num_threads, []() noexcept {
            std::cout << "所有线程到达同步点!\n";
        });
        
        std::vector<std::thread> threads;
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&sync_point, i]() {
                std::cout << "线程" << i << "开始工作\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(100 * (i + 1)));
                std::cout << "线程" << i << "完成工作，等待其他线程\n";
                sync_point.arrive_and_wait();
                std::cout << "线程" << i << "继续执行\n";
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
    }
    
    void test_latch() {
        std::cout << "\n=== C++20 Latch测试 ===\n";
        
        const int num_workers = 3;
        std::latch work_done(num_workers);
        
        std::vector<std::thread> workers;
        
        for (int i = 0; i < num_workers; ++i) {
            workers.emplace_back([&work_done, i]() {
                std::cout << "工作线程" << i << "开始工作\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(200 * (i + 1)));
                std::cout << "工作线程" << i << "完成工作\n";
                work_done.count_down();
            });
        }
        
        std::thread coordinator([&work_done]() {
            std::cout << "协调线程等待所有工作完成...\n";
            work_done.wait();
            std::cout << "所有工作已完成，开始清理\n";
        });
        
        for (auto& w : workers) {
            w.join();
        }
        coordinator.join();
    }
    
    void test_semaphore() {
        std::cout << "\n=== C++20 Semaphore测试 ===\n";
        
        std::counting_semaphore<3> semaphore(2); // 最多2个资源
        std::vector<std::thread> threads;
        
        for (int i = 0; i < 5; ++i) {
            threads.emplace_back([&semaphore, i]() {
                std::cout << "线程" << i << "请求资源\n";
                semaphore.acquire();
                std::cout << "线程" << i << "获得资源，开始工作\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                std::cout << "线程" << i << "释放资源\n";
                semaphore.release();
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
    }
}
#endif

// 研究7: 线程池实现
namespace thread_pool {
    class SimpleThreadPool {
    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;
        std::mutex queue_mutex;
        std::condition_variable condition;
        bool stop;
        
    public:
        SimpleThreadPool(size_t threads) : stop(false) {
            for (size_t i = 0; i < threads; ++i) {
                workers.emplace_back([this] {
                    for (;;) {
                        std::function<void()> task;
                        
                        {
                            std::unique_lock<std::mutex> lock(this->queue_mutex);
                            this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                            
                            if (this->stop && this->tasks.empty()) {
                                return;
                            }
                            
                            task = std::move(this->tasks.front());
                            this->tasks.pop();
                        }
                        
                        task();
                    }
                });
            }
        }
        
        template<class F, class... Args>
        auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
            using return_type = typename std::result_of<F(Args...)>::type;
            
            auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );
            
            std::future<return_type> res = task->get_future();
            
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                
                if (stop) {
                    throw std::runtime_error("enqueue on stopped ThreadPool");
                }
                
                tasks.emplace([task]() { (*task)(); });
            }
            
            condition.notify_one();
            return res;
        }
        
        ~SimpleThreadPool() {
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                stop = true;
            }
            
            condition.notify_all();
            
            for (std::thread &worker : workers) {
                worker.join();
            }
        }
    };
    
    void test_thread_pool() {
        std::cout << "\n=== 线程池测试 ===\n";
        
        SimpleThreadPool pool(4);
        std::vector<std::future<int>> results;
        
        // 提交任务
        for (int i = 0; i < 8; ++i) {
            results.emplace_back(
                pool.enqueue([i] {
                    std::cout << "任务" << i << "在线程" << std::this_thread::get_id() << "执行\n";
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    return i * i;
                })
            );
        }
        
        // 获取结果
        for (auto&& result : results) {
            std::cout << "任务结果: " << result.get() << "\n";
        }
    }
}

int main() {
    std::cout << "=== 现代C++并发编程深度研究 ===\n";
    
    singleton_study::test_singleton();
    producer_consumer::test_producer_consumer();
    reader_writer::test_reader_writer();
    atomic_study::test_atomic_operations();
    async_study::test_async_programming();
    
#if __cplusplus >= 202002L
    cpp20_sync::test_barrier();
    cpp20_sync::test_latch();
    cpp20_sync::test_semaphore();
#endif
    
    thread_pool::test_thread_pool();
    
    std::cout << "\n=== 研究结论 ===\n";
    std::cout << "1. 现代C++提供了丰富的并发编程工具\n";
    std::cout << "2. 原子操作实现了高效的无锁编程\n";
    std::cout << "3. 异步编程简化了复杂的并发逻辑\n";
    std::cout << "4. C++20同步原语提供了更精确的控制\n";
    std::cout << "5. 线程池是管理并发任务的有效模式\n";
    std::cout << "6. 正确的同步机制是并发程序的关键\n";
    
    return 0;
}
