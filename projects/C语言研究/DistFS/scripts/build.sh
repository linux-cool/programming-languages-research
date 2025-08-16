#!/bin/bash

# DistFS 构建脚本
# 用于编译和构建分布式文件系统

set -e  # 遇到错误立即退出

# 脚本配置
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
INSTALL_PREFIX="/usr/local"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 显示帮助信息
show_help() {
    cat << EOF
DistFS 构建脚本

用法: $0 [选项]

选项:
    -h, --help              显示此帮助信息
    -c, --clean             清理构建目录
    -d, --debug             调试构建 (Debug模式)
    -r, --release           发布构建 (Release模式，默认)
    -t, --test              运行测试
    -i, --install           安装到系统
    -p, --prefix PREFIX     设置安装前缀 (默认: /usr/local)
    -j, --jobs N            并行编译任务数 (默认: CPU核心数)
    --coverage              启用代码覆盖率
    --sanitizer             启用AddressSanitizer
    --static-analysis       启用静态分析

示例:
    $0                      # 默认Release构建
    $0 -d -t               # Debug构建并运行测试
    $0 -c -r -i            # 清理、Release构建并安装
    $0 --coverage -t       # 启用覆盖率并运行测试

EOF
}

# 检查依赖
check_dependencies() {
    log_info "检查构建依赖..."
    
    local missing_deps=()
    
    # 检查必需工具
    for tool in cmake gcc make pkg-config; do
        if ! command -v "$tool" &> /dev/null; then
            missing_deps+=("$tool")
        fi
    done
    
    # 检查可选工具
    for tool in clang-tidy cppcheck valgrind; do
        if ! command -v "$tool" &> /dev/null; then
            log_warning "可选工具 $tool 未找到"
        fi
    done
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        log_error "缺少必需依赖: ${missing_deps[*]}"
        log_info "请安装缺少的依赖后重试"
        exit 1
    fi
    
    log_success "依赖检查完成"
}

# 清理构建目录
clean_build() {
    log_info "清理构建目录..."
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        log_success "构建目录已清理"
    else
        log_info "构建目录不存在，跳过清理"
    fi
}

# 创建构建目录
create_build_dir() {
    log_info "创建构建目录..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
}

# 配置构建
configure_build() {
    local build_type="$1"
    local cmake_args=()
    
    log_info "配置构建 (类型: $build_type)..."
    
    cmake_args+=("-DCMAKE_BUILD_TYPE=$build_type")
    cmake_args+=("-DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX")
    
    # 添加可选配置
    if [ "$ENABLE_COVERAGE" = "ON" ]; then
        cmake_args+=("-DENABLE_COVERAGE=ON")
        log_info "启用代码覆盖率"
    fi
    
    if [ "$ENABLE_SANITIZER" = "ON" ]; then
        cmake_args+=("-DENABLE_SANITIZER=ON")
        log_info "启用AddressSanitizer"
    fi
    
    if [ "$ENABLE_STATIC_ANALYSIS" = "ON" ]; then
        cmake_args+=("-DENABLE_STATIC_ANALYSIS=ON")
        log_info "启用静态分析"
    fi
    
    # 运行CMake配置
    cmake "${cmake_args[@]}" "$PROJECT_ROOT"
    
    log_success "构建配置完成"
}

# 编译项目
build_project() {
    local jobs="$1"
    
    log_info "开始编译 (并行任务数: $jobs)..."
    
    make -j"$jobs"
    
    log_success "编译完成"
}

# 运行测试
run_tests() {
    log_info "运行测试..."
    
    if [ ! -f "Makefile" ]; then
        log_error "未找到Makefile，请先配置和构建项目"
        return 1
    fi
    
    # 运行CTest
    if command -v ctest &> /dev/null; then
        ctest --output-on-failure --parallel "$JOBS"
    else
        # 手动运行测试
        make test
    fi
    
    log_success "测试完成"
}

# 安装项目
install_project() {
    log_info "安装项目到 $INSTALL_PREFIX..."
    
    if [ "$EUID" -ne 0 ] && [[ "$INSTALL_PREFIX" == /usr* ]]; then
        log_warning "安装到系统目录需要root权限"
        sudo make install
    else
        make install
    fi
    
    log_success "安装完成"
}

# 生成覆盖率报告
generate_coverage_report() {
    if [ "$ENABLE_COVERAGE" != "ON" ]; then
        return 0
    fi
    
    log_info "生成代码覆盖率报告..."
    
    if command -v gcov &> /dev/null && command -v lcov &> /dev/null; then
        # 生成覆盖率数据
        lcov --capture --directory . --output-file coverage.info
        lcov --remove coverage.info '/usr/*' --output-file coverage.info
        lcov --remove coverage.info '*/tests/*' --output-file coverage.info
        
        # 生成HTML报告
        if command -v genhtml &> /dev/null; then
            genhtml coverage.info --output-directory coverage_html
            log_success "覆盖率报告生成完成: coverage_html/index.html"
        fi
    else
        log_warning "未找到gcov或lcov，跳过覆盖率报告生成"
    fi
}

# 显示构建信息
show_build_info() {
    log_info "构建信息:"
    echo "  项目根目录: $PROJECT_ROOT"
    echo "  构建目录: $BUILD_DIR"
    echo "  安装前缀: $INSTALL_PREFIX"
    echo "  并行任务数: $JOBS"
    echo "  构建类型: $BUILD_TYPE"
    echo "  代码覆盖率: ${ENABLE_COVERAGE:-OFF}"
    echo "  地址检查器: ${ENABLE_SANITIZER:-OFF}"
    echo "  静态分析: ${ENABLE_STATIC_ANALYSIS:-OFF}"
}

# 主函数
main() {
    # 默认参数
    BUILD_TYPE="Release"
    JOBS=$(nproc 2>/dev/null || echo 4)
    CLEAN_BUILD=false
    RUN_TESTS=false
    INSTALL=false
    
    # 解析命令行参数
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_help
                exit 0
                ;;
            -c|--clean)
                CLEAN_BUILD=true
                shift
                ;;
            -d|--debug)
                BUILD_TYPE="Debug"
                shift
                ;;
            -r|--release)
                BUILD_TYPE="Release"
                shift
                ;;
            -t|--test)
                RUN_TESTS=true
                shift
                ;;
            -i|--install)
                INSTALL=true
                shift
                ;;
            -p|--prefix)
                INSTALL_PREFIX="$2"
                shift 2
                ;;
            -j|--jobs)
                JOBS="$2"
                shift 2
                ;;
            --coverage)
                ENABLE_COVERAGE="ON"
                shift
                ;;
            --sanitizer)
                ENABLE_SANITIZER="ON"
                shift
                ;;
            --static-analysis)
                ENABLE_STATIC_ANALYSIS="ON"
                shift
                ;;
            *)
                log_error "未知选项: $1"
                show_help
                exit 1
                ;;
        esac
    done
    
    # 显示构建信息
    show_build_info
    
    # 检查依赖
    check_dependencies
    
    # 清理构建目录
    if [ "$CLEAN_BUILD" = true ]; then
        clean_build
    fi
    
    # 创建构建目录
    create_build_dir
    
    # 配置构建
    configure_build "$BUILD_TYPE"
    
    # 编译项目
    build_project "$JOBS"
    
    # 运行测试
    if [ "$RUN_TESTS" = true ]; then
        run_tests
    fi
    
    # 生成覆盖率报告
    generate_coverage_report
    
    # 安装项目
    if [ "$INSTALL" = true ]; then
        install_project
    fi
    
    log_success "构建流程完成!"
}

# 运行主函数
main "$@"
