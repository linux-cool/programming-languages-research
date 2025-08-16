#!/bin/bash

# Shell性能优化深度研究
# 系统性能监控与优化分析工具

set -euo pipefail

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# 输出函数
log() {
    echo -e "${GREEN}[$(date +'%Y-%m-%d %H:%M:%S')] $1${NC}"
}

info() {
    echo -e "${BLUE}[INFO] $1${NC}"
}

warning() {
    echo -e "${YELLOW}[WARNING] $1${NC}"
}

error() {
    echo -e "${RED}[ERROR] $1${NC}"
}

# 研究1: CPU性能监控与分析
cpu_performance() {
    log "=== CPU性能监控与分析 ==="
    
    # 获取CPU信息
    echo "CPU信息:"
    echo "--------"
    lscpu | grep -E "(Model name|CPU MHz|Cache size|CPU\(s\)|Thread|Core)"
    
    # 实时CPU使用率
    echo ""
    echo "实时CPU使用率:"
    echo "-------------"
    top -bn1 | grep "Cpu(s)" | awk '{print "用户空间: " $2 ", 系统空间: " $4 ", 空闲: " $8}'
    
    # CPU负载
    echo ""
    echo "CPU负载:"
    echo "--------"
    uptime | awk '{print "当前负载: " $NF ", 1分钟平均: " $(NF-2) ", 5分钟平均: " $(NF-1) ", 15分钟平均: " $NF}'
    
    # 进程CPU使用排行
    echo ""
    echo "CPU使用最高的10个进程:"
    echo "---------------------"
    ps aux --sort=-%cpu | head -11
    
    # CPU压力测试
    echo ""
    echo "CPU压力测试 (5秒):"
    echo "-----------------"
    timeout 5s stress --cpu 2 &>/dev/null && echo "压力测试完成" || echo "压力测试跳过"
}

# 研究2: 内存性能监控与分析
memory_performance() {
    log "=== 内存性能监控与分析 ==="
    
    # 内存总体情况
    echo "内存信息:"
    echo "---------"
    free -h | awk 'NR==2{printf "总内存: %s, 已用: %s, 空闲: %s, 缓存: %s\n", $2, $3, $4, $6}'
    
    # 内存使用率计算
    local mem_info=$(free | awk 'NR==2{printf "%.2f", $3/$2*100}')
    echo "内存使用率: ${mem_info}%"
    
    # 内存使用最高的进程
    echo ""
    echo "内存使用最高的10个进程:"
    echo "---------------------"
    ps aux --sort=-%mem | head -11
    
    # 内存详细信息
    echo ""
    echo "内存详细信息:"
    echo "-------------"
    cat /proc/meminfo | grep -E "(MemTotal|MemFree|MemAvailable|Buffers|Cached|SwapTotal|SwapFree)" | head -7
    
    # 内存碎片检查
    echo ""
    echo "内存碎片分析:"
    echo "------------"
    if command -v vmstat >/dev/null; then
        vmstat -s | grep -E "(free pages|used pages|buffer pages)"
    else
        echo "vmstat 不可用"
    fi
}

# 研究3: 磁盘I/O性能监控
disk_performance() {
    log "=== 磁盘I/O性能监控 ==="
    
    # 磁盘使用情况
    echo "磁盘使用情况:"
    echo "-------------"
    df -h | grep -E "(Filesystem|/dev/)"
    
    # 磁盘I/O统计
    echo ""
    echo "磁盘I/O统计:"
    echo "------------"
    if command -v iostat >/dev/null; then
        iostat -x 1 2 | tail -n +4
    else
        echo "iostat 不可用，使用基础统计:"
        cat /proc/diskstats | awk '{print $3 ": 读 " $4 " 扇区, 写 " $8 " 扇区"}' | head -5
    fi
    
    # 查找大文件
    echo ""
    echo "查找大文件 (前10个 > 100MB):"
    echo "---------------------------"
    find / -type f -size +100M -exec ls -lh {} \; 2>/dev/null | head -10 || echo "未找到或权限不足"
    
    # 目录大小分析
    echo ""
    echo "目录大小分析 (前10个最大目录):"
    echo "-----------------------------"
    du -h --max-depth=1 / 2>/dev/null | sort -hr | head -10
}

