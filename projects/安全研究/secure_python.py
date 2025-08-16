#!/usr/bin/env python3
"""
Python安全编程深度研究
输入验证、加密、安全通信与防御性编程
"""

import hashlib
import secrets
import re
import os
import json
import sqlite3
import hmac
import base64
import ssl
import socket
import threading
import time
from datetime import datetime, timedelta
from urllib.parse import urlparse, quote, unquote
from typing import Optional, Dict, Any, List, Tuple
import logging

# 配置日志
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class SecurityValidator:
    """输入验证与数据净化类"""
    
    @staticmethod
    def validate_email(email: str) -> bool:
        """验证邮箱地址格式"""
        pattern = r'^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$'
        return bool(re.match(pattern, email))
    
    @staticmethod
    def validate_username(username: str, min_length: int = 3, max_length: int = 20) -> bool:
        """验证用户名格式"""
        if not isinstance(username, str):
            return False
        if not (min_length <= len(username) <= max_length):
            return False
        pattern = r'^[a-zA-Z0-9_-]+$'
        return bool(re.match(pattern, username))
    
    @staticmethod
    def validate_password(password: str) -> Dict[str, Any]:
        """验证密码强度"""
        result = {
            'valid': True,
            'errors': [],
            'strength': 0
        }
        
        if len(password) < 8:
            result['valid'] = False
            result['errors'].append('密码长度至少8位')
        
        if not re.search(r'[A-Z]', password):
            result['valid'] = False
            result['errors'].append('需包含大写字母')
        
        if not re.search(r'[a-z]', password):
            result['valid'] = False
            result['errors'].append('需包含小写字母')
        
        if not re.search(r'\d', password):
            result['valid'] = False
            result['errors'].append('需包含数字')
        
        if not re.search(r'[!@#$%^&*(),.?":{}|<>]', password):
            result['valid'] = False
            result['errors'].append('需包含特殊字符')
        
        # 计算强度评分
        strength = 0
        strength += min(len(password) * 4, 32)  # 长度加分
        strength += 10 if re.search(r'[A-Z]', password) else 0
        strength += 10 if re.search(r'[a-z]', password) else 0
        strength += 10 if re.search(r'\d', password) else 0
        strength += 20 if re.search(r'[!@#$%^&*(),.?":{}|<>]', password) else 0
        
        result['strength'] = min(strength, 100)
        return result
    
    @staticmethod
    def sanitize_html(html: str) -> str:
        """净化HTML内容，防止XSS攻击"""
        dangerous_tags = ['script', 'iframe', 'object', 'embed', 'form']
        dangerous_attrs = ['onload', 'onerror', 'onclick', 'onmouseover', 'javascript:']
        
        # 移除危险标签
        for tag in dangerous_tags:
            html = re.sub(f'<{tag}.*?</{tag}>', '', html, flags=re.IGNORECASE | re.DOTALL)
        
        # 移除危险属性
        for attr in dangerous_attrs:
            html = re.sub(f'{attr}.*?["\'].*?["\']', '', html, flags=re.IGNORECASE)
        
        # 转义特殊字符
        html = html.replace('<', '&lt;').replace('>', '&gt;')
        html = html.replace('"', '&quot;').replace("'", '&#x27;')
        
        return html
    
    @staticmethod
    def validate_url(url: str) -> bool:
        """验证URL格式和安全性"""
        try:
            parsed = urlparse(url)
            if not parsed.scheme or not parsed.netloc:
                return False
            
            # 检查危险协议
            dangerous_schemes = ['file', 'javascript', 'data']
            if parsed.scheme.lower() in dangerous_schemes:
                return False
            
            return True
        except Exception:
            return False
    
    @staticmethod
    def validate_file_path(filepath: str) -> bool:
        """验证文件路径安全"""
        if not isinstance(filepath, str):
            return False
        
        # 检查路径遍历
        dangerous_patterns = ['..', '~', '/etc/passwd', '/etc/shadow', '\\windows\\system32']
        for pattern in dangerous_patterns:
            if pattern.lower() in filepath.lower():
                return False
        
        # 检查绝对路径
        if filepath.startswith('/') or filepath.startswith('C:\\'):
            return False
        
        return True

