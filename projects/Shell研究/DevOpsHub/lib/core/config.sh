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
