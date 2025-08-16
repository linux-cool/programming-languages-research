#!/usr/bin/env python3
"""
Python性能测试深度研究
多维度性能分析与优化策略
"""

import time
import random
import string
import json
import csv
import os
import sys
import threading
import multiprocessing
from concurrent.futures import ThreadPoolExecutor, ProcessPoolExecutor
from typing import List, Dict, Any, Callable
import timeit
import tracemalloc
import gc
import statistics
import math

class PerformanceTester:
    """性能测试基类"""
    
    def __init__(self):
        self.results = []
    
    def measure_time(self, func: Callable, *args, **kwargs) -> Dict[str, Any]:
        """测量函数执行时间"""
        start_time = time.perf_counter()
        result = func(*args, **kwargs)
        end_time = time.perf_counter()
        
        return {
            'function': func.__name__,
            'execution_time': end_time - start_time,
            'result': result
        }
    
    def measure_memory(self, func: Callable, *args, **kwargs) -> Dict[str, Any]:
        """测量内存使用"""
        tracemalloc.start()
        start_memory = tracemalloc.get_traced_memory()[0]
        
        result = func(*args, **kwargs)
        
        current, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        
        return {
            'function': func.__name__,
            'current_memory': current - start_memory,
            'peak_memory': peak - start_memory,
            'result': result
        }

class CPUBenchmark(PerformanceTester):
    """CPU性能基准测试"""
    
    def test_arithmetic_operations(self, iterations: int = 1000000) -> Dict[str, Any]:
        """测试算术运算性能"""
        def integer_addition():
            total = 0
            for i in range(iterations):
                total += i
            return total
        
        def float_addition():
            total = 0.0
            for i in range(iterations):
                total += float(i)
            return total
        
        def multiplication():
            total = 1.0
            for i in range(1, iterations):
                total *= 1.000001
            return total
        
        def division():
            total = 1.0
            for i in range(1, iterations):
                total /= 1.000001
            return total
        
        results = {
            'integer_addition': self.measure_time(integer_addition),
            'float_addition': self.measure_time(float_addition),
            'multiplication': self.measure_time(multiplication),
            'division': self.measure_time(division)
        }
        
        # 计算吞吐量
        for test_name, result in results.items():
            throughput = iterations / result['execution_time']
            result['throughput'] = throughput
            result['throughput_unit'] = 'ops/sec'
        
        return results
    
    def test_string_operations(self, iterations: int = 100000) -> Dict[str, Any]:
        """测试字符串操作性能"""
        test_string = "Hello, World! " * 100
        
        def string_concatenation():
            result = ""
            for i in range(iterations):
                result += test_string[:10]
            return len(result)
        
        def string_formatting():
            result = []
            for i in range(iterations):
                result.append(f"String {i}: {test_string}")
            return len(result)
        
        def string_joining():
            parts = [test_string] * iterations
            return "".join(parts)
        
        def string_splitting():
            s = test_string * iterations
            return s.split()
        
        return {
            'string_concatenation': self.measure_time(string_concatenation),
            'string_formatting': self.measure_time(string_formatting),
            'string_joining': self.measure_time(string_joining),
            'string_splitting': self.measure_time(string_splitting)
        }