class PasswordManager:
    """密码管理器"""
    
    @staticmethod
    def hash_password(password: str, salt: Optional[bytes] = None) -> Tuple[str, str]:
        """安全地哈希密码"""
        if salt is None:
            salt = secrets.token_bytes(32)
        
        # 使用PBKDF2-HMAC-SHA256
        key = hashlib.pbkdf2_hmac('sha256', password.encode('utf-8'), salt, 100000)
        
        # 返回哈希值和盐值（base64编码）
        hashed_password = base64.b64encode(key).decode('utf-8')
        salt_str = base64.b64encode(salt).decode('utf-8')
        
        return hashed_password, salt_str
    
    @staticmethod
    def verify_password(password: str, hashed_password: str, salt_str: str) -> bool:
        """验证密码"""
        try:
            salt = base64.b64decode(salt_str.encode('utf-8'))
            new_hash, _ = PasswordManager.hash_password(password, salt)
            return hmac.compare_digest(new_hash, hashed_password)
        except Exception:
            return False
    
    @staticmethod
    def generate_secure_password(length: int = 12) -> str:
        """生成安全随机密码"""
        alphabet = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()'
        return ''.join(secrets.choice(alphabet) for _ in range(length))

class SecureSessionManager:
    """安全会话管理器"""
    
    def __init__(self, session_timeout: int = 3600):
        self.sessions: Dict[str, Dict[str, Any]] = {}
        self.session_timeout = session_timeout
        self.lock = threading.Lock()
        
        # 启动清理线程
        self.cleanup_thread = threading.Thread(target=self._cleanup_expired_sessions, daemon=True)
        self.cleanup_thread.start()
    
    def create_session(self, user_id: str) -> str:
        """创建新会话"""
        session_id = secrets.token_urlsafe(32)
        
        with self.lock:
            self.sessions[session_id] = {
                'user_id': user_id,
                'created_at': datetime.now(),
                'last_activity': datetime.now(),
                'ip_address': None,
                'user_agent': None
            }
        
        return session_id
    
    def validate_session(self, session_id: str) -> bool:
        """验证会话有效性"""
        with self.lock:
            if session_id not in self.sessions:
                return False
            
            session = self.sessions[session_id]
            if datetime.now() - session['last_activity'] > timedelta(seconds=self.session_timeout):
                del self.sessions[session_id]
                return False
            
            session['last_activity'] = datetime.now()
            return True
    
    def get_session_data(self, session_id: str) -> Optional[Dict[str, Any]]:
        """获取会话数据"""
        with self.lock:
            if self.validate_session(session_id):
                return self.sessions[session_id]
            return None
    
    def destroy_session(self, session_id: str) -> bool:
        """销毁会话"""
        with self.lock:
            if session_id in self.sessions:
                del self.sessions[session_id]
                return True
            return False
    
    def _cleanup_expired_sessions(self):
        """清理过期会话"""
        while True:
            time.sleep(300)  # 每5分钟清理一次
            with self.lock:
                expired_sessions = []
                for session_id, session_data in self.sessions.items():
                    if datetime.now() - session_data['last_activity'] > timedelta(seconds=self.session_timeout):
                        expired_sessions.append(session_id)
                
                for session_id in expired_sessions:
                    del self.sessions[session_id]

class SecureDatabase:
    """安全数据库操作类"""
    
    def __init__(self, db_path: str):
        self.db_path = db_path
        self.init_database()
    
    def init_database(self):
        """初始化数据库"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        # 创建用户表
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT UNIQUE NOT NULL,
                email TEXT UNIQUE NOT NULL,
                password_hash TEXT NOT NULL,
                salt TEXT NOT NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                last_login TIMESTAMP,
                failed_attempts INTEGER DEFAULT 0,
                locked_until TIMESTAMP
            )
        ''')
        
        # 创建审计日志表
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS audit_log (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id INTEGER,
                action TEXT NOT NULL,
                ip_address TEXT,
                user_agent TEXT,
                timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (user_id) REFERENCES users (id)
            )
        ''')
        
        conn.commit()
        conn.close()
    
    def create_user(self, username: str, email: str, password: str) -> bool:
        """安全创建用户"""
        if not SecurityValidator.validate_username(username):
            return False
        
        if not SecurityValidator.validate_email(email):
            return False
        
        password_check = SecurityValidator.validate_password(password)
        if not password_check['valid']:
            return False
        
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        try:
            # 检查用户名和邮箱是否已存在
            cursor.execute('SELECT id FROM users WHERE username = ? OR email = ?', (username, email))
            if cursor.fetchone():
                return False
            
            # 安全存储密码
            password_hash, salt = PasswordManager.hash_password(password)
            cursor.execute('''
                INSERT INTO users (username, email, password_hash, salt)
                VALUES (?, ?, ?, ?)
            ''', (username, email, password_hash, salt))
            
            conn.commit()
            return True
        except sqlite3.Error:
            return False
        finally:
            conn.close()
    
    def authenticate_user(self, username: str, password: str) -> bool:
        """安全认证用户"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        try:
            # 获取用户信息
            cursor.execute('''
                SELECT id, password_hash, salt, failed_attempts, locked_until
                FROM users WHERE username = ?
            ''', (username,))
            
            user = cursor.fetchone()
            if not user:
                return False
            
            user_id, stored_hash, salt_str, failed_attempts, locked_until = user
            
            # 检查账户是否被锁定
            if locked_until and datetime.now() < datetime.fromisoformat(locked_until):
                return False
            
            # 验证密码
            if not PasswordManager.verify_password(password, stored_hash, salt_str):
                # 增加失败次数
                new_attempts = failed_attempts + 1
                locked_time = None
                
                if new_attempts >= 5:
                    locked_time = datetime.now() + timedelta(minutes=30)
                
                cursor.execute('''
                    UPDATE users SET failed_attempts = ?, locked_until = ?
                    WHERE id = ?
                ''', (new_attempts, locked_time, user_id))
                conn.commit()
                return False
            
            # 重置失败次数
            cursor.execute('''
                UPDATE users SET failed_attempts = 0, locked_until = NULL, last_login = CURRENT_TIMESTAMP
                WHERE id = ?
            ''', (user_id,))
            conn.commit()
            
            return True
        except sqlite3.Error:
            return False
        finally:
            conn.close()

