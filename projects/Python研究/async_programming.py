"""
Python异步编程深度研究
asyncio、异步IO与并发性能优化
"""
import asyncio
import aiohttp
import aiofiles
import time
import random
import json
import logging
from typing import List, Dict, Any, Optional, Callable, Awaitable
from dataclasses import dataclass
from pathlib import Path
import concurrent.futures
import threading

# 研究1: 基础异步编程模式
class BasicAsyncPatterns:
    """基础异步编程模式研究"""
    
    @staticmethod
    async def simple_coroutine(name: str, delay: float) -> str:
        """简单协程示例"""
        print(f"Starting {name}")
        await asyncio.sleep(delay)
        print(f"Finished {name}")
        return f"{name} completed after {delay}s"
    
    @staticmethod
    async def concurrent_tasks():
        """并发任务执行"""
        tasks = [
            BasicAsyncPatterns.simple_coroutine("Task1", 1.0),
            BasicAsyncPatterns.simple_coroutine("Task2", 2.0),
            BasicAsyncPatterns.simple_coroutine("Task3", 1.5)
        ]
        
        results = await asyncio.gather(*tasks)
        return results
    
    @staticmethod
    async def task_with_semaphore(sem: asyncio.Semaphore, task_id: int) -> str:
        """使用信号量限制并发"""
        async with sem:
            print(f"Task {task_id} started")
            await asyncio.sleep(random.uniform(0.5, 2.0))
            print(f"Task {task_id} completed")
            return f"Task {task_id} done"

# 研究2: 异步HTTP客户端
class AsyncHTTPClient:
    """异步HTTP客户端研究"""
    
    def __init__(self, max_concurrent: int = 10):
        self.semaphore = asyncio.Semaphore(max_concurrent)
        self.session: Optional[aiohttp.ClientSession] = None
    
    async def __aenter__(self):
        self.session = aiohttp.ClientSession()
        return self
    
    async def __aexit__(self, exc_type, exc_val, exc_tb):
        if self.session:
            await self.session.close()
    
    async def fetch_url(self, url: str) -> Dict[str, Any]:
        """获取单个URL"""
        async with self.semaphore:
            async with self.session.get(url) as response:
                data = await response.text()
                return {
                    'url': url,
                    'status': response.status,
                    'content_length': len(data),
                    'response_time': time.time()
                }
    
    async def fetch_multiple_urls(self, urls: List[str]) -> List[Dict[str, Any]]:
        """并发获取多个URL"""
        tasks = [self.fetch_url(url) for url in urls]
        results = await asyncio.gather(*tasks, return_exceptions=True)
        
        valid_results = []
        for result in results:
            if isinstance(result, Exception):
                print(f"Error fetching URL: {result}")
            else:
                valid_results.append(result)
        
        return valid_results

# 研究3: 异步文件IO
class AsyncFileIO:
    """异步文件IO操作研究"""
    
    @staticmethod
    async def read_large_file(file_path: str, chunk_size: int = 8192) -> str:
        """异步读取大文件"""
        content = ""
        async with aiofiles.open(file_path, 'r', encoding='utf-8') as file:
            while True:
                chunk = await file.read(chunk_size)
                if not chunk:
                    break
                content += chunk
        return content
    
    @staticmethod
    async def write_large_file(file_path: str, content: str, chunk_size: int = 8192) -> int:
        """异步写入大文件"""
        bytes_written = 0
        async with aiofiles.open(file_path, 'w', encoding='utf-8') as file:
            for i in range(0, len(content), chunk_size):
                chunk = content[i:i+chunk_size]
                await file.write(chunk)
                bytes_written += len(chunk)
        return bytes_written
    
    @staticmethod
    async def process_files_concurrently(file_paths: List[str], 
                                       processor: Callable[[str], Awaitable[str]]) -> List[str]:
        """并发处理多个文件"""
        tasks = [processor(file_path) for file_path in file_paths]
        results = await asyncio.gather(*tasks)
        return results

