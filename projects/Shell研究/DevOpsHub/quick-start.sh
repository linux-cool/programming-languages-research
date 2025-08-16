#!/bin/bash
# DevOpsHub 快速启动脚本

set -euo pipefail

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 显示横幅
show_banner() {
    echo -e "${CYAN}"
    cat << 'EOF'
    ____              ____            _   _       _     
   |  _ \  _____   __/ __ \ _ __  ___| | | |_   _| |__  
   | | | |/ _ \ \ / / |  | | '_ \/ __| |_| | | | | '_ \ 
   | |_| |  __/\ V /| |__| | |_) \__ \  _  | |_| | |_) |
   |____/ \___| \_/  \____/| .__/|___/_| |_|\__,_|_.__/ 
                           |_|                          
EOF
    echo -e "${NC}"
    echo -e "${GREEN}企业级系统运维自动化平台${NC}"
    echo -e "${BLUE}基于Shell脚本的DevOps工具链${NC}"
    echo
}

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

log_step() {
    echo -e "${PURPLE}[STEP]${NC} $1"
}

# 检查系统要求
check_requirements() {
    log_step "检查系统要求..."
    
    local missing_tools=()
    
    # 检查必需工具
    local required_tools=("bash" "git" "curl" "jq")
    for tool in "${required_tools[@]}"; do
        if ! command -v "$tool" &> /dev/null; then
            missing_tools+=("$tool")
        fi
    done
    
    if [[ ${#missing_tools[@]} -gt 0 ]]; then
        log_error "缺少必需工具: ${missing_tools[*]}"
        log_info "请先运行安装脚本: ./scripts/setup-dev.sh"
        return 1
    fi
    
    # 检查Shell版本
    if [[ ${BASH_VERSION%%.*} -lt 4 ]]; then
        log_error "需要Bash 4.0或更高版本，当前版本: $BASH_VERSION"
        return 1
    fi
    
    log_info "✅ 系统要求检查通过"
}

# 初始化项目
init_project() {
    log_step "初始化项目结构..."
    
    # 检查是否已经初始化
    if [[ -f "bin/devops-hub" ]]; then
        log_info "项目已经初始化"
        return 0
    fi
    
    # 运行安装脚本
    if [[ -f "scripts/setup-dev.sh" ]]; then
        log_info "运行开发环境安装脚本..."
        chmod +x scripts/setup-dev.sh
        ./scripts/setup-dev.sh
    else
        log_error "安装脚本不存在: scripts/setup-dev.sh"
        return 1
    fi
}

# 验证安装
verify_installation() {
    log_step "验证安装..."
    
    # 检查主要文件
    local required_files=(
        "bin/devops-hub"
        "lib/core/bootstrap.sh"
        "lib/core/config.sh"
        "lib/core/logging.sh"
        "config/default.conf"
    )
    
    for file in "${required_files[@]}"; do
        if [[ ! -f "$file" ]]; then
            log_error "缺少必需文件: $file"
            return 1
        fi
    done
    
    # 检查可执行权限
    if [[ ! -x "bin/devops-hub" ]]; then
        log_warn "修复可执行权限..."
        chmod +x bin/devops-hub
    fi
    
    log_info "✅ 安装验证通过"
}

# 运行基本测试
run_basic_tests() {
    log_step "运行基本测试..."
    
    # 测试主命令
    if ./bin/devops-hub version &> /dev/null; then
        log_info "✅ 主命令测试通过"
    else
        log_error "主命令测试失败"
        return 1
    fi
    
    # 测试配置加载
    if ./bin/devops-hub status &> /dev/null; then
        log_info "✅ 配置加载测试通过"
    else
        log_error "配置加载测试失败"
        return 1
    fi
    
    # 运行单元测试（如果存在）
    if command -v bats &> /dev/null && [[ -d "tests/unit" ]]; then
        log_info "运行单元测试..."
        if bats tests/unit/*.bats 2>/dev/null; then
            log_info "✅ 单元测试通过"
        else
            log_warn "单元测试失败，但不影响基本功能"
        fi
    fi
}

# 显示使用指南
show_usage_guide() {
    echo
    echo -e "${CYAN}=== 使用指南 ===${NC}"
    echo
    echo -e "${GREEN}基本命令:${NC}"
    echo "  ./bin/devops-hub help      # 显示帮助信息"
    echo "  ./bin/devops-hub version   # 显示版本信息"
    echo "  ./bin/devops-hub status    # 查看系统状态"
    echo "  ./bin/devops-hub init      # 初始化系统"
    echo
    echo -e "${GREEN}开发模式:${NC}"
    echo "  export DEVOPS_ENV=development"
    echo "  export DEVOPS_LOG_LEVEL=DEBUG"
    echo "  ./bin/devops-hub status"
    echo
    echo -e "${GREEN}配置文件:${NC}"
    echo "  config/default.conf                    # 默认配置"
    echo "  config/environments/development.conf   # 开发环境配置"
    echo "  config/environments/production.conf    # 生产环境配置"
    echo
    echo -e "${GREEN}日志文件:${NC}"
    echo "  data/logs/system.log       # 系统日志"
    echo "  data/logs/error.log        # 错误日志"
    echo
    echo -e "${GREEN}开发文档:${NC}"
    echo "  README.md                  # 项目概述"
    echo "  DEVELOPMENT.md             # 开发指南"
    echo "  REQUIREMENTS.md            # 需求文档"
    echo "  ARCHITECTURE.md            # 架构文档"
    echo "  API_DESIGN.md              # API设计"
    echo
}

# 显示项目状态
show_project_status() {
    echo -e "${CYAN}=== 项目状态 ===${NC}"
    echo
    
    # 显示版本信息
    if [[ -f "bin/devops-hub" ]]; then
        echo -e "${GREEN}版本信息:${NC}"
        ./bin/devops-hub version
        echo
    fi
    
    # 显示目录结构
    echo -e "${GREEN}项目结构:${NC}"
    if command -v tree &> /dev/null; then
        tree -L 2 -I 'data'
    else
        find . -maxdepth 2 -type d | sort
    fi
    echo
    
    # 显示配置信息
    echo -e "${GREEN}当前配置:${NC}"
    echo "  环境: ${DEVOPS_ENV:-production}"
    echo "  日志级别: ${DEVOPS_LOG_LEVEL:-INFO}"
    echo "  配置文件: ${DEVOPS_CONFIG_FILE:-config/default.conf}"
    echo
}

# 交互式演示
interactive_demo() {
    echo -e "${CYAN}=== 交互式演示 ===${NC}"
    echo
    
    while true; do
        echo -e "${YELLOW}选择要演示的功能:${NC}"
        echo "1) 查看系统状态"
        echo "2) 查看配置信息"
        echo "3) 测试日志功能"
        echo "4) 查看帮助信息"
        echo "5) 退出演示"
        echo
        read -p "请选择 (1-5): " choice
        
        case $choice in
            1)
                echo -e "${GREEN}=== 系统状态 ===${NC}"
                ./bin/devops-hub status
                ;;
            2)
                echo -e "${GREEN}=== 配置信息 ===${NC}"
                if [[ -f "config/default.conf" ]]; then
                    cat config/default.conf
                else
                    echo "配置文件不存在"
                fi
                ;;
            3)
                echo -e "${GREEN}=== 日志测试 ===${NC}"
                export DEVOPS_LOG_LEVEL=DEBUG
                ./bin/devops-hub status
                ;;
            4)
                echo -e "${GREEN}=== 帮助信息 ===${NC}"
                ./bin/devops-hub help
                ;;
            5)
                echo "退出演示"
                break
                ;;
            *)
                echo "无效选择，请重试"
                ;;
        esac
        echo
        read -p "按回车键继续..."
        echo
    done
}

# 主函数
main() {
    show_banner
    
    # 解析命令行参数
    local demo_mode=false
    local skip_tests=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --demo)
                demo_mode=true
                shift
                ;;
            --skip-tests)
                skip_tests=true
                shift
                ;;
            -h|--help)
                echo "用法: $0 [选项]"
                echo
                echo "选项:"
                echo "  --demo        运行交互式演示"
                echo "  --skip-tests  跳过测试步骤"
                echo "  -h, --help    显示帮助信息"
                exit 0
                ;;
            *)
                log_error "未知选项: $1"
                exit 1
                ;;
        esac
    done
    
    # 执行启动步骤
    if ! check_requirements; then
        exit 1
    fi
    
    if ! init_project; then
        exit 1
    fi
    
    if ! verify_installation; then
        exit 1
    fi
    
    if [[ "$skip_tests" != "true" ]]; then
        if ! run_basic_tests; then
            log_warn "基本测试失败，但项目可能仍然可用"
        fi
    fi
    
    show_project_status
    show_usage_guide
    
    if [[ "$demo_mode" == "true" ]]; then
        interactive_demo
    fi
    
    echo -e "${GREEN}🎉 DevOpsHub 快速启动完成！${NC}"
    echo -e "${BLUE}开始您的DevOps自动化之旅吧！${NC}"
}

# 执行主函数
main "$@"
