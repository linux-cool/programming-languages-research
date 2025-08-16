# DevOpsHub 开发环境搭建指南

## 1. 开发环境要求

### 1.1 系统要求
- **操作系统**: Ubuntu 18.04+, CentOS 7+, RHEL 8+, macOS 10.15+
- **Shell**: Bash 4.0+ 或 Zsh 5.0+
- **内存**: 最少4GB，推荐8GB+
- **磁盘**: 最少20GB可用空间
- **网络**: 稳定的互联网连接

### 1.2 必需工具
```bash
# 基础工具
- git (版本控制)
- curl/wget (网络请求)
- jq (JSON处理)
- sqlite3 (数据库)
- docker (容器支持)
- docker-compose (容器编排)

# 开发工具
- shellcheck (Shell脚本静态分析)
- bats (Bash测试框架)
- shfmt (Shell代码格式化)
- tree (目录结构显示)

# 监控工具
- htop (系统监控)
- iostat (IO监控)
- netstat (网络监控)
- ss (网络连接)
```

### 1.3 可选工具
```bash
# 编辑器和IDE
- vim/neovim (命令行编辑器)
- VS Code (图形界面编辑器)
- IntelliJ IDEA (集成开发环境)

# 调试工具
- strace (系统调用跟踪)
- ltrace (库调用跟踪)
- gdb (调试器)
```

## 2. 环境安装脚本

### 2.1 Ubuntu/Debian 环境安装
```bash
#!/bin/bash
# setup-ubuntu.sh

set -euo pipefail

echo "=== DevOpsHub 开发环境安装 (Ubuntu/Debian) ==="

# 更新包管理器
sudo apt update

# 安装基础工具
sudo apt install -y \
    git curl wget \
    jq sqlite3 \
    htop tree \
    build-essential \
    ca-certificates \
    gnupg \
    lsb-release

# 安装Docker
if ! command -v docker &> /dev/null; then
    echo "安装Docker..."
    curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
    sudo apt update
    sudo apt install -y docker-ce docker-ce-cli containerd.io docker-compose-plugin
    sudo usermod -aG docker $USER
fi

# 安装开发工具
echo "安装开发工具..."
sudo apt install -y shellcheck

# 安装bats测试框架
if ! command -v bats &> /dev/null; then
    git clone https://github.com/bats-core/bats-core.git /tmp/bats-core
    cd /tmp/bats-core
    sudo ./install.sh /usr/local
    cd -
    rm -rf /tmp/bats-core
fi

# 安装shfmt
if ! command -v shfmt &> /dev/null; then
    GO_VERSION="1.19"
    wget -O /tmp/go.tar.gz "https://golang.org/dl/go${GO_VERSION}.linux-amd64.tar.gz"
    sudo tar -C /usr/local -xzf /tmp/go.tar.gz
    export PATH=$PATH:/usr/local/go/bin
    go install mvdan.cc/sh/v3/cmd/shfmt@latest
    sudo cp ~/go/bin/shfmt /usr/local/bin/
fi

echo "✅ 开发环境安装完成！"
echo "请重新登录以使Docker权限生效"
```

### 2.2 CentOS/RHEL 环境安装
```bash
#!/bin/bash
# setup-centos.sh

set -euo pipefail

echo "=== DevOpsHub 开发环境安装 (CentOS/RHEL) ==="

# 安装EPEL仓库
sudo yum install -y epel-release

# 安装基础工具
sudo yum install -y \
    git curl wget \
    jq sqlite \
    htop tree \
    gcc gcc-c++ make \
    yum-utils \
    device-mapper-persistent-data \
    lvm2

# 安装Docker
if ! command -v docker &> /dev/null; then
    echo "安装Docker..."
    sudo yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo
    sudo yum install -y docker-ce docker-ce-cli containerd.io docker-compose-plugin
    sudo systemctl start docker
    sudo systemctl enable docker
    sudo usermod -aG docker $USER
fi

# 安装ShellCheck
sudo yum install -y ShellCheck

echo "✅ 开发环境安装完成！"
```

### 2.3 macOS 环境安装
```bash
#!/bin/bash
# setup-macos.sh

set -euo pipefail

echo "=== DevOpsHub 开发环境安装 (macOS) ==="

# 检查Homebrew
if ! command -v brew &> /dev/null; then
    echo "安装Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

# 安装基础工具
brew install \
    git curl wget \
    jq sqlite \
    htop tree \
    docker docker-compose \
    shellcheck \
    bats-core \
    shfmt

echo "✅ 开发环境安装完成！"
```

## 3. 项目初始化

