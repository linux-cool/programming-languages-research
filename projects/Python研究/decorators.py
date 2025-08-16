"""
Python装饰器深度研究
高级编程模式与元编程技术
"""
import functools
import time
import logging
import threading
import asyncio
from typing import Any, Callable, Dict, List, Optional, TypeVar, Union
from functools import wraps, lru_cache, singledispatch
import inspect

# 研究1: 基础装饰器实现
class BasicDecorators:
    """基础装饰器模式研究"""
    
    @staticmethod
    def timing_decorator(func: Callable) -> Callable:
        """计算函数执行时间的装饰器"""
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            start_time = time.time()
            result = func(*args, **kwargs)
            end_time = time.time()
            print(f"{func.__name__} took {end_time - start_time:.4f} seconds")
            return result
        return wrapper
    
    @staticmethod
    def logger_decorator(log_level: str = "INFO"):
        """日志记录装饰器"""
        def decorator(func: Callable) -> Callable:
            @functools.wraps(func)
            def wrapper(*args, **kwargs):
                logging.log(getattr(logging, log_level), 
                          f"Calling {func.__name__} with args={args}, kwargs={kwargs}")
                result = func(*args, **kwargs)
                logging.log(getattr(logging, log_level), 
                          f"{func.__name__} returned {result}")
                return result
            return wrapper
        return decorator
    
    @staticmethod
    def retry_decorator(max_attempts: int = 3, delay: float = 1.0):
        """重试机制装饰器"""
        def decorator(func: Callable) -> Callable:
            @functools.wraps(func)
            def wrapper(*args, **kwargs):
                for attempt in range(max_attempts):
                    try:
                        return func(*args, **kwargs)
                    except Exception as e:
                        if attempt == max_attempts - 1:
                            raise
                        print(f"Attempt {attempt + 1} failed: {e}")
                        time.sleep(delay)
            return wrapper
        return decorator

# 研究2: 带参数的装饰器工厂
class ParameterizedDecorators:
    """带参数的装饰器工厂模式"""
    
    def __init__(self):
        self.cache_stats = {}
    
    def memoize_with_ttl(self, ttl: float = 300.0):
        """带过期时间的缓存装饰器"""
        def decorator(func: Callable) -> Callable:
            cache = {}
            timestamps = {}
            
            @functools.wraps(func)
            def wrapper(*args, **kwargs):
                key = (args, frozenset(kwargs.items()) if kwargs else None)
                current_time = time.time()
                
                # 清理过期缓存
                expired_keys = [k for k, t in timestamps.items() 
                              if current_time - t > ttl]
                for k in expired_keys:
                    del cache[k]
                    del timestamps[k]
                
                if key in cache:
                    return cache[key]
                
                result = func(*args, **kwargs)
                cache[key] = result
                timestamps[key] = current_time
                return result
            
            wrapper.cache_clear = lambda: cache.clear()
            wrapper.cache_info = lambda: {
                'size': len(cache),
                'ttl': ttl
            }
            return wrapper
        return decorator
    
    def rate_limit(self, max_calls: int = 100, window: float = 60.0):
        """速率限制装饰器"""
        def decorator(func: Callable) -> Callable:
            calls = []
            lock = threading.Lock()
            
            @functools.wraps(func)
            def wrapper(*args, **kwargs):
                current_time = time.time()
                
                with lock:
                    # 清理旧调用记录
                    calls[:] = [t for t in calls if current_time - t < window]
                    
                    if len(calls) >= max_calls:
                        raise RuntimeError(f"Rate limit exceeded: {max_calls} calls per {window}s")
                    
                    calls.append(current_time)
                
                return func(*args, **kwargs)
            
            wrapper.rate_limit_info = lambda: {
                'calls_in_window': len(calls),
                'max_calls': max_calls,
                'window': window
            }
            return wrapper
        return decorator

# 研究3: 类装饰器和元类
class ClassDecorators:
    """类装饰器与元类研究"""
    
    @staticmethod
    def singleton(cls):
        """单例模式装饰器"""
        instances = {}
        lock = threading.Lock()
        
        @functools.wraps(cls)
        def wrapper(*args, **kwargs):
            if cls not in instances:
                with lock:
                    if cls not in instances:
                        instances[cls] = cls(*args, **kwargs)
            return instances[cls]
        return wrapper
    
    @staticmethod
    def auto_str(cls):
        """自动生成__str__方法的装饰器"""
        def __str__(self):
            attrs = [f"{k}={v}" for k, v in self.__dict__.items()]
            return f"{cls.__name__}({', '.join(attrs)})"
        
        cls.__str__ = __str__
        return cls
    
    @staticmethod
    def validate_attributes(*required_attrs):
        """属性验证装饰器"""
        def decorator(cls):
            original_init = cls.__init__
            
            @functools.wraps(original_init)
            def new_init(self, *args, **kwargs):
                original_init(self, *args, **kwargs)
                
                missing = [attr for attr in required_attrs 
                          if not hasattr(self, attr) or getattr(self, attr) is None]
                if missing:
                    raise ValueError(f"Missing required attributes: {missing}")
            
            cls.__init__ = new_init
            return cls
        return decorator

