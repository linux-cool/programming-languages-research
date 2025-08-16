#!/bin/bash

# Shell脚本自动化深度研究
# 系统自动化与DevOps工具链实现

set -euo pipefail

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log() {
    echo -e "${GREEN}[$(date +'%Y-%m-%d %H:%M:%S')] $1${NC}"
}

error() {
    echo -e "${RED}[ERROR] $1${NC}" >&2
}

warning() {
    echo -e "${YELLOW}[WARNING] $1${NC}" >&2
}

# 研究1: 系统监控自动化
monitor_system() {
    log "=== 系统监控自动化 ==="
    
    # CPU使用率监控
    cpu_usage=$(top -bn1 | grep "Cpu(s)" | awk '{print $2}' | cut -d'%' -f1)
    echo "CPU使用率: ${cpu_usage}%"
    
    # 内存使用率监控
    memory_info=$(free -m | awk 'NR==2{printf "%.2f%%", $3*100/$2 }')
    echo "内存使用率: ${memory_info}"
    
    # 磁盘使用率监控
    disk_usage=$(df -h / | awk 'NR==2{print $5}' | cut -d'%' -f1)
    echo "磁盘使用率: ${disk_usage}%"
    
    # 网络连接监控
    connections=$(netstat -an | grep ESTABLISHED | wc -l)
    echo "活跃网络连接: ${connections}"
    
    # 进程监控
    process_count=$(ps aux | wc -l)
    echo "运行进程数: ${process_count}"
    
    # 生成监控报告
    cat > "/tmp/system_report_$(date +%Y%m%d_%H%M%S).txt" << EOF
系统监控报告 - $(date)
CPU使用率: ${cpu_usage}%
内存使用率: ${memory_info}
磁盘使用率: ${disk_usage}%
活跃网络连接: ${connections}
运行进程数: ${process_count}
EOF
}

# 研究2: 日志分析与自动化处理
analyze_logs() {
    log "=== 日志分析与自动化处理 ==="
    
    local log_dir="/var/log"
    local report_file="/tmp/log_analysis_$(date +%Y%m%d_%H%M%S).txt"
    
    # 检查日志目录是否存在
    if [[ ! -d "$log_dir" ]]; then
        warning "日志目录 $log_dir 不存在，使用当前目录"
        log_dir="."
    fi
    
    # 分析系统日志
    echo "日志分析报告 - $(date)" > "$report_file"
    echo "================================" >> "$report_file"
    
    # 查找错误日志
    if [[ -f "$log_dir/syslog" ]]; then
        error_count=$(grep -i "error" "$log_dir/syslog" 2>/dev/null | wc -l || echo "0")
        echo "系统错误日志条数: $error_count" >> "$report_file"
    fi
    
    # 查找警告日志
    if [[ -f "$log_dir/syslog" ]]; then
        warning_count=$(grep -i "warning" "$log_dir/syslog" 2>/dev/null | wc -l || echo "0")
        echo "系统警告日志条数: $warning_count" >> "$report_file"
    fi
    
    # 查找认证失败
    if [[ -f "$log_dir/auth.log" ]]; then
        auth_failures=$(grep -i "authentication failure" "$log_dir/auth.log" 2>/dev/null | wc -l || echo "0")
        echo "认证失败次数: $auth_failures" >> "$report_file"
    fi
    
    # 查找磁盘空间警告
    if [[ -f "$log_dir/syslog" ]]; then
        disk_warnings=$(grep -i "no space left" "$log_dir/syslog" 2>/dev/null | wc -l || echo "0")
        echo "磁盘空间警告: $disk_warnings" >> "$report_file"
    fi
    
    echo "日志分析报告已生成: $report_file"
}