### 3.1 克隆项目
```bash
# 克隆项目到本地
git clone https://github.com/your-repo/devopshub.git
cd devopshub

# 或者创建新项目
mkdir devopshub && cd devopshub
git init
```

### 3.2 创建项目结构
```bash
#!/bin/bash
# init-project.sh

echo "=== 初始化DevOpsHub项目结构 ==="

# 创建目录结构
mkdir -p {bin,lib/{core,utils,plugins},config/{environments,templates},modules/{deployment,monitoring,backup,cicd,container,security},data/{logs,cache,db},tests/{unit,integration,fixtures},docs/{api,user-guide,developer-guide},scripts}

# 创建主要配置文件
cat > config/default.conf << 'EOF'
# DevOpsHub 默认配置文件

[system]
name = "DevOpsHub"
version = "1.0.0"
log_level = "INFO"
data_dir = "./data"

[api]
host = "localhost"
port = 8080
timeout = 30

[database]
type = "sqlite"
path = "./data/db/devopshub.db"

[logging]
level = "INFO"
file = "./data/logs/system.log"
max_size = "100MB"
max_files = 10
EOF

# 创建环境配置
cat > config/environments/development.conf << 'EOF'
[system]
log_level = "DEBUG"

[api]
host = "0.0.0.0"
port = 8080

[database]
path = "./data/db/devopshub-dev.db"
EOF

cat > config/environments/production.conf << 'EOF'
[system]
log_level = "WARN"

[api]
host = "0.0.0.0"
port = 80

[database]
path = "/var/lib/devopshub/devopshub.db"
EOF

echo "✅ 项目结构初始化完成！"
```

## 4. 核心框架开发

### 4.1 主入口脚本
```bash
#!/bin/bash
# bin/devops-hub

set -euo pipefail

# 项目根目录
DEVOPS_HOME="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export DEVOPS_HOME

# 加载核心库
source "$DEVOPS_HOME/lib/core/bootstrap.sh"

# 主函数
main() {
    local command="${1:-help}"
    shift || true
    
    case "$command" in
        "init")
            cmd_init "$@"
            ;;
        "deploy")
            cmd_deploy "$@"
            ;;
        "monitor")
            cmd_monitor "$@"
            ;;
        "backup")
            cmd_backup "$@"
            ;;
        "status")
            cmd_status "$@"
            ;;
        "help"|"-h"|"--help")
            cmd_help
            ;;
        "version"|"-v"|"--version")
            cmd_version
            ;;
        *)
            echo "错误: 未知命令 '$command'"
            echo "使用 'devops-hub help' 查看帮助"
            exit 1
            ;;
    esac
}

# 执行主函数
main "$@"
```

### 4.2 核心引导脚本
```bash
#!/bin/bash
# lib/core/bootstrap.sh

# 设置错误处理
set -euo pipefail

# 全局变量
declare -g DEVOPS_VERSION="1.0.0"
declare -g DEVOPS_CONFIG_FILE="${DEVOPS_CONFIG_FILE:-$DEVOPS_HOME/config/default.conf}"
declare -g DEVOPS_LOG_LEVEL="${DEVOPS_LOG_LEVEL:-INFO}"

# 加载核心模块
source "$DEVOPS_HOME/lib/core/config.sh"
source "$DEVOPS_HOME/lib/core/logging.sh"
source "$DEVOPS_HOME/lib/core/utils.sh"

# 加载工具库
source "$DEVOPS_HOME/lib/utils/string.sh"
source "$DEVOPS_HOME/lib/utils/file.sh"
source "$DEVOPS_HOME/lib/utils/network.sh"

# 初始化系统
init_system() {
    # 创建必要目录
    mkdir -p "$DEVOPS_HOME/data/logs"
    mkdir -p "$DEVOPS_HOME/data/cache"
    mkdir -p "$DEVOPS_HOME/data/db"
    
    # 初始化日志系统
    init_logging
    
    # 加载配置
    load_config "$DEVOPS_CONFIG_FILE"
    
    log_info "DevOpsHub v$DEVOPS_VERSION 初始化完成"
}

# 自动初始化
init_system
```

