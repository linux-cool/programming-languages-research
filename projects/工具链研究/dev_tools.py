#!/usr/bin/env python3
"""
Python开发工具链深度研究
构建系统、包管理、测试框架与开发工具集成
"""

import os
import sys
import subprocess
import json
import time
import re
import shutil
from pathlib import Path
from typing import Dict, List, Any, Optional, Tuple
import argparse
import logging
from datetime import datetime

class BuildSystemAnalyzer:
    """构建系统分析器"""
    
    def __init__(self, project_root: str = "."):
        self.project_root = Path(project_root)
        self.build_systems = {
            'setup.py': 'setuptools',
            'pyproject.toml': 'poetry/flit',
            'requirements.txt': 'pip',
            'Pipfile': 'pipenv',
            'poetry.lock': 'poetry',
            'environment.yml': 'conda',
            'tox.ini': 'tox',
            'noxfile.py': 'nox'
        }
    
    def detect_build_system(self) -> Dict[str, Any]:
        """检测项目使用的构建系统"""
        detected = {}
        
        for filename, system in self.build_systems.items():
            file_path = self.project_root / filename
            if file_path.exists():
                detected[system] = str(file_path)
        
        return {
            'project_root': str(self.project_root),
            'build_systems': detected,
            'primary_system': next(iter(detected.keys()), 'unknown')
        }
    
    def analyze_dependencies(self) -> Dict[str, List[str]]:
        """分析项目依赖"""
        dependencies = {
            'runtime': [],
            'development': [],
            'optional': []
        }
        
        # 分析requirements.txt
        req_file = self.project_root / "requirements.txt"
        if req_file.exists():
            with open(req_file) as f:
                dependencies['runtime'] = [line.strip() for line in f 
                                         if line.strip() and not line.startswith('#')]
        
        # 分析setup.py
        setup_py = self.project_root / "setup.py"
        if setup_py.exists():
            try:
                result = subprocess.run([sys.executable, "setup.py", "--requires"], 
                                      capture_output=True, text=True, cwd=self.project_root)
                if result.returncode == 0:
                    dependencies['runtime'].extend(result.stdout.strip().split('\n'))
            except Exception:
                pass
        
        return dependencies

class PackageManager:
    """包管理器工具"""
    
    def __init__(self):
        self.managers = {
            'pip': self._check_command('pip'),
            'pipenv': self._check_command('pipenv'),
            'poetry': self._check_command('poetry'),
            'conda': self._check_command('conda'),
            'pipx': self._check_command('pipx')
        }
    
    def _check_command(self, cmd: str) -> bool:
        """检查命令是否可用"""
        return shutil.which(cmd) is not None
    
    def get_installed_packages(self) -> Dict[str, str]:
        """获取已安装的包信息"""
        packages = {}
        
        try:
            result = subprocess.run([sys.executable, '-m', 'pip', 'list', '--format=json'],
                                  capture_output=True, text=True)
            if result.returncode == 0:
                data = json.loads(result.stdout)
                packages = {pkg['name']: pkg['version'] for pkg in data}
        except Exception as e:
            print(f"获取包信息失败: {e}")
        
        return packages
    
    def create_virtual_environment(self, env_path: str, python_version: str = None) -> bool:
        """创建虚拟环境"""
        try:
            cmd = [sys.executable, '-m', 'venv', env_path]
            if python_version:
                cmd.extend(['--python', python_version])
            
            result = subprocess.run(cmd, capture_output=True, text=True)
            return result.returncode == 0
        except Exception as e:
            print(f"创建虚拟环境失败: {e}")
            return False
    
    def install_packages(self, packages: List[str], dev: bool = False) -> bool:
        """安装包"""
        cmd = [sys.executable, '-m', 'pip', 'install']
        if dev:
            cmd.append('--dev')
        cmd.extend(packages)
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True)
            return result.returncode == 0
        except Exception as e:
            print(f"安装包失败: {e}")
            return False

