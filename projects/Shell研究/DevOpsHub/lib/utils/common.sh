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