# 研究3: 备份自动化系统
setup_backup_system() {
    log "=== 备份自动化系统 ==="
    
    local backup_dir="/tmp/backups/$(date +%Y%m%d)"
    local source_dirs=("/etc" "/home" "/var/log")
    
    # 创建备份目录
    mkdir -p "$backup_dir"
    
    # 创建备份
    for source_dir in "${source_dirs[@]}"; do
        if [[ -d "$source_dir" ]]; then
            local dir_name=$(basename "$source_dir")
            local backup_file="$backup_dir/${dir_name}_$(date +%H%M%S).tar.gz"
            
            log "备份 $source_dir 到 $backup_file"
            tar -czf "$backup_file" -C "$(dirname "$source_dir")" "$dir_name" 2>/dev/null || warning "备份 $source_dir 失败"
            
            # 验证备份
            if [[ -f "$backup_file" ]]; then
                local backup_size=$(du -h "$backup_file" | cut -f1)
                log "备份成功: $backup_file ($backup_size)"
            fi
        else
            warning "目录 $source_dir 不存在，跳过"
        fi
    done
    
    # 清理旧备份（保留最近7天）
    find /tmp/backups -type d -mtime +7 -exec rm -rf {} + 2>/dev/null || true
}

# 研究4: 服务监控与自动重启
monitor_services() {
    log "=== 服务监控与自动重启 ==="
    
    local services=("ssh" "nginx" "mysql" "apache2")
    local restart_count=0
    
    for service in "${services[@]}"; do
        if command -v systemctl &> /dev/null; then
            # Systemd系统
            if systemctl is-active --quiet "$service"; then
                echo "$service 服务运行正常"
            else
                warning "$service 服务未运行，尝试启动..."
                if sudo systemctl start "$service" 2>/dev/null; then
                    log "$service 服务启动成功"
                    ((restart_count++))
                else
                    error "$service 服务启动失败"
                fi
            fi
        elif command -v service &> /dev/null; then
            # SysV系统
            if service "$service" status &>/dev/null; then
                echo "$service 服务运行正常"
            else
                warning "$service 服务未运行，尝试启动..."
                if sudo service "$service" start 2>/dev/null; then
                    log "$service 服务启动成功"
                    ((restart_count++))
                else
                    error "$service 服务启动失败"
                fi
            fi
        fi
    done
    
    echo "共重启 $restart_count 个服务"
}

# 研究5: 文件系统自动化管理
manage_filesystem() {
    log "=== 文件系统自动化管理 ==="
    
    # 清理临时文件
    local temp_dirs=("/tmp" "/var/tmp")
    local cleaned_files=0
    
    for temp_dir in "${temp_dirs[@]}"; do
        if [[ -d "$temp_dir" ]]; then
            # 清理7天前的临时文件
            local files_to_clean=$(find "$temp_dir" -type f -mtime +7 2>/dev/null | wc -l)
            if [[ $files_to_clean -gt 0 ]]; then
                find "$temp_dir" -type f -mtime +7 -delete 2>/dev/null
                cleaned_files=$((cleaned_files + files_to_clean))
            fi
        fi
    done
    
    echo "清理了 $cleaned_files 个临时文件"
    
    # 检查磁盘空间
    local disk_usage=$(df -h / | awk 'NR==2{print $5}' | sed 's/%//')
    if [[ $disk_usage -gt 80 ]]; then
        warning "磁盘使用率过高: ${disk_usage}%"
        
        # 清理日志文件
        if [[ -d "/var/log" ]]; then
            find /var/log -name "*.log" -size +100M -exec truncate -s 0 {} \; 2>/dev/null || true
            log "已清理大日志文件"
        fi
    else
        echo "磁盘使用率正常: ${disk_usage}%"
    fi
    
    # 查找大文件
    echo "查找前10个最大文件:"
    find / -type f -size +100M -exec ls -lh {} \; 2>/dev/null | head -10 || echo "未找到大文件或权限不足"
}