class TestFramework:
    """测试框架集成"""
    
    def __init__(self, project_root: str = "."):
        self.project_root = Path(project_root)
        self.test_frameworks = {
            'pytest': self._has_pytest,
            'unittest': self._has_unittest,
            'nose': self._has_nose,
            'tox': self._has_tox
        }
    
    def _has_pytest(self) -> bool:
        return (self.project_root / "pytest.ini").exists() or \
               (self.project_root / "pyproject.toml").exists() and 
               "pytest" in (self.project_root / "pyproject.toml").read_text()
    
    def _has_unittest(self) -> bool:
        test_files = list(self.project_root.glob("**/test_*.py")) + \
                    list(self.project_root.glob("**/*_test.py"))
        return len(test_files) > 0
    
    def _has_nose(self) -> bool:
        return (self.project_root / "nose.cfg").exists()
    
    def _has_tox(self) -> bool:
        return (self.project_root / "tox.ini").exists()
    
    def detect_test_framework(self) -> Dict[str, Any]:
        """检测测试框架"""
        detected = []
        
        for name, check_func in self.test_frameworks.items():
            if check_func():
                detected.append(name)
        
        return {
            'available_frameworks': detected,
            'test_files': list(self.project_root.glob("**/test_*.py")) + 
                         list(self.project_root.glob("**/*_test.py")),
            'coverage_files': list(self.project_root.glob("**/.coverage*"))
        }
    
    def run_tests(self, framework: str = "pytest") -> Dict[str, Any]:
        """运行测试"""
        result = {
            'framework': framework,
            'success': False,
            'output': '',
            'error': '',
            'coverage': None
        }
        
        try:
            if framework == "pytest":
                cmd = [sys.executable, '-m', 'pytest', '-v', '--tb=short']
                if (self.project_root / '.coveragerc').exists():
                    cmd.extend(['--cov', str(self.project_root)])
                
                proc = subprocess.run(cmd, capture_output=True, text=True, 
                                    cwd=self.project_root)
                result['success'] = proc.returncode == 0
                result['output'] = proc.stdout
                result['error'] = proc.stderr
                
        except Exception as e:
            result['error'] = str(e)
        
        return result

class CodeQualityAnalyzer:
    """代码质量分析器"""
    
    def __init__(self, project_root: str = "."):
        self.project_root = Path(project_root)
    
    def run_linter(self, linter: str = "pylint") -> Dict[str, Any]:
        """运行代码检查工具"""
        linters = {
            'pylint': ['pylint', '--output-format=json'],
            'flake8': ['flake8', '--format=json'],
            'black': ['black', '--check', '--diff'],
            'isort': ['isort', '--check-only', '--diff'],
            'mypy': ['mypy', '--json']
        }
        
        if linter not in linters:
            return {'error': f'不支持的检查器: {linter}'}
        
        if not shutil.which(linter):
            return {'error': f'{linter} 未安装'}
        
        try:
            cmd = linters[linter]
            python_files = list(self.project_root.rglob("*.py"))
            cmd.extend([str(f) for f in python_files])
            
            result = subprocess.run(cmd, capture_output=True, text=True, 
                                  cwd=self.project_root)
            
            return {
                'linter': linter,
                'success': result.returncode == 0,
                'output': result.stdout,
                'error': result.stderr,
                'returncode': result.returncode
            }
        except Exception as e:
            return {'error': str(e)}
    
    def analyze_code_complexity(self) -> Dict[str, Any]:
        """分析代码复杂度"""
        complexity_report = {}
        
        for py_file in self.project_root.rglob("*.py"):
            try:
                with open(py_file) as f:
                    lines = f.readlines()
                
                complexity_report[str(py_file)] = {
                    'total_lines': len(lines),
                    'code_lines': len([l for l in lines if l.strip() and not l.strip().startswith('#')]),
                    'comment_lines': len([l for l in lines if l.strip().startswith('#')]),
                    'blank_lines': len([l for l in lines if not l.strip()])
                }
            except Exception as e:
                complexity_report[str(py_file)] = {'error': str(e)}
        
        return complexity_report

