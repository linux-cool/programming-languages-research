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