### 4.3 配置管理模块
```bash
#!/bin/bash
# lib/core/config.sh

declare -A CONFIG_CACHE

# 加载配置文件
load_config() {
    local config_file="$1"
    
    if [[ ! -f "$config_file" ]]; then
        log_error "配置文件不存在: $config_file"
        return 1
    fi
    
    log_debug "加载配置文件: $config_file"
    
    # 解析INI格式配置文件
    local section=""
    while IFS= read -r line; do
        # 跳过注释和空行
        [[ "$line" =~ ^[[:space:]]*# ]] && continue
        [[ "$line" =~ ^[[:space:]]*$ ]] && continue
        
        # 解析section
        if [[ "$line" =~ ^\[([^]]+)\] ]]; then
            section="${BASH_REMATCH[1]}"
            continue
        fi
        
        # 解析key=value
        if [[ "$line" =~ ^([^=]+)=(.*)$ ]]; then
            local key="${BASH_REMATCH[1]// /}"
            local value="${BASH_REMATCH[2]}"
            # 移除引号
            value="${value%\"}"
            value="${value#\"}"
            CONFIG_CACHE["${section}.${key}"]="$value"
        fi
    done < "$config_file"
    
    log_debug "配置加载完成，共 ${#CONFIG_CACHE[@]} 项"
}

# 获取配置值
get_config() {
    local key="$1"
    local default_value="${2:-}"
    
    echo "${CONFIG_CACHE[$key]:-$default_value}"
}

# 设置配置值
set_config() {
    local key="$1"
    local value="$2"
    
    CONFIG_CACHE["$key"]="$value"
}
```

## 5. 开发工作流

### 5.1 开发环境启动
```bash
#!/bin/bash
# scripts/dev-start.sh

echo "=== 启动DevOpsHub开发环境 ==="

# 检查依赖
echo "检查系统依赖..."
./scripts/check-deps.sh

# 初始化项目
echo "初始化项目..."
./scripts/init-project.sh

# 启动开发服务
echo "启动开发服务..."
export DEVOPS_ENV="development"
export DEVOPS_LOG_LEVEL="DEBUG"

# 启动API服务器（如果需要）
if [[ "${START_API:-true}" == "true" ]]; then
    ./bin/devops-hub api start --port 8080 &
    API_PID=$!
    echo "API服务器已启动 (PID: $API_PID)"
fi

# 启动监控服务
if [[ "${START_MONITOR:-true}" == "true" ]]; then
    ./bin/devops-hub monitor start &
    MONITOR_PID=$!
    echo "监控服务已启动 (PID: $MONITOR_PID)"
fi

echo "✅ 开发环境启动完成！"
echo "使用 './bin/devops-hub status' 查看状态"
```

### 5.2 代码质量检查
```bash
#!/bin/bash
# scripts/lint.sh

echo "=== 代码质量检查 ==="

# ShellCheck静态分析
echo "运行ShellCheck..."
find . -name "*.sh" -not -path "./tests/*" | xargs shellcheck

# 代码格式检查
echo "检查代码格式..."
find . -name "*.sh" -not -path "./tests/*" | xargs shfmt -d

echo "✅ 代码质量检查完成！"
```

### 5.3 测试运行
```bash
#!/bin/bash
# scripts/test.sh

echo "=== 运行测试套件 ==="

# 运行单元测试
echo "运行单元测试..."
bats tests/unit/*.bats

# 运行集成测试
echo "运行集成测试..."
bats tests/integration/*.bats

# 生成测试报告
echo "生成测试报告..."
./scripts/test-report.sh

echo "✅ 测试完成！"
```

## 6. 调试和故障排除

### 6.1 调试模式
```bash
# 启用调试模式
export DEVOPS_LOG_LEVEL="DEBUG"
export DEVOPS_DEBUG="true"

# 运行命令
./bin/devops-hub deploy create test-app --debug
```

### 6.2 日志查看
```bash
# 查看系统日志
tail -f data/logs/system.log

# 查看错误日志
tail -f data/logs/error.log

# 查看特定模块日志
tail -f data/logs/deployment.log
```

### 6.3 常见问题
```bash
# 权限问题
sudo chown -R $USER:$USER $DEVOPS_HOME
chmod +x bin/devops-hub

# 依赖问题
./scripts/check-deps.sh
./scripts/install-deps.sh

# 配置问题
./bin/devops-hub config validate
./bin/devops-hub config show
```

## 7. 开发工具推荐

### 7.1 VS Code配置
```json
// .vscode/settings.json
{
    "shellcheck.enable": true,
    "shellformat.useEditorConfig": true,
    "files.associations": {
        "*.sh": "shellscript",
        "*.bats": "shellscript"
    },
    "shellformat.flag": "-i 4"
}
```

### 7.2 Git钩子
```bash
#!/bin/bash
# .git/hooks/pre-commit

# 运行代码检查
./scripts/lint.sh

# 运行测试
./scripts/test.sh

echo "✅ 预提交检查通过！"
```

这个开发环境指南提供了完整的环境搭建、项目初始化、开发工作流和调试方法，让您可以快速开始DevOpsHub项目的开发工作。