class DevelopmentWorkflow:
    """开发工作流管理"""
    
    def __init__(self, project_root: str = "."):
        self.project_root = Path(project_root)
        self.logger = self._setup_logging()
    
    def _setup_logging(self) -> logging.Logger:
        """设置日志"""
        logger = logging.getLogger('dev_tools')
        logger.setLevel(logging.INFO)
        
        handler = logging.StreamHandler()
        formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
        handler.setFormatter(formatter)
        logger.addHandler(handler)
        
        return logger
    
    def setup_pre_commit_hooks(self) -> bool:
        """设置pre-commit钩子"""
        pre_commit_config = """
repos:
  - repo: https://github.com/psf/black
    rev: 22.3.0
    hooks:
      - id: black
        language_version: python3
  
  - repo: https://github.com/pycqa/isort
    rev: 5.10.1
    hooks:
      - id: isort
  
  - repo: https://github.com/pycqa/flake8
    rev: 4.0.1
    hooks:
      - id: flake8
  
  - repo: https://github.com/pre-commit/mirrors-mypy
    rev: v0.950
    hooks:
      - id: mypy
"""
        
        try:
            config_path = self.project_root / ".pre-commit-config.yaml"
            with open(config_path, 'w') as f:
                f.write(pre_commit_config)
            
            # 安装pre-commit
            subprocess.run([sys.executable, '-m', 'pip', 'install', 'pre-commit'], 
                         check=True)
            subprocess.run(['pre-commit', 'install'], check=True)
            
            self.logger.info("Pre-commit钩子设置完成")
            return True
        except Exception as e:
            self.logger.error(f"设置pre-commit钩子失败: {e}")
            return False
    
    def create_project_structure(self, project_name: str) -> bool:
        """创建项目结构"""
        try:
            project_dir = self.project_root / project_name
            project_dir.mkdir(exist_ok=True)
            
            # 创建目录结构
            directories = [
                'src', 'tests', 'docs', 'scripts', '.github/workflows'
            ]
            
            for dir_name in directories:
                (project_dir / dir_name).mkdir(parents=True, exist_ok=True)
            
            # 创建基本文件
            self._create_setup_files(project_dir, project_name)
            self._create_test_files(project_dir, project_name)
            self._create_documentation(project_dir, project_name)
            
            self.logger.info(f"项目结构创建完成: {project_dir}")
            return True
        except Exception as e:
            self.logger.error(f"创建项目结构失败: {e}")
            return False
    
    def _create_setup_files(self, project_dir: Path, project_name: str):
        """创建配置文件"""
        
        # pyproject.toml
        pyproject_content = f"""[build-system]
requires = ["setuptools>=45", "wheel"]
build-backend = "setuptools.build_meta"

[project]
name = "{project_name}"
version = "0.1.0"
description = "A Python project"
authors = [{{name = "Developer", email = "dev@example.com"}}]
license = {{text = "MIT"}}
readme = "README.md"
requires-python = ">=3.8"
classifiers = [
    "Development Status :: 3 - Alpha",
    "Intended Audience :: Developers",
    "License :: OSI Approved :: MIT License",
    "Programming Language :: Python :: 3",
    "Programming Language :: Python :: 3.8",
    "Programming Language :: Python :: 3.9",
    "Programming Language :: Python :: 3.10",
    "Programming Language :: Python :: 3.11",
]
dependencies = []

[project.optional-dependencies]
dev = [
    "pytest>=6.0",
    "pytest-cov>=2.0",
    "black",
    "isort",
    "flake8",
    "mypy",
]

[project.scripts]
{project_name} = "{project_name}.__main__:main"

[tool.setuptools.packages.find]
where = ["src"]

[tool.black]
line-length = 88
target-version = ['py38']

[tool.isort]
profile = "black"
multi_line_output = 3

[tool.mypy]
python_version = "3.8"
warn_return_any = true
warn_unused_configs = true
disallow_untyped_defs = true

[tool.pytest.ini_options]
testpaths = ["tests"]
python_files = ["test_*.py"]
python_classes = ["Test*"]
python_functions = ["test_*"]
addopts = "--cov=src --cov-report=html --cov-report=term-missing"
"""
        
        with open(project_dir / "pyproject.toml", 'w') as f:
            f.write(pyproject_content)
        
        # README.md
        readme_content = f"""# {project_name}

A Python project with modern development tools.

## 快速开始

### 安装
```bash
pip install -e .
```

### 开发环境设置
```bash
pip install -e ".[dev]"
pre-commit install
```

### 运行测试
```bash
pytest
```

## 项目结构
```
{project_name}/
├── src/{project_name}/     # 源代码
├── tests/                # 测试文件
├── docs/                 # 文档
├── scripts/              # 脚本文件
└── .github/workflows/    # CI/CD配置
```
"""
        
        with open(project_dir / "README.md", 'w') as f:
            f.write(readme_content)
    
    def _create_test_files(self, project_dir: Path, project_name: str):
        """创建测试文件"""
        
        # __init__.py
        init_content = f'"""{project_name} package."""\n'
        with open(project_dir / "src" / project_name / "__init__.py", 'w') as f:
            f.write(init_content)
        
        # __main__.py
        main_content = f'''"""Main module for {project_name}."""

def main():
    """Main function."""
    print("Hello from {project_name}!")

if __name__ == "__main__":
    main()
'''
        with open(project_dir / "src" / project_name / "__main__.py", 'w') as f:
            f.write(main_content)
        
        # 测试文件
        test_content = f'''"""Tests for {project_name}."""
import pytest
from {project_name} import __version__

def test_version():
    """Test version is defined."""
    assert __version__ is not None

def test_import():
    """Test package can be imported."""
    import {project_name}
    assert {project_name} is not None
'''
        with open(project_dir / "tests" / "test_basic.py", 'w') as f:
            f.write(test_content)
    
    def _create_documentation(self, project_dir: Path, project_name: str):
        """创建文档"""
        
        # GitHub Actions workflow
        workflow_content = f'''name: CI

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  test:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        python-version: [3.8, 3.9, "3.10", "3.11"]

    steps:
    - uses: actions/checkout@v3
    
    - name: Set up Python ${{ matrix.python-version }}
      uses: actions/setup-python@v3
      with:
        python-version: ${{ matrix.python-version }}
    
    - name: Install dependencies
      run: |
        python -m pip install --upgrade pip
        pip install -e ".[dev]"
    
    - name: Lint with flake8
      run: |
        flake8 src tests
    
    - name: Test with pytest
      run: |
        pytest
'''
        
        workflow_dir = project_dir / ".github" / "workflows"
        workflow_dir.mkdir(parents=True, exist_ok=True)
        
        with open(workflow_dir / "ci.yml", 'w') as f:
            f.write(workflow_content)