class CSRFProtector:
    """CSRF保护器"""
    
    def __init__(self):
        self.tokens: Dict[str, str] = {}
        self.token_timeout = 3600  # 1小时
    
    def generate_token(self, session_id: str) -> str:
        """生成CSRF令牌"""
        token = secrets.token_urlsafe(32)
        self.tokens[session_id] = token
        return token
    
    def validate_token(self, session_id: str, token: str) -> bool:
        """验证CSRF令牌"""
        if session_id not in self.tokens:
            return False
        
        stored_token = self.tokens[session_id]
        return hmac.compare_digest(stored_token, token)

class RateLimiter:
    """速率限制器"""
    
    def __init__(self, max_requests: int = 100, window_seconds: int = 3600):
        self.max_requests = max_requests
        self.window_seconds = window_seconds
        self.requests: Dict[str, List[datetime]] = {}
        self.lock = threading.Lock()
    
    def is_allowed(self, identifier: str) -> bool:
        """检查是否允许请求"""
        now = datetime.now()
        
        with self.lock:
            if identifier not in self.requests:
                self.requests[identifier] = []
            
            # 移除过期的请求记录
            self.requests[identifier] = [
                req_time for req_time in self.requests[identifier]
                if now - req_time < timedelta(seconds=self.window_seconds)
            ]
            
            # 检查是否超过限制
            if len(self.requests[identifier]) >= self.max_requests:
                return False
            
            # 记录新请求
            self.requests[identifier].append(now)
            return True

class SecureHTTPServer:
    """安全HTTP服务器示例"""
    
    def __init__(self, host: str = 'localhost', port: int = 8443):
        self.host = host
        self.port = port
        self.session_manager = SecureSessionManager()
        self.csrf_protector = CSRFProtector()
        self.rate_limiter = RateLimiter()
    
    def create_ssl_context(self) -> ssl.SSLContext:
        """创建SSL上下文"""
        context = ssl.create_default_context(ssl.Purpose.CLIENT_AUTH)
        context.load_cert_chain(certfile="server.crt", keyfile="server.key")
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        return context
    
    def handle_request(self, conn: socket.socket, addr: tuple):
        """处理HTTP请求"""
        try:
            data = conn.recv(1024).decode('utf-8')
            if not data:
                return
            
            # 简单的HTTP解析
            lines = data.split('\r\n')
            request_line = lines[0]
            
            # 速率限制
            client_ip = addr[0]
            if not self.rate_limiter.is_allowed(client_ip):
                response = "HTTP/1.1 429 Too Many Requests\r\n\r\nRate limit exceeded"
                conn.send(response.encode('utf-8'))
                return
            
            # 处理登录请求示例
            if 'POST /login' in request_line:
                response = self.handle_login(conn)
            else:
                response = "HTTP/1.1 200 OK\r\n\r\nSecure server running"
            
            conn.send(response.encode('utf-8'))
            
        except Exception as e:
            logger.error(f"处理请求时出错: {e}")
        finally:
            conn.close()
    
    def handle_login(self, conn: socket.socket) -> str:
        """处理登录请求"""
        # 这里应该解析POST数据，简化为示例
        session_id = self.session_manager.create_session("user123")
        return f"HTTP/1.1 200 OK\r\nSet-Cookie: session={session_id}\r\n\r\nLogin successful"