# 研究4: 异步生产者-消费者模式
class AsyncProducerConsumer:
    """异步生产者-消费者模式"""
    
    def __init__(self, max_size: int = 100):
        self.queue = asyncio.Queue(maxsize=max_size)
        self.producer_count = 0
        self.consumer_count = 0
        self.results = []
    
    async def producer(self, producer_id: int, items: List[int]):
        """生产者协程"""
        for item in items:
            await self.queue.put(item)
            print(f"Producer {producer_id} produced: {item}")
            self.producer_count += 1
            await asyncio.sleep(random.uniform(0.1, 0.5))
    
    async def consumer(self, consumer_id: int) -> List[int]:
        """消费者协程"""
        processed = []
        while True:
            try:
                item = await asyncio.wait_for(self.queue.get(), timeout=2.0)
                processed_item = item * 2
                processed.append(processed_item)
                self.consumer_count += 1
                print(f"Consumer {consumer_id} processed: {item} -> {processed_item}")
                self.queue.task_done()
            except asyncio.TimeoutError:
                break
        return processed
    
    async def run_producer_consumer(self, producer_data: List[List[int]], 
                                  num_consumers: int = 3) -> List[int]:
        """运行生产者-消费者模式"""
        producers = [self.producer(i, data) for i, data in enumerate(producer_data)]
        consumers = [self.consumer(i) for i in range(num_consumers)]
        
        await asyncio.gather(*producers)
        results = await asyncio.gather(*consumers)
        await self.queue.join()
        
        return [item for sublist in results for item in sublist]

# 研究5: 异步数据库操作模拟
@dataclass
class User:
    id: int
    name: str
    email: str

class AsyncDatabase:
    """模拟异步数据库操作"""
    
    def __init__(self):
        self.users = {}
        self.lock = asyncio.Lock()
    
    async def create_user(self, name: str, email: str) -> User:
        """创建用户"""
        async with self.lock:
            user_id = len(self.users) + 1
            user = User(id=user_id, name=name, email=email)
            self.users[user_id] = user
            await asyncio.sleep(0.1)  # 模拟数据库延迟
            return user
    
    async def get_user(self, user_id: int) -> Optional[User]:
        """获取用户"""
        await asyncio.sleep(0.05)  # 模拟数据库延迟
        return self.users.get(user_id)
    
    async def update_user(self, user_id: int, **kwargs) -> bool:
        """更新用户"""
        async with self.lock:
            if user_id not in self.users:
                return False
            user = self.users[user_id]
            for key, value in kwargs.items():
                if hasattr(user, key):
                    setattr(user, key, value)
            await asyncio.sleep(0.08)  # 模拟数据库延迟
            return True
    
    async def get_all_users(self) -> List[User]:
        """获取所有用户"""
        await asyncio.sleep(0.03)
        return list(self.users.values())

# 研究6: 异步任务调度器
class AsyncTaskScheduler:
    """异步任务调度器"""
    
    def __init__(self):
        self.tasks = []
        self.running = False
    
    def schedule_task(self, coro: Callable, *args, **kwargs):
        """调度任务"""
        task = {
            'coro': coro,
            'args': args,
            'kwargs': kwargs,
            'created_at': time.time()
        }
        self.tasks.append(task)
    
    async def run_scheduled_tasks(self):
        """运行所有调度任务"""
        self.running = True
        results = []
        
        for task in self.tasks:
            try:
                result = await task['coro'](*task['args'], **task['kwargs'])
                results.append(result)
            except Exception as e:
                print(f"Task failed: {e}")
                results.append(None)
        
        self.running = False
        return results

