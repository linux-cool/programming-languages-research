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