# 研究6: 网络自动化配置
configure_network() {
    log "=== 网络自动化配置 ==="
    
    # 检查网络连接
    local test_hosts=("google.com" "baidu.com" "cloudflare.com")
    local connected_hosts=0
    
    for host in "${test_hosts[@]}"; do
        if ping -c 1 "$host" &> /dev/null; then
            echo "网络连接正常: $host"
            ((connected_hosts++))
        else
            warning "网络连接失败: $host"
        fi
    done
    
    echo "成功连接的主机数: $connected_hosts/3"
    
    # 检查端口状态
    local important_ports=("22" "80" "443" "3306")
    echo "重要端口状态:"
    
    for port in "${important_ports[@]}"; do
        if netstat -tuln | grep -q ":$port "; then
            echo "  端口 $port 已监听"
        else
            echo "  端口 $port 未监听"
        fi
    done
}

# 研究7: 软件部署自动化
deploy_software() {
    log "=== 软件部署自动化 ==="
    
    # 检查包管理器
    local package_manager=""
    if command -v apt-get &> /dev/null; then
        package_manager="apt-get"
    elif command -v yum &> /dev/null; then
        package_manager="yum"
    elif command -v dnf &> /dev/null; then
        package_manager="dnf"
    elif command -v pacman &> /dev/null; then
        package_manager="pacman"
    fi
    
    if [[ -n "$package_manager" ]]; then
        echo "检测到包管理器: $package_manager"
        
        # 更新软件包列表
        log "更新软件包列表..."
        if [[ "$package_manager" == "apt-get" ]]; then
            sudo apt-get update -qq
        elif [[ "$package_manager" == "yum" ]]; then
            sudo yum check-update -q
        elif [[ "$package_manager" == "dnf" ]]; then
            sudo dnf check-update -q
        elif [[ "$package_manager" == "pacman" ]]; then
            sudo pacman -Sy
        fi
        
        # 安装常用工具
        local common_tools=("git" "curl" "wget" "htop" "tree")
        local installed_count=0
        
        for tool in "${common_tools[@]}"; do
            if ! command -v "$tool" &> /dev/null; then
                log "安装 $tool..."
                if [[ "$package_manager" == "apt-get" ]]; then
                    sudo apt-get install -y "$tool" &>/dev/null
                elif [[ "$package_manager" == "yum" || "$package_manager" == "dnf" ]]; then
                    sudo "$package_manager" install -y "$tool" &>/dev/null
                elif [[ "$package_manager" == "pacman" ]]; then
                    sudo pacman -S --noconfirm "$tool" &>/dev/null
                fi
                
                if command -v "$tool" &> /dev/null; then
                    ((installed_count++))
                    echo "$tool 安装成功"
                else
                    error "$tool 安装失败"
                fi
            else
                echo "$tool 已安装"
            fi
        done
        
        echo "共安装 $installed_count 个新工具"
    else
        warning "未检测到支持的包管理器"
    fi
}

# 研究8: 容器化部署自动化
setup_docker() {
    log "=== 容器化部署自动化 ==="
    
    # 检查Docker是否已安装
    if command -v docker &> /dev/null; then
        echo "Docker 已安装"
        
        # 检查Docker服务状态
        if systemctl is-active --quiet docker; then
            echo "Docker 服务运行正常"
        else
            warning "Docker 服务未运行，尝试启动..."
            sudo systemctl start docker
        fi
        
        # 拉取常用镜像
        local common_images=("nginx" "mysql" "redis" "ubuntu")
        for image in "${common_images[@]}"; do
            if ! docker images | grep -q "^$image"; then
                log "拉取 Docker 镜像: $image"
                docker pull "$image" &>/dev/null || warning "拉取 $image 失败"
            else
                echo "镜像 $image 已存在"
            fi
        done
        
        # 运行测试容器
        if ! docker ps -a | grep -q "test-nginx"; then
            log "启动测试 Nginx 容器..."
            docker run -d --name test-nginx -p 8080:80 nginx &>/dev/null || warning "启动测试容器失败"
        else
            echo "测试容器已存在"
        fi
    else
        warning "Docker 未安装"
        
        # 提供安装建议
        echo "安装 Docker:"
        echo "  Ubuntu/Debian: sudo apt-get install docker.io"
        echo "  CentOS/RHEL: sudo yum install docker"
        echo "  或使用官方脚本: curl -fsSL https://get.docker.com | sh"
    fi
}