class MemoryBenchmark(PerformanceTester):
    """内存性能基准测试"""
    
    def test_list_operations(self, size: int = 100000) -> Dict[str, Any]:
        """测试列表操作性能"""
        
        def list_creation():
            return [i for i in range(size)]
        
        def list_append():
            lst = []
            for i in range(size):
                lst.append(i)
            return len(lst)
        
        def list_insert():
            lst = []
            for i in range(size):
                lst.insert(0, i)
            return len(lst)
        
        def list_lookup():
            lst = list(range(size))
            total = 0
            for i in range(size):
                total += lst[i]
            return total
        
        return {
            'list_creation': self.measure_time(list_creation),
            'list_append': self.measure_time(list_append),
            'list_insert': self.measure_time(list_insert),
            'list_lookup': self.measure_time(list_lookup)
        }
    
    def test_dict_operations(self, size: int = 100000) -> Dict[str, Any]:
        """测试字典操作性能"""
        
        def dict_creation():
            return {i: str(i) for i in range(size)}
        
        def dict_insertion():
            d = {}
            for i in range(size):
                d[i] = str(i)
            return len(d)
        
        def dict_lookup():
            d = {i: str(i) for i in range(size)}
            total = 0
            for i in range(size):
                total += len(d[i])
            return total
        
        def dict_deletion():
            d = {i: str(i) for i in range(size)}
            for i in range(0, size, 2):
                del d[i]
            return len(d)
        
        return {
            'dict_creation': self.measure_time(dict_creation),
            'dict_insertion': self.measure_time(dict_insertion),
            'dict_lookup': self.measure_time(dict_lookup),
            'dict_deletion': self.measure_time(dict_deletion)
        }
    
    def test_memory_allocation(self, iterations: int = 10000) -> Dict[str, Any]:
        """测试内存分配性能"""
        
        def small_object_allocation():
            objects = []
            for i in range(iterations):
                objects.append({"id": i, "data": "x" * 100})
            return len(objects)
        
        def large_object_allocation():
            objects = []
            for i in range(iterations // 10):
                objects.append([0] * 1000)
            return len(objects)
        
        def memory_intensive_operation():
            data = []
            for i in range(iterations):
                data.append([j for j in range(100)])
            return sum(len(x) for x in data)
        
        return {
            'small_object_allocation': self.measure_memory(small_object_allocation),
            'large_object_allocation': self.measure_memory(large_object_allocation),
            'memory_intensive_operation': self.measure_memory(memory_intensive_operation)
        }

class IOTBenchmark(PerformanceTester):
    """IO性能基准测试"""
    
    def test_file_io(self, file_size_mb: int = 10) -> Dict[str, Any]:
        """测试文件IO性能"""
        filename = f'/tmp/test_{file_size_mb}mb.txt'
        test_data = "x" * 1024 * 1024  # 1MB data
        
        def write_file():
            with open(filename, 'w') as f:
                for _ in range(file_size_mb):
                    f.write(test_data)
            return os.path.getsize(filename)
        
        def read_file():
            with open(filename, 'r') as f:
                data = f.read()
            return len(data)
        
        def read_line_by_line():
            with open(filename, 'r') as f:
                lines = f.readlines()
            return len(lines)
        
        results = {
            'write_file': self.measure_time(write_file),
            'read_file': self.measure_time(read_file),
            'read_line_by_line': self.measure_time(read_line_by_line)
        }
        
        # 计算吞吐量
        total_bytes = file_size_mb * 1024 * 1024
        for test_name, result in results.items():
            throughput = total_bytes / result['execution_time']
            result['throughput'] = throughput / (1024 * 1024)  # MB/s
            result['throughput_unit'] = 'MB/s'
        
        # 清理文件
        try:
            os.remove(filename)
        except FileNotFoundError:
            pass
        
        return results
    
    def test_json_performance(self, iterations: int = 10000) -> Dict[str, Any]:
        """测试JSON序列化性能"""
        test_data = {
            "users": [
                {"id": i, "name": f"User{i}", "email": f"user{i}@example.com"}
                for i in range(100)
            ]
        }
        
        def json_serialization():
            return json.dumps(test_data)
        
        def json_deserialization(data: str):
            return json.loads(data)
        
        serialized = json.dumps(test_data)
        
        return {
            'json_serialization': self.measure_time(json_serialization),
            'json_deserialization': self.measure_time(json_deserialization, serialized)
        }

class ConcurrencyBenchmark(PerformanceTester):
    """并发性能基准测试"""
    
    def test_threading_performance(self, num_threads: int = 4, iterations: int = 1000000) -> Dict[str, Any]:
        """测试多线程性能"""
        
        def cpu_bound_task(start: int, end: int) -> int:
            return sum(range(start, end))
        
        def single_thread():
            chunk_size = iterations // num_threads
            results = []
            for i in range(num_threads):
                start = i * chunk_size
                end = (i + 1) * chunk_size if i < num_threads - 1 else iterations
                results.append(cpu_bound_task(start, end))
            return sum(results)
        
        def multi_thread():
            chunk_size = iterations // num_threads
            threads = []
            results = [0] * num_threads
            
            def worker(thread_id: int, start: int, end: int):
                results[thread_id] = cpu_bound_task(start, end)
            
            for i in range(num_threads):
                start = i * chunk_size
                end = (i + 1) * chunk_size if i < num_threads - 1 else iterations
                thread = threading.Thread(target=worker, args=(i, start, end))
                threads.append(thread)
                thread.start()
            
            for thread in threads:
                thread.join()
            
            return sum(results)
        
        return {
            'single_thread': self.measure_time(single_thread),
            'multi_thread': self.measure_time(multi_thread)
        }
    
    def test_multiprocessing_performance(self, num_processes: int = 4, iterations: int = 1000000) -> Dict[str, Any]:
        """测试多进程性能"""
        
        def cpu_bound_task(data: List[int]) -> int:
            return sum(data)
        
        def single_process():
            data = list(range(iterations))
            return cpu_bound_task(data)
        
        def multi_process():
            chunk_size = iterations // num_processes
            data_chunks = []
            
            for i in range(num_processes):
                start = i * chunk_size
                end = (i + 1) * chunk_size if i < num_processes - 1 else iterations
                data_chunks.append(list(range(start, end)))
            
            with ProcessPoolExecutor(max_workers=num_processes) as executor:
                results = list(executor.map(cpu_bound_task, data_chunks))
            
            return sum(results)
        
        return {
            'single_process': self.measure_time(single_process),
            'multi_process': self.measure_time(multi_process)
        }

class AlgorithmBenchmark(PerformanceTester):
    """算法性能基准测试"""
    
    def test_sorting_algorithms(self, array_size: int = 10000) -> Dict[str, Any]:
        """测试排序算法性能"""
        
        def generate_random_array():
            return [random.randint(0, 1000000) for _ in range(array_size)]
        
        def builtin_sort(data):
            return sorted(data)
        
        def quicksort(arr):
            if len(arr) <= 1:
                return arr
            pivot = arr[len(arr) // 2]
            left = [x for x in arr if x < pivot]
            middle = [x for x in arr if x == pivot]
            right = [x for x in arr if x > pivot]
            return quicksort(left) + middle + quicksort(right)
        
        def bubblesort(arr):
            n = len(arr)
            arr_copy = arr[:]
            for i in range(n):
                for j in range(0, n - i - 1):
                    if arr_copy[j] > arr_copy[j + 1]:
                        arr_copy[j], arr_copy[j + 1] = arr_copy[j + 1], arr_copy[j]
            return arr_copy
        
        test_data = generate_random_array()
        
        return {
            'builtin_sort': self.measure_time(builtin_sort, test_data[:]),
            'quicksort': self.measure_time(quicksort, test_data[:]),
            'bubblesort': self.measure_time(bubblesort, test_data[:])
        }
    
    def test_search_algorithms(self, array_size: int = 100000) -> Dict[str, Any]:
        """测试搜索算法性能"""
        
        def generate_sorted_array():
            return sorted([random.randint(0, 1000000) for _ in range(array_size)])
        
        def linear_search(arr, target):
            for i, val in enumerate(arr):
                if val == target:
                    return i
            return -1
        
        def binary_search(arr, target):
            left, right = 0, len(arr) - 1
            while left <= right:
                mid = (left + right) // 2
                if arr[mid] == target:
                    return mid
                elif arr[mid] < target:
                    left = mid + 1
                else:
                    right = mid - 1
            return -1
        
        test_array = generate_sorted_array()
        target = test_array[array_size // 2]  # 存在的目标
        
        return {
            'linear_search': self.measure_time(linear_search, test_array, target),
            'binary_search': self.measure_time(binary_search, test_array, target)
        }

class NetworkBenchmark(PerformanceTester):
    """网络性能基准测试"""
    
    def test_http_client(self, num_requests: int = 100) -> Dict[str, Any]:
        """测试HTTP客户端性能"""
        try:
            import requests
            
            def make_requests():
                for i in range(num_requests):
                    try:
                        response = requests.get('https://httpbin.org/delay/0.1', timeout=5)
                        response.raise_for_status()
                    except Exception as e:
                        print(f"Request {i} failed: {e}")
                return num_requests
            
            return {
                'http_requests': self.measure_time(make_requests)
            }
        except ImportError:
            return {
                'http_requests': {
                    'error': 'requests library not available',
                    'execution_time': 0
                }
            }

class DatabaseBenchmark(PerformanceTester):
    """数据库性能基准测试"""
    
    def test_sqlite_performance(self, num_records: int = 10000) -> Dict[str, Any]:
        """测试SQLite数据库性能"""
        import sqlite3
        
        db_path = '/tmp/test_performance.db'
        
        def create_database():
            conn = sqlite3.connect(db_path)
            cursor = conn.cursor()
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS users (
                    id INTEGER PRIMARY KEY,
                    name TEXT,
                    email TEXT,
                    age INTEGER
                )
            ''')
            conn.commit()
            conn.close()
            return True
        
        def insert_records():
            conn = sqlite3.connect(db_path)
            cursor = conn.cursor()
            for i in range(num_records):
                cursor.execute(
                    "INSERT INTO users (name, email, age) VALUES (?, ?, ?)",
                    (f"User{i}", f"user{i}@example.com", random.randint(18, 80))
                )
            conn.commit()
            conn.close()
            return num_records
        
        def query_records():
            conn = sqlite3.connect(db_path)
            cursor = conn.cursor()
            cursor.execute("SELECT * FROM users WHERE age > 30")
            results = cursor.fetchall()
            conn.close()
            return len(results)
        
        def update_records():
            conn = sqlite3.connect(db_path)
            cursor = conn.cursor()
            cursor.execute("UPDATE users SET age = age + 1 WHERE age < 50")
            conn.commit()
            conn.close()
            return cursor.rowcount
        
        # 初始化数据库
        create_database()
        
        results = {
            'insert_records': self.measure_time(insert_records),
            'query_records': self.measure_time(query_records),
            'update_records': self.measure_time(update_records)
        }
        
        # 清理
        try:
            os.remove(db_path)
        except FileNotFoundError:
            pass
        
        return results

class PerformanceReport:
    """性能报告生成器"""
    
    def __init__(self):
        self.tester = PerformanceTester()
        self.benchmarks = {
            'cpu': CPUBenchmark(),
            'memory': MemoryBenchmark(),
            'io': IOTBenchmark(),
            'concurrency': ConcurrencyBenchmark(),
            'algorithms': AlgorithmBenchmark(),
            'network': NetworkBenchmark(),
            'database': DatabaseBenchmark()
        }
    
    def run_all_benchmarks(self) -> Dict[str, Any]:
        """运行所有基准测试"""
        results = {}
        
        print("=== Python性能测试深度研究 ===")
        print("运行所有基准测试...\n")
        
        # CPU基准测试
        print("1. CPU性能测试...")
        results['cpu'] = {
            'arithmetic': self.benchmarks['cpu'].test_arithmetic_operations(),
            'string_operations': self.benchmarks['cpu'].test_string_operations()
        }
        
        # 内存基准测试
        print("2. 内存性能测试...")
        results['memory'] = {
            'list_operations': self.benchmarks['memory'].test_list_operations(),
            'dict_operations': self.benchmarks['memory'].test_dict_operations(),
            'memory_allocation': self.benchmarks['memory'].test_memory_allocation()
        }
        
        # IO基准测试
        print("3. IO性能测试...")
        results['io'] = {
            'file_io': self.benchmarks['io'].test_file_io(),
            'json_performance': self.benchmarks['io'].test_json_performance()
        }
        
        # 并发基准测试
        print("4. 并发性能测试...")
        results['concurrency'] = {
            'threading': self.benchmarks['concurrency'].test_threading_performance(),
            'multiprocessing': self.benchmarks['concurrency'].test_multiprocessing_performance()
        }
        
        # 算法基准测试
        print("5. 算法性能测试...")
        results['algorithms'] = {
            'sorting': self.benchmarks['algorithms'].test_sorting_algorithms(),
            'searching': self.benchmarks['algorithms'].test_search_algorithms()
        }
        
        # 数据库基准测试
        print("6. 数据库性能测试...")
        results['database'] = {
            'sqlite': self.benchmarks['database'].test_sqlite_performance()
        }
        
        return results
    
    def generate_report(self, results: Dict[str, Any], output_file: str = 'performance_report.json') -> None:
        """生成性能报告"""
        
        # 保存JSON报告
        with open(output_file, 'w') as f:
            json.dump(results, f, indent=2, default=str)
        
        # 生成CSV摘要
        csv_file = output_file.replace('.json', '.csv')
        with open(csv_file, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['Category', 'Test', 'Execution Time (s)', 'Throughput'])
            
            for category, tests in results.items():
                for test_name, test_results in tests.items():
                    if isinstance(test_results, dict) and 'execution_time' in test_results:
                        writer.writerow([
                            category,
                            test_name,
                            test_results['execution_time'],
                            test_results.get('throughput', 'N/A')
                        ])
        
        print(f"\n性能报告已生成:")
        print(f"- JSON详细报告: {output_file}")
        print(f"- CSV摘要报告: {csv_file}")
    
    def print_summary(self, results: Dict[str, Any]) -> None:
        """打印测试摘要"""
        print("\n=== 性能测试摘要 ===")
        
        for category, tests in results.items():
            print(f"\n{category.upper()}性能:")
            for test_name, test_results in tests.items():
                if isinstance(test_results, dict) and 'execution_time' in test_results:
                    print(f"  {test_name}: {test_results['execution_time']:.4f}s")

class PerformanceOptimizer:
    """性能优化建议"""
    
    @staticmethod
    def get_optimization_tips() -> List[str]:
        """获取性能优化建议"""
        return [
            "1. 使用内置函数和标准库，它们通常经过高度优化",
            "2. 避免在循环中进行字符串拼接，使用join()方法",
            "3. 使用列表推导式替代循环进行列表构建",
            "4. 对于大量数据，使用生成器表达式减少内存使用",
            "5. 使用set或dict进行快速查找，时间复杂度O(1)",
            "6. 避免使用全局变量，使用局部变量更快",
            "7. 使用__slots__减少类实例的内存占用",
            "8. 对于数值计算，考虑使用NumPy等C扩展库",
            "9. 使用缓存（如functools.lru_cache）避免重复计算",
            "10. 使用异步编程（asyncio）处理IO密集型任务",
            "11. 使用multiprocessing处理CPU密集型任务",
            "12. 使用profiling工具（如cProfile）定位性能瓶颈",
            "13. 避免不必要的对象创建和销毁",
            "14. 使用适当的数据结构，如deque用于队列操作",
            "15. 考虑使用JIT编译器（如PyPy）提高性能"
        ]

def main():
    """主函数"""
    
    print("=== Python性能测试深度研究 ===")
    print("系统信息:")
    print(f"Python版本: {sys.version}")
    print(f"CPU核心数: {multiprocessing.cpu_count()}")
    print(f"操作系统: {os.name}")
    print()
    
    # 创建性能报告
    report = PerformanceReport()
    
    # 运行所有基准测试
    results = report.run_all_benchmarks()
    
    # 生成报告
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    report.generate_report(results, f'performance_report_{timestamp}.json')
    
    # 打印摘要
    report.print_summary(results)
    
    # 打印优化建议
    print("\n=== 性能优化建议 ===")
    optimizer = PerformanceOptimizer()
    for tip in optimizer.get_optimization_tips():
        print(tip)
    
    print("\n=== 测试完成 ===")

if __name__ == "__main__":
    main()