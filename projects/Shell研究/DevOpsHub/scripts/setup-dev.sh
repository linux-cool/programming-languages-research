#!/bin/bash
# DevOpsHub 开发环境一键安装脚本

set -euo pipefail

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_debug() {
    [[ "${DEBUG:-false}" == "true" ]] && echo -e "${BLUE}[DEBUG]${NC} $1"
}

# 检测操作系统
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if command -v apt-get &> /dev/null; then
            echo "ubuntu"
        elif command -v yum &> /dev/null; then
            echo "centos"
        else
            echo "linux"
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    else
        echo "unknown"
    fi
}

# 检查命令是否存在
command_exists() {
    command -v "$1" &> /dev/null
}

# 安装Ubuntu/Debian依赖
install_ubuntu_deps() {
    log_info "安装Ubuntu/Debian依赖..."
    
    sudo apt update
    sudo apt install -y \
        git curl wget \
        jq sqlite3 \
        htop tree \
        build-essential \
        ca-certificates \
        gnupg \
        lsb-release \
        shellcheck
    
    # 安装Docker
    if ! command_exists docker; then
        log_info "安装Docker..."
        curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg
        echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
        sudo apt update
        sudo apt install -y docker-ce docker-ce-cli containerd.io docker-compose-plugin
        sudo usermod -aG docker "$USER"
        log_warn "请重新登录以使Docker权限生效"
    fi
    
    # 安装bats测试框架
    if ! command_exists bats; then
        log_info "安装bats测试框架..."
        git clone https://github.com/bats-core/bats-core.git /tmp/bats-core
        cd /tmp/bats-core
        sudo ./install.sh /usr/local
        cd - > /dev/null
        rm -rf /tmp/bats-core
    fi
}

# 安装CentOS/RHEL依赖
install_centos_deps() {
    log_info "安装CentOS/RHEL依赖..."
    
    sudo yum install -y epel-release
    sudo yum install -y \
        git curl wget \
        jq sqlite \
        htop tree \
        gcc gcc-c++ make \
        yum-utils \
        device-mapper-persistent-data \
        lvm2 \
        ShellCheck
    
    # 安装Docker
    if ! command_exists docker; then
        log_info "安装Docker..."
        sudo yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo
        sudo yum install -y docker-ce docker-ce-cli containerd.io docker-compose-plugin
        sudo systemctl start docker
        sudo systemctl enable docker
        sudo usermod -aG docker "$USER"
        log_warn "请重新登录以使Docker权限生效"
    fi
}

# 安装macOS依赖
install_macos_deps() {
    log_info "安装macOS依赖..."
    
    # 检查Homebrew
    if ! command_exists brew; then
        log_info "安装Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi
    
    # 安装依赖
    brew install \
        git curl wget \
        jq sqlite \
        htop tree \
        docker docker-compose \
        shellcheck \
        bats-core
}