# 研究9: 配置管理自动化
manage_configurations() {
    log "=== 配置管理自动化 ==="
    
    # 创建配置备份
    local config_backup_dir="/tmp/config_backups/$(date +%Y%m%d)"
    mkdir -p "$config_backup_dir"
    
    # 备份重要配置文件
    local important_configs=(
        "/etc/passwd"
        "/etc/group"
        "/etc/hosts"
        "/etc/resolv.conf"
        "/etc/fstab"
    )
    
    for config in "${important_configs[@]}"; do
        if [[ -f "$config" ]]; then
            local backup_name="$config_backup_dir/$(basename "$config")_$(date +%H%M%S)"
            cp "$config" "$backup_name"
            echo "备份配置文件: $config -> $backup_name"
        fi
    done
    
    # 生成系统配置报告
    local config_report="/tmp/config_report_$(date +%Y%m%d_%H%M%S).txt"
    {
        echo "系统配置报告 - $(date)"
        echo "=============================="
        echo ""
        echo "网络配置:"
        ip addr show 2>/dev/null || ifconfig 2>/dev/null || echo "无法获取网络配置"
        echo ""
        echo "路由表:"
        ip route show 2>/dev/null || route -n 2>/dev/null || echo "无法获取路由表"
        echo ""
        echo "DNS配置:"
        cat /etc/resolv.conf 2>/dev/null || echo "无法获取DNS配置"
        echo ""
        echo "系统信息:"
        uname -a
        echo ""
        echo "内存信息:"
        free -h
        echo ""
        echo "磁盘信息:"
        df -h
    } > "$config_report"
    
    echo "配置报告已生成: $config_report"
}

# 主函数
main() {
    log "=== Shell脚本自动化深度研究 ==="
    
    # 检查运行权限
    if [[ $EUID -eq 0 ]]; then
        warning "以root用户运行，请谨慎操作"
    fi
    
    # 执行所有自动化任务
    monitor_system
    analyze_logs
    setup_backup_system
    monitor_services
    manage_filesystem
    configure_network
    deploy_software
    setup_docker
    manage_configurations
    
    log "=== 自动化任务完成 ==="
    
    # 生成总结报告
    local summary_report="/tmp/automation_summary_$(date +%Y%m%d_%H%M%S).txt"
    {
        echo "Shell自动化执行总结 - $(date)"
        echo "================================"
        echo "执行时间: $(date)"
        echo "用户: $(whoami)"
        echo "主机: $(hostname)"
        echo "系统: $(uname -a)"
        echo ""
        echo "执行的任务:"
        echo "1. 系统监控"
        echo "2. 日志分析"
        echo "3. 备份系统"
        echo "4. 服务监控"
        echo "5. 文件系统管理"
        echo "6. 网络配置"
        echo "7. 软件部署"
        echo "8. 容器化部署"
        echo "9. 配置管理"
        echo ""
        echo "生成的文件:"
        find /tmp -name "*$(date +%Y%m%d)*" -type f -name "*.txt" | while read -r file; do
            echo "  - $file ($(du -h "$file" | cut -f1))"
        done
    } > "$summary_report"
    
    echo ""
    log "总结报告已生成: $summary_report"
}

# 参数处理
case "${1:-all}" in
    "monitor")
        monitor_system
        ;;
    "logs")
        analyze_logs
        ;;
    "backup")
        setup_backup_system
        ;;
    "services")
        monitor_services
        ;;
    "filesystem")
        manage_filesystem
        ;;
    "network")
        configure_network
        ;;
    "deploy")
        deploy_software
        ;;
    "docker")
        setup_docker
        ;;
    "config")
        manage_configurations
        ;;
    "all"|*)
        main
        ;;
esac