# 研究7: 异步性能基准测试
class AsyncPerformanceBenchmark:
    """异步性能基准测试"""
    
    @staticmethod
    async def cpu_bound_task(n: int) -> int:
        """CPU密集型任务"""
        return sum(i * i for i in range(n))
    
    @staticmethod
    async def io_bound_task(delay: float) -> str:
        """IO密集型任务"""
        await asyncio.sleep(delay)
        return f"Completed after {delay}s"
    
    @staticmethod
    async def benchmark_concurrent_vs_sequential():
        """并发 vs 顺序执行基准测试"""
        
        # 顺序执行
        start_time = time.time()
        for delay in [1.0, 1.5, 2.0, 0.5]:
            await AsyncPerformanceBenchmark.io_bound_task(delay)
        sequential_time = time.time() - start_time
        
        # 并发执行
        start_time = time.time()
        tasks = [AsyncPerformanceBenchmark.io_bound_task(delay) 
                for delay in [1.0, 1.5, 2.0, 0.5]]
        await asyncio.gather(*tasks)
        concurrent_time = time.time() - start_time
        
        return {
            'sequential_time': sequential_time,
            'concurrent_time': concurrent_time,
            'speedup': sequential_time / concurrent_time
        }
    
    @staticmethod
    async def benchmark_thread_pool_vs_async():
        """线程池 vs 异步基准测试"""
        
        def blocking_io(delay: float) -> str:
            """阻塞IO操作"""
            time.sleep(delay)
            return f"Blocking IO completed after {delay}s"
        
        # 线程池执行
        start_time = time.time()
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
            delays = [1.0, 1.5, 2.0, 0.5]
            futures = [executor.submit(blocking_io, delay) for delay in delays]
            thread_results = [f.result() for f in futures]
        thread_time = time.time() - start_time
        
        # 异步执行
        start_time = time.time()
        async_tasks = [AsyncPerformanceBenchmark.io_bound_task(delay) 
                      for delay in [1.0, 1.5, 2.0, 0.5]]
        async_results = await asyncio.gather(*async_tasks)
        async_time = time.time() - start_time
        
        return {
            'thread_time': thread_time,
            'async_time': async_time,
            'thread_results': thread_results,
            'async_results': async_results
        }

# 研究8: 异步上下文管理器
class AsyncContextManager:
    """异步上下文管理器研究"""
    
    def __init__(self, name: str):
        self.name = name
    
    async def __aenter__(self):
        print(f"Entering async context: {self.name}")
        await asyncio.sleep(0.1)
        return self
    
    async def __aexit__(self, exc_type, exc_val, exc_tb):
        print(f"Exiting async context: {self.name}")
        await asyncio.sleep(0.1)
        if exc_type:
            print(f"Exception occurred: {exc_type}")

# 研究9: 异步异常处理
class AsyncExceptionHandler:
    """异步异常处理研究"""
    
    @staticmethod
    async def handle_exceptions(coro: Callable, *args, **kwargs):
        """异常处理包装器"""
        try:
            return await coro(*args, **kwargs)
        except asyncio.CancelledError:
            print("Task was cancelled")
            raise
        except Exception as e:
            print(f"Exception occurred: {e}")
            return None
    
    @staticmethod
    async def timeout_with_fallback(coro: Callable, timeout: float, fallback=None):
        """带超时和回退的异常处理"""
        try:
            return await asyncio.wait_for(coro(), timeout=timeout)
        except asyncio.TimeoutError:
            print(f"Operation timed out after {timeout}s")
            return fallback

# 测试函数
async def test_basic_async():
    """测试基础异步模式"""
    print("=== 基础异步模式测试 ===")
    
    basic = BasicAsyncPatterns()
    
    # 测试简单协程
    result = await basic.simple_coroutine("Test Task", 0.5)
    print(f"Result: {result}")
    
    # 测试并发任务
    results = await basic.concurrent_tasks()
    print(f"Concurrent results: {results}")
    
    # 测试信号量限制
    sem = asyncio.Semaphore(2)
    tasks = [basic.task_with_semaphore(sem, i) for i in range(5)]
    await asyncio.gather(*tasks)

async def test_async_http():
    """测试异步HTTP客户端"""
    print("\n=== 异步HTTP客户端测试 ===")
    
    urls = [
        'https://httpbin.org/delay/1',
        'https://httpbin.org/delay/2',
        'https://httpbin.org/delay/1.5'
    ]
    
    async with AsyncHTTPClient(max_concurrent=2) as client:
        results = await client.fetch_multiple_urls(urls)
        for result in results:
            print(f"Fetched: {result['url']} (status: {result['status']})")