# 研究4: 网络性能监控
network_performance() {
    log "=== 网络性能监控 ==="
    
    # 网络接口信息
    echo "网络接口信息:"
    echo "-------------"
    ip -s link | grep -E "(^[0-9]|RX:|TX:)" || ifconfig -a
    
    # 网络连接统计
    echo ""
    echo "网络连接统计:"
    echo "------------"
    netstat -i | grep -E "(Name|eth|wlan|lo)" || ss -i | head -10
    
    # 网络延迟测试
    echo ""
    echo "网络延迟测试:"
    echo "------------"
    local test_hosts=("google.com" "baidu.com" "cloudflare.com")
    for host in "${test_hosts[@]}"; do
        if ping -c 3 "$host" &>/dev/null; then
            local latency=$(ping -c 3 "$host" | tail -1 | awk -F'/' '{print $5}')
            echo "$host 延迟: ${latency}ms"
        else
            echo "$host 不可达"
        fi
    done
    
    # 带宽测试（需要iperf3）
    echo ""
    echo "带宽测试:"
    echo "--------"
    if command -v iperf3 >/dev/null; then
        echo "iperf3 可用，可运行: iperf3 -c [服务器地址]"
    else
        echo "iperf3 未安装，建议安装用于带宽测试"
    fi
}

# 研究5: 进程性能监控
process_performance() {
    log "=== 进程性能监控 ==="
    
    # 进程总数
    echo "进程总数: $(ps aux | wc -l)"
    
    # 僵尸进程检查
    local zombie_count=$(ps aux | awk '$8 ~ /^Z/ {print $0}' | wc -l)
    echo "僵尸进程数: $zombie_count"
    
    # CPU密集型进程
    echo ""
    echo "CPU密集型进程 (CPU > 10%):"
    echo "-------------------------"
    ps aux | awk '$3 > 10.0' | head -10
    
    # 内存密集型进程
    echo ""
    echo "内存密集型进程 (内存 > 5%):"
    echo "--------------------------"
    ps aux | awk '$4 > 5.0' | head -10
    
    # 进程启动时间
    echo ""
    echo "最近启动的10个进程:"
    echo "------------------"
    ps -eo pid,ppid,cmd,pcpu,pmem,etime --sort=-etime | head -11
}

# 研究6: 系统资源瓶颈检测
detect_bottlenecks() {
    log "=== 系统资源瓶颈检测 ==="
    
    # CPU瓶颈检测
    echo "CPU瓶颈分析:"
    echo "------------"
    local cpu_usage=$(top -bn1 | grep "Cpu(s)" | awk '{print $2}' | cut -d'%' -f1)
    if (( $(echo "$cpu_usage > 80" | bc -l) )); then
        warning "CPU使用率过高: ${cpu_usage}%"
    else
        info "CPU使用率正常: ${cpu_usage}%"
    fi
    
    # 内存瓶颈检测
    echo ""
    echo "内存瓶颈分析:"
    echo "-------------"
    local mem_usage=$(free | awk 'NR==2{printf "%.0f", $3/$2*100}')
    if [[ $mem_usage -gt 80 ]]; then
        warning "内存使用率过高: ${mem_usage}%"
    else
        info "内存使用率正常: ${mem_usage}%"
    fi
    
    # 磁盘瓶颈检测
    echo ""
    echo "磁盘瓶颈分析:"
    echo "-------------"
    local disk_usage=$(df -h / | awk 'NR==2{print $5}' | sed 's/%//')
    if [[ $disk_usage -gt 80 ]]; then
        warning "磁盘使用率过高: ${disk_usage}%"
    else
        info "磁盘使用率正常: ${disk_usage}%"
    fi
    
    # 负载检测
    echo ""
    echo "系统负载分析:"
    echo "------------"
    local load_avg=$(uptime | awk -F'load average:' '{print $2}' | awk '{print $1}' | sed 's/,//')
    local cpu_cores=$(nproc)
    local load_ratio=$(echo "scale=2; $load_avg / $cpu_cores" | bc -l)
    
    if (( $(echo "$load_ratio > 1.0" | bc -l) )); then
        warning "系统负载过高: ${load_avg} (${load_ratio} 倍CPU核心数)"
    else
        info "系统负载正常: ${load_avg} (${load_ratio} 倍CPU核心数)"
    fi
}