class SecurityTester:
    """安全测试工具"""
    
    @staticmethod
    def test_sql_injection():
        """测试SQL注入防护"""
        print("\n=== SQL注入防护测试 ===")
        
        # 恶意输入
        malicious_inputs = [
            "admin' OR '1'='1",
            "admin'; DROP TABLE users; --",
            "admin' UNION SELECT * FROM users --",
            "admin'/**/OR/**/1=1#"
        ]
        
        db = SecureDatabase(':memory:')
        db.create_user('admin', 'admin@example.com', 'SecurePass123!')
        
        for malicious_input in malicious_inputs:
            # 验证输入会被拒绝
            if not SecurityValidator.validate_username(malicious_input):
                print(f"✓ 阻止恶意输入: {malicious_input}")
            else:
                print(f"✗ 未阻止恶意输入: {malicious_input}")
    
    @staticmethod
    def test_xss_protection():
        """测试XSS防护"""
        print("\n=== XSS防护测试 ===")
        
        xss_payloads = [
            "<script>alert('XSS')</script>",
            "<img src=x onerror=alert('XSS')>",
            "javascript:alert('XSS')",
            "<svg onload=alert('XSS')>"
        ]
        
        for payload in xss_payloads:
            sanitized = SecurityValidator.sanitize_html(payload)
            if '<script>' not in sanitized and 'javascript:' not in sanitized.lower():
                print(f"✓ 净化XSS载荷: {payload}")
            else:
                print(f"✗ 未净化XSS载荷: {payload}")
    
    @staticmethod
    def test_password_security():
        """测试密码安全"""
        print("\n=== 密码安全测试 ===")
        
        test_passwords = [
            "123456",
            "password",
            "Password123",
            "SecurePass123!"
        ]
        
        for password in test_passwords:
            result = SecurityValidator.validate_password(password)
            print(f"密码 '{password}': 强度 {result['strength']}/100, 有效: {result['valid']}")
    
    @staticmethod
    def test_session_security():
        """测试会话安全"""
        print("\n=== 会话安全测试 ===")
        
        session_manager = SecureSessionManager()
        
        # 创建会话
        session_id = session_manager.create_session("test_user")
        print(f"创建会话: {session_id}")
        
        # 验证会话
        is_valid = session_manager.validate_session(session_id)
        print(f"验证会话: {is_valid}")
        
        # 销毁会话
        session_manager.destroy_session(session_id)
        is_valid = session_manager.validate_session(session_id)
        print(f"销毁后验证: {is_valid}")

def main():
    """主测试函数"""
    print("=== Python安全编程深度研究 ===")
    
    # 1. 输入验证测试
    print("\n[1] 输入验证测试")
    email = "user@example.com"
    username = "valid_user"
    password = "SecurePass123!"
    
    print(f"邮箱验证: {SecurityValidator.validate_email(email)}")
    print(f"用户名验证: {SecurityValidator.validate_username(username)}")
    print(f"密码验证: {SecurityValidator.validate_password(password)}")
    
    # 2. 密码安全测试
    print("\n[2] 密码安全测试")
    hashed_pw, salt = PasswordManager.hash_password(password)
    print(f"密码哈希: {hashed_pw[:20]}...")
    print(f"验证密码: {PasswordManager.verify_password(password, hashed_pw, salt)}")
    
    # 3. 会话管理测试
    print("\n[3] 会话管理测试")
    session_manager = SecureSessionManager()
    session_id = session_manager.create_session("user123")
    print(f"创建会话: {session_id}")
    
    # 4. 数据库安全测试
    print("\n[4] 数据库安全测试")
    db = SecureDatabase('/tmp/secure_test.db')
    
    # 创建用户
    success = db.create_user('testuser', 'test@example.com', 'SecurePass123!')
    print(f"创建用户: {success}")
    
    # 认证用户
    auth_success = db.authenticate_user('testuser', 'SecurePass123!')
    print(f"用户认证: {auth_success}")
    
    # 5. 安全测试
    print("\n[5] 安全测试执行")
    SecurityTester.test_sql_injection()
    SecurityTester.test_xss_protection()
    SecurityTester.test_password_security()
    SecurityTester.test_session_security()
    
    # 6. 速率限制测试
    print("\n[6] 速率限制测试")
    rate_limiter = RateLimiter(max_requests=5, window_seconds=60)
    
    for i in range(7):
        allowed = rate_limiter.is_allowed("test_client")
        print(f"请求 {i+1}: {'允许' if allowed else '拒绝'}")
    
    print("\n=== 研究结论 ===")
    print("1. 始终验证和净化用户输入")
    print("2. 使用安全密码存储和会话管理")
    print("3. 实施CSRF和速率限制保护")
    print("4. 定期进行安全测试和审计")
    print("5. 保持安全库和依赖更新")
    
    # 清理测试文件
    try:
        os.remove('/tmp/secure_test.db')
    except FileNotFoundError:
        pass

if __name__ == "__main__":
    main()