# 创建项目结构
create_project_structure() {
    log_info "创建项目目录结构..."
    
    # 创建目录
    mkdir -p {bin,lib/{core,utils,plugins},config/{environments,templates},modules/{deployment,monitoring,backup,cicd,container,security},data/{logs,cache,db},tests/{unit,integration,fixtures},docs/{api,user-guide,developer-guide},scripts}
    
    # 创建.gitignore
    cat > .gitignore << 'EOF'
# 数据文件
data/logs/*
data/cache/*
data/db/*
!data/logs/.gitkeep
!data/cache/.gitkeep
!data/db/.gitkeep

# 临时文件
*.tmp
*.log
*.pid

# 备份文件
*.bak
*~

# IDE文件
.vscode/
.idea/
*.swp
*.swo

# 系统文件
.DS_Store
Thumbs.db
EOF
    
    # 创建空的.gitkeep文件
    touch data/logs/.gitkeep
    touch data/cache/.gitkeep
    touch data/db/.gitkeep
}

# 创建配置文件
create_config_files() {
    log_info "创建配置文件..."
    
    # 默认配置
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

[deployment]
strategy = "rolling"
timeout = 300
health_check_interval = 30

[monitoring]
interval = 60
retention_days = 30
alert_channels = "email,slack"

[backup]
schedule = "0 2 * * *"
retention_days = 30
compression = true
EOF
    
    # 开发环境配置
    cat > config/environments/development.conf << 'EOF'
[system]
log_level = "DEBUG"

[api]
host = "0.0.0.0"
port = 8080

[database]
path = "./data/db/devopshub-dev.db"

[monitoring]
interval = 30
EOF
    
    # 生产环境配置
    cat > config/environments/production.conf << 'EOF'
[system]
log_level = "WARN"

[api]
host = "0.0.0.0"
port = 80

[database]
path = "/var/lib/devopshub/devopshub.db"

[logging]
file = "/var/log/devopshub/system.log"

[monitoring]
interval = 60
retention_days = 90
EOF
}

# 创建核心脚本
create_core_scripts() {
    log_info "创建核心脚本..."
    
    # 主入口脚本
    cat > bin/devops-hub << 'EOF'
#!/bin/bash
# DevOpsHub 主入口脚本

set -euo pipefail

# 项目根目录
DEVOPS_HOME="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export DEVOPS_HOME

# 检查核心文件是否存在
if [[ ! -f "$DEVOPS_HOME/lib/core/bootstrap.sh" ]]; then
    echo "错误: 核心文件不存在，请运行 ./scripts/setup-dev.sh 初始化项目"
    exit 1
fi

# 加载核心库
source "$DEVOPS_HOME/lib/core/bootstrap.sh"

# 显示帮助信息
show_help() {
    cat << 'HELP'
DevOpsHub - 企业级系统运维自动化平台

用法:
    devops-hub <command> [options] [arguments]

命令:
    init                初始化系统
    status              查看系统状态
    deploy              部署管理
    monitor             监控管理
    backup              备份管理
    config              配置管理
    help                显示帮助信息
    version             显示版本信息

全局选项:
    -c, --config FILE   指定配置文件
    -v, --verbose       详细输出模式
    -q, --quiet         静默模式
    -h, --help          显示帮助信息

示例:
    devops-hub init
    devops-hub status
    devops-hub deploy create web-app --version 1.0.0
    devops-hub monitor start
    devops-hub backup create daily-backup

更多信息请访问: https://github.com/your-repo/devopshub
HELP
}

# 显示版本信息
show_version() {
    echo "DevOpsHub v${DEVOPS_VERSION:-1.0.0}"
    echo "Shell-based DevOps automation platform"
}

# 主函数
main() {
    local command="${1:-help}"
    shift || true
    
    case "$command" in
        "init")
            log_info "初始化DevOpsHub系统..."
            log_info "✅ 系统初始化完成！"
            ;;
        "status")
            log_info "DevOpsHub系统状态:"
            echo "  版本: ${DEVOPS_VERSION:-1.0.0}"
            echo "  状态: 运行中"
            echo "  配置: $(get_config "system.name" "DevOpsHub")"
            echo "  日志级别: $(get_config "system.log_level" "INFO")"
            ;;
        "deploy")
            log_info "部署管理功能开发中..."
            ;;
        "monitor")
            log_info "监控管理功能开发中..."
            ;;
        "backup")
            log_info "备份管理功能开发中..."
            ;;
        "config")
            log_info "配置管理功能开发中..."
            ;;
        "help"|"-h"|"--help")
            show_help
            ;;
        "version"|"-v"|"--version")
            show_version
            ;;
        *)
            log_error "未知命令: '$command'"
            echo "使用 'devops-hub help' 查看帮助"
            exit 1
            ;;
    esac
}

# 执行主函数
main "$@"
EOF
    
    chmod +x bin/devops-hub
}

# 创建核心库文件
create_core_libs() {
    log_info "创建核心库文件..."
    
    # 引导脚本
    cat > lib/core/bootstrap.sh << 'EOF'
#!/bin/bash
# DevOpsHub 核心引导脚本

# 设置错误处理
set -euo pipefail

# 全局变量
declare -g DEVOPS_VERSION="1.0.0"
declare -g DEVOPS_CONFIG_FILE="${DEVOPS_CONFIG_FILE:-$DEVOPS_HOME/config/default.conf}"
declare -g DEVOPS_LOG_LEVEL="${DEVOPS_LOG_LEVEL:-INFO}"
declare -g DEVOPS_ENV="${DEVOPS_ENV:-production}"

# 加载核心模块
source "$DEVOPS_HOME/lib/core/config.sh"
source "$DEVOPS_HOME/lib/core/logging.sh"

# 加载工具库
source "$DEVOPS_HOME/lib/utils/common.sh"

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
    
    # 加载环境特定配置
    local env_config="$DEVOPS_HOME/config/environments/$DEVOPS_ENV.conf"
    if [[ -f "$env_config" ]]; then
        load_config "$env_config"
    fi
    
    log_debug "DevOpsHub v$DEVOPS_VERSION 初始化完成 (环境: $DEVOPS_ENV)"
}

# 自动初始化
init_system
EOF
    
    # 配置管理
    cat > lib/core/config.sh << 'EOF'
#!/bin/bash
# 配置管理模块

declare -A CONFIG_CACHE

# 加载配置文件
load_config() {
    local config_file="$1"
    
    if [[ ! -f "$config_file" ]]; then
        echo "警告: 配置文件不存在: $config_file" >&2
        return 1
    fi
    
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
}

# 获取配置值
get_config() {
    local key="$1"
    local default_value="${2:-}"
    
    echo "${CONFIG_CACHE[$key]:-$default_value}"
}
EOF
    
    # 日志模块
    cat > lib/core/logging.sh << 'EOF'
#!/bin/bash
# 日志管理模块

# 日志级别
declare -g LOG_LEVEL_DEBUG=0
declare -g LOG_LEVEL_INFO=1
declare -g LOG_LEVEL_WARN=2
declare -g LOG_LEVEL_ERROR=3

# 当前日志级别
declare -g CURRENT_LOG_LEVEL=1

# 颜色定义
declare -g LOG_COLOR_DEBUG='\033[0;36m'
declare -g LOG_COLOR_INFO='\033[0;32m'
declare -g LOG_COLOR_WARN='\033[1;33m'
declare -g LOG_COLOR_ERROR='\033[0;31m'
declare -g LOG_COLOR_RESET='\033[0m'

# 初始化日志系统
init_logging() {
    case "${DEVOPS_LOG_LEVEL:-INFO}" in
        "DEBUG") CURRENT_LOG_LEVEL=$LOG_LEVEL_DEBUG ;;
        "INFO")  CURRENT_LOG_LEVEL=$LOG_LEVEL_INFO ;;
        "WARN")  CURRENT_LOG_LEVEL=$LOG_LEVEL_WARN ;;
        "ERROR") CURRENT_LOG_LEVEL=$LOG_LEVEL_ERROR ;;
    esac
}

# 通用日志函数
log_message() {
    local level="$1"
    local level_num="$2"
    local color="$3"
    local message="$4"
    
    if (( level_num >= CURRENT_LOG_LEVEL )); then
        local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
        echo -e "${color}[$timestamp] [$level]${LOG_COLOR_RESET} $message" >&2
    fi
}

# 具体日志函数
log_debug() {
    log_message "DEBUG" $LOG_LEVEL_DEBUG "$LOG_COLOR_DEBUG" "$1"
}

log_info() {
    log_message "INFO" $LOG_LEVEL_INFO "$LOG_COLOR_INFO" "$1"
}

log_warn() {
    log_message "WARN" $LOG_LEVEL_WARN "$LOG_COLOR_WARN" "$1"
}

log_error() {
    log_message "ERROR" $LOG_LEVEL_ERROR "$LOG_COLOR_ERROR" "$1"
}
EOF
    
    # 通用工具
    cat > lib/utils/common.sh << 'EOF'
#!/bin/bash
# 通用工具函数

# 检查命令是否存在
command_exists() {
    command -v "$1" &> /dev/null
}

# 检查文件是否存在且可读
file_readable() {
    [[ -f "$1" && -r "$1" ]]
}

# 检查目录是否存在且可写
dir_writable() {
    [[ -d "$1" && -w "$1" ]]
}

# 获取当前时间戳
get_timestamp() {
    date '+%Y%m%d_%H%M%S'
}

# 人性化文件大小
human_size() {
    local size="$1"
    local units=("B" "KB" "MB" "GB" "TB")
    local unit=0
    
    while (( size > 1024 && unit < ${#units[@]} - 1 )); do
        size=$((size / 1024))
        ((unit++))
    done
    
    echo "${size}${units[$unit]}"
}
EOF
}

# 创建测试文件
create_test_files() {
    log_info "创建测试文件..."
    
    # 单元测试示例
    cat > tests/unit/config.bats << 'EOF'
#!/usr/bin/env bats

# 加载测试辅助函数
load '../test_helper'

setup() {
    # 设置测试环境
    export DEVOPS_HOME="$(pwd)"
    source lib/core/config.sh
}

@test "load_config should load configuration file" {
    # 创建临时配置文件
    cat > /tmp/test.conf << 'CONF'
[test]
key1 = value1
key2 = "value2"
CONF
    
    # 加载配置
    run load_config /tmp/test.conf
    [ "$status" -eq 0 ]
    
    # 验证配置值
    result=$(get_config "test.key1")
    [ "$result" = "value1" ]
    
    result=$(get_config "test.key2")
    [ "$result" = "value2" ]
    
    # 清理
    rm -f /tmp/test.conf
}

@test "get_config should return default value for missing key" {
    result=$(get_config "missing.key" "default")
    [ "$result" = "default" ]
}
EOF
    
    # 测试辅助函数
    cat > tests/test_helper.bash << 'EOF'
#!/bin/bash
# 测试辅助函数

# 设置测试环境
setup_test_env() {
    export DEVOPS_HOME="$(pwd)"
    export DEVOPS_ENV="test"
    export DEVOPS_LOG_LEVEL="DEBUG"
}

# 清理测试环境
cleanup_test_env() {
    unset DEVOPS_HOME
    unset DEVOPS_ENV
    unset DEVOPS_LOG_LEVEL
}
EOF
}

# 主函数
main() {
    local os_type
    os_type=$(detect_os)
    
    echo "=== DevOpsHub 开发环境安装 ==="
    log_info "检测到操作系统: $os_type"
    
    # 安装系统依赖
    case "$os_type" in
        "ubuntu")
            install_ubuntu_deps
            ;;
        "centos")
            install_centos_deps
            ;;
        "macos")
            install_macos_deps
            ;;
        *)
            log_error "不支持的操作系统: $os_type"
            exit 1
            ;;
    esac
    
    # 创建项目结构
    create_project_structure
    create_config_files
    create_core_scripts
    create_core_libs
    create_test_files
    
    log_info "✅ DevOpsHub开发环境安装完成！"
    echo
    echo "下一步:"
    echo "1. 运行 './bin/devops-hub status' 检查系统状态"
    echo "2. 运行 './bin/devops-hub help' 查看可用命令"
    echo "3. 查看 DEVELOPMENT.md 了解开发指南"
    echo
    echo "开始开发:"
    echo "  export DEVOPS_ENV=development"
    echo "  ./bin/devops-hub init"
}

# 执行主函数
main "$@"