# 研究4: 异步装饰器
class AsyncDecorators:
    """异步编程装饰器"""
    
    @staticmethod
    def async_timeout(seconds: float):
        """异步超时装饰器"""
        def decorator(func):
            @functools.wraps(func)
            async def wrapper(*args, **kwargs):
                try:
                    return await asyncio.wait_for(func(*args, **kwargs), timeout=seconds)
                except asyncio.TimeoutError:
                    raise TimeoutError(f"Function {func.__name__} timed out after {seconds}s")
            return wrapper
        return decorator
    
    @staticmethod
    def async_retry(max_attempts: int = 3, delay: float = 1.0):
        """异步重试装饰器"""
        def decorator(func):
            @functools.wraps(func)
            async def wrapper(*args, **kwargs):
                for attempt in range(max_attempts):
                    try:
                        return await func(*args, **kwargs)
                    except Exception as e:
                        if attempt == max_attempts - 1:
                            raise
                        print(f"Async attempt {attempt + 1} failed: {e}")
                        await asyncio.sleep(delay)
            return wrapper
        return decorator

# 研究5: 装饰器链和组合
class DecoratorComposition:
    """装饰器组合模式"""
    
    @staticmethod
    def compose(*decorators):
        """组合多个装饰器"""
        def decorator(func):
            for dec in reversed(decorators):
                func = dec(func)
            return func
        return decorator
    
    @staticmethod
    def conditional_decorator(condition: Callable[[], bool], decorator: Callable):
        """条件装饰器"""
        def wrapper(func):
            if condition():
                return decorator(func)
            return func
        return wrapper

# 研究6: 高级装饰器特性
class AdvancedDecorators:
    """高级装饰器特性研究"""
    
    @staticmethod
    def debug_decorator(verbose: bool = False):
        """调试装饰器"""
        def decorator(func):
            @functools.wraps(func)
            def wrapper(*args, **kwargs):
                if verbose:
                    print(f"Calling {func.__name__}")
                    print(f"Arguments: args={args}, kwargs={kwargs}")
                    print(f"Function signature: {inspect.signature(func)}")
                
                try:
                    result = func(*args, **kwargs)
                    if verbose:
                        print(f"{func.__name__} returned: {result}")
                    return result
                except Exception as e:
                    if verbose:
                        print(f"{func.__name__} raised: {e}")
                    raise
            return wrapper
        return decorator
    
    @staticmethod
    def type_check(*types, **kwtypes):
        """类型检查装饰器"""
        def decorator(func):
            @functools.wraps(func)
            def wrapper(*args, **kwargs):
                # 检查位置参数类型
                for i, (arg, expected_type) in enumerate(zip(args, types)):
                    if not isinstance(arg, expected_type):
                        raise TypeError(
                            f"Argument {i} expected {expected_type}, got {type(arg)}")
                
                # 检查关键字参数类型
                for key, value in kwargs.items():
                    if key in kwtypes and not isinstance(value, kwtypes[key]):
                        raise TypeError(
                            f"Keyword argument {key} expected {kwtypes[key]}, got {type(value)}")
                
                return func(*args, **kwargs)
            return wrapper
        return decorator
    
    @staticmethod
    def register_method(registry: Dict[str, Callable]):
        """方法注册装饰器"""
        def decorator(func):
            registry[func.__name__] = func
            return func
        return decorator

# 研究7: 装饰器实现示例
class DecoratorExamples:
    """装饰器实现示例"""
    
    def __init__(self):
        self.basic = BasicDecorators()
        self.parameterized = ParameterizedDecorators()
        self.class_dec = ClassDecorators()
        self.async_dec = AsyncDecorators()
        self.composition = DecoratorComposition()
        self.advanced = AdvancedDecorators()
    
    @BasicDecorators.timing_decorator
    def slow_function(self, n: int) -> int:
        """慢速函数示例"""
        time.sleep(0.1)
        return sum(range(n))
    
    @ParameterizedDecorators().memoize_with_ttl(ttl=5.0)
    def fibonacci(self, n: int) -> int:
        """带缓存的斐波那契数列"""
        if n <= 1:
            return n
        return self.fibonacci(n-1) + self.fibonacci(n-2)
    
    @ClassDecorators.singleton
    class DatabaseConnection:
        """单例数据库连接"""
        def __init__(self):
            self.connection_id = id(self)
            print(f"Database connection created: {self.connection_id}")
    
    @ClassDecorators.auto_str
    @ClassDecorators.validate_attributes('name', 'age')
    class Person:
        """自动字符串表示和属性验证"""
        def __init__(self, name: str, age: int):
            self.name = name
            self.age = age