# 研究7: 实时性能监控
realtime_monitor() {
    log "=== 实时性能监控 ==="
    
    local duration=${1:-60}  # 默认监控60秒
    local interval=${2:-5}   # 默认5秒间隔
    
    echo "开始${duration}秒实时监控 (间隔${interval}秒)..."
    echo "时间,CPU%,内存%,磁盘读KB/s,磁盘写KB/s,网络接收KB/s,网络发送KB/s"
    
    local start_time=$(date +%s)
    while (( $(date +%s) - start_time < duration )); do
        local timestamp=$(date '+%H:%M:%S')
        
        # CPU使用率
        local cpu_usage=$(top -bn1 | grep "Cpu(s)" | awk '{print $2}' | cut -d'%' -f1)
        
        # 内存使用率
        local mem_usage=$(free | awk 'NR==2{printf "%.1f", $3/$2*100}')
        
        # 磁盘I/O
        if command -v iostat >/dev/null; then
            local disk_read=$(iostat -d 1 2 | tail -n +4 | awk 'NR>3{print $3; exit}')
            local disk_write=$(iostat -d 1 2 | tail -n +4 | awk 'NR>3{print $4; exit}')
        else
            local disk_read="N/A"
            local disk_write="N/A"
        fi
        
        # 网络I/O
        if command -v sar >/dev/null; then
            local net_rx=$(sar -n DEV 1 1 | tail -n +4 | awk 'NR==1{print $5}')
            local net_tx=$(sar -n DEV 1 1 | tail -n +4 | awk 'NR==1{print $6}')
        else
            local net_rx="N/A"
            local net_tx="N/A"
        fi
        
        echo "$timestamp,$cpu_usage,$mem_usage,$disk_read,$disk_write,$net_rx,$net_tx"
        
        sleep $interval
    done
}

# 研究8: 性能基准测试
performance_benchmark() {
    log "=== 性能基准测试 ==="
    
    # CPU基准测试
    echo "CPU基准测试:"
    echo "------------"
    local cpu_score=$(echo "scale=2; $(nproc) * 1000" | bc -l)
    echo "CPU评分: $cpu_score (基于核心数)"
    
    # 内存基准测试
    echo ""
    echo "内存基准测试:"
    echo "-------------"
    local mem_total=$(free -m | awk 'NR==2{print $2}')
    local mem_score=$(echo "scale=2; $mem_total / 1024" | bc -l)
    echo "内存评分: $mem_score GB"
    
    # 磁盘基准测试
    echo ""
    echo "磁盘基准测试:"
    echo "-------------"
    if command -v dd >/dev/null; then
        local test_file="/tmp/benchmark_test"
        local write_speed=$(dd if=/dev/zero of="$test_file" bs=1M count=100 2>&1 | tail -1 | awk '{print $(NF-1) " " $NF}')
        local read_speed=$(dd if="$test_file" of=/dev/null bs=1M 2>&1 | tail -1 | awk '{print $(NF-1) " " $NF}')
        echo "写入速度: $write_speed"
        echo "读取速度: $read_speed"
        rm -f "$test_file"
    else
        echo "dd 不可用"
    fi
    
    # 网络基准测试
    echo ""
    echo "网络基准测试:"
    echo "-------------"
    local test_hosts=("8.8.8.8" "1.1.1.1" "208.67.222.222")
    local total_latency=0
    local count=0
    
    for host in "${test_hosts[@]}"; do
        if ping -c 3 "$host" &>/dev/null; then
            local latency=$(ping -c 3 "$host" | tail -1 | awk -F'/' '{print $5}')
            total_latency=$(echo "$total_latency + $latency" | bc -l)
            ((count++))
        fi
    done
    
    if [[ $count -gt 0 ]]; then
        local avg_latency=$(echo "scale=2; $total_latency / $count" | bc -l)
        echo "平均网络延迟: ${avg_latency}ms"
    else
        echo "网络测试失败"
    fi
}

