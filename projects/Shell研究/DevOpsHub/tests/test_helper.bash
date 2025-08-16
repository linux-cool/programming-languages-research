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