async def test_async_file_io():
    """测试异步文件IO"""
    print("\n=== 异步文件IO测试 ===")
    
    # 创建测试文件
    test_content = "Hello Async World!\n" * 1000
    test_file = "async_test.txt"
    
    file_io = AsyncFileIO()
    
    # 写入文件
    bytes_written = await file_io.write_large_file(test_file, test_content)
    print(f"Written {bytes_written} bytes to {test_file}")
    
    # 读取文件
    content = await file_io.read_large_file(test_file)
    print(f"Read {len(content)} bytes from {test_file}")
    
    # 清理
    Path(test_file).unlink(missing_ok=True)

async def test_producer_consumer():
    """测试生产者-消费者模式"""
    print("\n=== 生产者-消费者模式测试 ===")
    
    pc = AsyncProducerConsumer(max_size=10)
    
    producer_data = [
        [1, 2, 3, 4, 5],
        [6, 7, 8, 9, 10],
        [11, 12, 13, 14, 15]
    ]
    
    results = await pc.run_producer_consumer(producer_data, num_consumers=2)
    print(f"Processed {len(results)} items: {results}")

async def test_async_database():
    """测试异步数据库"""
    print("\n=== 异步数据库测试 ===")
    
    db = AsyncDatabase()
    
    # 并发创建用户
    user_tasks = [
        db.create_user(f"User{i}", f"user{i}@example.com") 
        for i in range(5)
    ]
    users = await asyncio.gather(*user_tasks)
    
    # 并发获取用户
    get_tasks = [db.get_user(user.id) for user in users]
    retrieved_users = await asyncio.gather(*get_tasks)
    
    print(f"Created {len(users)} users")
    for user in retrieved_users:
        if user:
            print(f"User: {user.name} ({user.email})")

async def test_performance_benchmarks():
    """测试性能基准"""
    print("\n=== 性能基准测试 ===")
    
    benchmark = AsyncPerformanceBenchmark()
    
    # 并发 vs 顺序测试
    perf_results = await benchmark.benchmark_concurrent_vs_sequential()
    print(f"并发性能提升: {perf_results['speedup']:.2f}x")
    
    # 线程池 vs 异步测试
    thread_results = await benchmark.benchmark_thread_pool_vs_async()
    print(f"异步 vs 线程池: {thread_results['async_time']:.2f}s vs {thread_results['thread_time']:.2f}s")

async def test_async_context():
    """测试异步上下文管理器"""
    print("\n=== 异步上下文管理器测试 ===")
    
    async with AsyncContextManager("Test Context") as ctx:
        print(f"Working in context: {ctx.name}")

async def test_async_exception_handling():
    """测试异步异常处理"""
    print("\n=== 异步异常处理测试 ===")
    
    handler = AsyncExceptionHandler()
    
    # 测试异常处理
    async def failing_task():
        await asyncio.sleep(0.1)
        raise ValueError("Test exception")
    
    result = await handler.handle_exceptions(failing_task)
    print(f"Exception handled, result: {result}")
    
    # 测试超时回退
    async def slow_task():
        await asyncio.sleep(2)
        return "slow result"
    
    result = await handler.timeout_with_fallback(slow_task, timeout=1.0, fallback="timeout fallback")
    print(f"Timeout fallback result: {result}")

# 主测试函数
async def main():
    """运行所有异步测试"""
    print("=== Python异步编程深度研究 ===")
    
    await test_basic_async()
    await test_async_http()
    await test_async_file_io()
    await test_producer_consumer()
    await test_async_database()
    await test_performance_benchmarks()
    await test_async_context()
    await test_async_exception_handling()
    
    print("\n=== 研究结论 ===")
    print("1. 异步编程显著提升IO密集型任务性能")
    print("2. asyncio提供完整的异步编程框架")
    print("3. 并发执行可线性提升吞吐量")
    print("4. 异步上下文管理器支持资源管理")
    print("5. 异常处理机制保证程序稳定性")
    print("6. 性能基准测试验证优化效果")

if __name__ == "__main__":
    asyncio.run(main())