# 研究9: 性能优化建议
optimization_recommendations() {
    log "=== 性能优化建议 ==="
    
    local report_file="/tmp/optimization_report_$(date +%Y%m%d_%H%M%S).txt"
    
    {
        echo "性能优化建议报告 - $(date)"
        echo "============================"
        echo ""
        
        # CPU优化建议
        echo "CPU优化建议:"
        echo "------------"
        local cpu_usage=$(top -bn1 | grep "Cpu(s)" | awk '{print $2}' | cut -d'%' -f1)
        if (( $(echo "$cpu_usage > 80" | bc -l) )); then
            echo "• CPU使用率过高，建议:"
            echo "  - 识别并优化高CPU使用的进程"
            echo "  - 增加CPU核心或升级硬件"
            echo "  - 考虑负载均衡"
        else
            echo "• CPU使用率正常"
        fi
        echo ""
        
        # 内存优化建议
        echo "内存优化建议:"
        echo "-------------"
        local mem_usage=$(free | awk 'NR==2{printf "%.0f", $3/$2*100}')
        if [[ $mem_usage -gt 80 ]]; then
            echo "• 内存使用率过高，建议:"
            echo "  - 增加物理内存"
            echo "  - 优化内存泄漏的应用程序"
            echo "  - 调整缓存设置"
        else
            echo "• 内存使用率正常"
        fi
        echo ""
        
        # 磁盘优化建议
        echo "磁盘优化建议:"
        echo "-------------"
        local disk_usage=$(df -h / | awk 'NR==2{print $5}' | sed 's/%//')
        if [[ $disk_usage -gt 80 ]]; then
            echo "• 磁盘使用率过高，建议:"
            echo "  - 清理不必要的文件"
            echo "  - 移动数据到其他磁盘"
            echo "  - 扩展磁盘容量"
        else
            echo "• 磁盘使用率正常"
        fi
        echo ""
        
        # 网络优化建议
        echo "网络优化建议:"
        echo "-------------"
        echo "• 检查网络延迟和带宽"
        echo "• 优化网络配置"
        echo "• 使用CDN加速"
        echo ""
        
        # 进程优化建议
        echo "进程优化建议:"
        echo "-------------"
        echo "• 监控并限制资源密集型进程"
        echo "• 优化应用程序配置"
        echo "• 使用进程管理工具"
        echo ""
        
        # 系统调优建议
        echo "系统调优建议:"
        echo "-------------"
        echo "• 调整内核参数"
        echo "• 优化文件系统挂载选项"
        echo "• 配置适当的swap大小"
        echo "• 定期清理系统缓存"
        
    } > "$report_file"
    
    echo "优化建议报告已生成: $report_file"
    cat "$report_file"
}

# 主函数
main() {
    log "=== Shell性能优化深度研究 ==="
    
    # 检查依赖
    local missing_tools=()
    for tool in bc stress iostat sar iperf3; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            missing_tools+=("$tool")
        fi
    done
    
    if [[ ${#missing_tools[@]} -gt 0 ]]; then
        warning "缺少以下工具: ${missing_tools[*]}"
        echo "建议安装:"
        echo "  Ubuntu/Debian: sudo apt-get install bc stress sysstat iperf3"
        echo "  CentOS/RHEL: sudo yum install bc stress sysstat iperf3"
    fi
    
    # 执行所有性能测试
    cpu_performance
    memory_performance
    disk_performance
    network_performance
    process_performance
    detect_bottlenecks
    performance_benchmark
    optimization_recommendations
    
    log "=== 性能分析完成 ==="
    
    # 生成总结报告
    local summary_file="/tmp/performance_summary_$(date +%Y%m%d_%H%M%S).txt"
    {
        echo "性能监控总结报告 - $(date)"
        echo "============================"
        echo "系统: $(uname -a)"
        echo "用户: $(whoami)"
        echo "主机: $(hostname)"
        echo ""
        echo "硬件信息:"
        echo "CPU: $(nproc) cores"
        echo "内存: $(free -h | awk 'NR==2{print $2}')"
        echo "磁盘: $(df -h / | awk 'NR==2{print $2}')"
        echo ""
        echo "性能指标:"
        echo "CPU使用率: $(top -bn1 | grep 'Cpu(s)' | awk '{print $2}')"
        echo "内存使用率: $(free | awk 'NR==2{printf "%.1f%%", $3/$2*100}')"
        echo "磁盘使用率: $(df -h / | awk 'NR==2{print $5}')"
        echo "负载: $(uptime | awk -F'load average:' '{print $2}')"
        echo ""
        echo "生成的报告文件:"
        find /tmp -name "*$(date +%Y%m%d)*" -type f -name "*.txt" | grep -E "(optimization|performance)" | while read -r file; do
            echo "  - $file ($(du -h "$file" | cut -f1))"
        done
    } > "$summary_file"
    
    info "总结报告已生成: $summary_file"
}

# 参数处理
case "${1:-all}" in
    "cpu")
        cpu_performance
        ;;
    "memory")
        memory_performance
        ;;
    "disk")
        disk_performance
        ;;
    "network")
        network_performance
        ;;
    "process")
        process_performance
        ;;
    "bottleneck")
        detect_bottlenecks
        ;;
    "benchmark")
        performance_benchmark
        ;;
    "realtime")
        realtime_monitor "${2:-60}" "${3:-5}"
        ;;
    "recommend")
        optimization_recommendations
        ;;
    "all"|*)
        main
        ;;
esac