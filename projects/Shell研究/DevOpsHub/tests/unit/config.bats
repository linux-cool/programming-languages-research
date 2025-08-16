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