class PerformanceProfiler:
    """性能分析工具"""
    
    def __init__(self):
        self.profilers = {
            'cprofile': self._run_cprofile,
            'line_profiler': self._run_line_profiler,
            'memory_profiler': self._run_memory_profiler,
            'pyspy': self._run_pyspy
        }
    
    def _run_cprofile(self, script: str) -> Dict[str, Any]:
        """运行cProfile"""
        try:
            cmd = [sys.executable, '-m', 'cProfile', '-s', 'cumulative', script]
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            return {
                'profiler': 'cProfile',
                'success': result.returncode == 0,
                'output': result.stdout,
                'error': result.stderr
            }
        except Exception as e:
            return {'error': str(e)}
    
    def _run_line_profiler(self, script: str, function: str) -> Dict[str, Any]:
        """运行line_profiler"""
        try:
            # 需要安装line_profiler
            if not shutil.which('kernprof'):
                return {'error': 'line_profiler未安装'}
            
            cmd = ['kernprof', '-l', '-v', script]
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            return {
                'profiler': 'line_profiler',
                'success': result.returncode == 0,
                'output': result.stdout,
                'error': result.stderr
            }
        except Exception as e:
            return {'error': str(e)}
    
    def profile_function(self, func, *args, **kwargs) -> Dict[str, Any]:
        """分析函数性能"""
        import cProfile
        import pstats
        import io
        
        pr = cProfile.Profile()
        pr.enable()
        
        result = func(*args, **kwargs)
        
        pr.disable()
        
        s = io.StringIO()
        ps = pstats.Stats(pr, stream=s).sort_stats('cumulative')
        ps.print_stats(10)
        
        return {
            'result': result,
            'profile_output': s.getvalue()
        }

def main():
    """主函数"""
    print("=== Python开发工具链深度研究 ===\n")
    
    # 1. 构建系统分析
    print("[1] 构建系统分析")
    analyzer = BuildSystemAnalyzer()
    build_info = analyzer.detect_build_system()
    print(f"项目根目录: {build_info['project_root']}")
    print(f"主要构建系统: {build_info['primary_system']}")
    print(f"检测到的系统: {list(build_info['build_systems'].keys())}")
    
    # 2. 包管理器检查
    print("\n[2] 包管理器检查")
    pkg_manager = PackageManager()
    for manager, available in pkg_manager.managers.items():
        print(f"{manager}: {'✓' if available else '✗'}")
    
    # 3. 测试框架检测
    print("\n[3] 测试框架检测")
    test_framework = TestFramework()
    test_info = test_framework.detect_test_framework()
    print(f"可用测试框架: {test_info['available_frameworks']}")
    print(f"测试文件: {len(test_info['test_files'])}")
    
    # 4. 代码质量分析
    print("\n[4] 代码质量分析")
    quality_analyzer = CodeQualityAnalyzer()
    
    # 检查linters
    linters = ['flake8', 'black', 'isort', 'mypy']
    for linter in linters:
        result = quality_analyzer.run_linter(linter)
        status = '✓' if result.get('success', False) else '✗'
        print(f"{linter}: {status}")
    
    # 5. 创建示例项目
    print("\n[5] 创建示例项目")
    workflow = DevelopmentWorkflow()
    workflow.create_project_structure("example_project")
    
    # 6. 性能分析演示
    print("\n[6] 性能分析演示")
    profiler = PerformanceProfiler()
    
    def sample_function():
        """示例函数用于性能分析"""
        total = 0
        for i in range(100000):
            total += i * i
        return total
    
    profile_result = profiler.profile_function(sample_function)
    print("性能分析结果:")
    print(profile_result['profile_output'][:300] + "...")
    
    print("\n=== 研究结论 ===")
    print("1. 使用pyproject.toml统一配置构建系统")
    print("2. 集成pre-commit保证代码质量")
    print("3. 使用pytest进行单元测试")
    print("4. 采用black和isort统一代码格式")
    print("5. 使用mypy进行类型检查")
    print("6. 集成GitHub Actions进行CI/CD")
    print("7. 使用性能分析工具优化代码")
    print("8. 维护良好的项目文档")

if __name__ == "__main__":
    main()