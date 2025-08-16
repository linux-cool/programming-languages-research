# DevOpsHub API设计文档

## 1. API概述

### 1.1 设计原则
- **RESTful设计**: 遵循REST架构风格
- **统一接口**: 一致的API接口设计
- **版本控制**: 支持API版本管理
- **错误处理**: 统一的错误响应格式
- **安全认证**: 完善的认证和授权机制

### 1.2 API特性
- **命令行接口**: 丰富的CLI命令支持
- **Web API**: HTTP RESTful API
- **批量操作**: 支持批量任务执行
- **异步处理**: 长时间任务异步执行
- **实时通知**: WebSocket实时状态推送

### 1.3 接口分类
- **系统管理API**: 系统配置和状态管理
- **部署管理API**: 应用部署和版本控制
- **监控管理API**: 系统监控和告警管理
- **日志管理API**: 日志查询和分析
- **备份管理API**: 数据备份和恢复

## 2. 命令行接口 (CLI)

### 2.1 主命令结构
```bash
devops-hub [global-options] <command> [command-options] [arguments]

Global Options:
  -c, --config FILE     指定配置文件
  -v, --verbose         详细输出模式
  -q, --quiet           静默模式
  -h, --help            显示帮助信息
  --version             显示版本信息

Commands:
  init                  初始化系统
  deploy                部署管理
  monitor               监控管理
  backup                备份管理
  log                   日志管理
  config                配置管理
  status                状态查询
```

### 2.2 部署管理命令
```bash
# 部署应用
devops-hub deploy create <app-name> [options]
  --version VERSION     指定版本
  --env ENVIRONMENT     指定环境
  --strategy STRATEGY   部署策略 (blue-green|rolling|canary)
  --config FILE         部署配置文件
  --dry-run             预演模式

# 查看部署状态
devops-hub deploy status [app-name]
  --env ENVIRONMENT     指定环境
  --format FORMAT       输出格式 (table|json|yaml)

# 回滚部署
devops-hub deploy rollback <app-name> [version]
  --env ENVIRONMENT     指定环境
  --confirm             确认回滚

# 部署历史
devops-hub deploy history <app-name>
  --env ENVIRONMENT     指定环境
  --limit NUMBER        限制条数
```

### 2.3 监控管理命令
```bash
# 启动监控
devops-hub monitor start [service-name]
  --config FILE         监控配置
  --interval SECONDS    监控间隔

# 停止监控
devops-hub monitor stop [service-name]

# 监控状态
devops-hub monitor status
  --format FORMAT       输出格式

# 告警规则管理
devops-hub monitor alert create <rule-name>
  --condition CONDITION 告警条件
  --action ACTION       告警动作
  --config FILE         规则配置文件

devops-hub monitor alert list
devops-hub monitor alert delete <rule-name>
```

### 2.4 备份管理命令
```bash
# 创建备份
devops-hub backup create <backup-name>
  --type TYPE           备份类型 (full|incremental)
  --source PATH         备份源路径
  --target PATH         备份目标路径
  --compress            压缩备份

# 恢复备份
devops-hub backup restore <backup-name>
  --target PATH         恢复目标路径
  --verify              验证恢复

# 备份列表
devops-hub backup list
  --type TYPE           备份类型过滤
  --date DATE           日期过滤

# 删除备份
devops-hub backup delete <backup-name>
  --confirm             确认删除
```

## 3. Web API接口

### 3.1 API基础信息
- **Base URL**: `http://localhost:8080/api/v1`
- **认证方式**: Bearer Token / API Key
- **内容类型**: `application/json`
- **字符编码**: `UTF-8`

### 3.2 通用响应格式
```json
{
  "success": true,
  "code": 200,
  "message": "Success",
  "data": {},
  "timestamp": "2024-01-01T00:00:00Z",
  "request_id": "req-123456"
}
```

### 3.3 错误响应格式
```json
{
  "success": false,
  "code": 400,
  "message": "Bad Request",
  "error": {
    "type": "ValidationError",
    "details": "Invalid parameter: app_name is required"
  },
  "timestamp": "2024-01-01T00:00:00Z",
  "request_id": "req-123456"
}
```

### 3.4 系统管理API

#### 3.4.1 系统状态
```http
GET /api/v1/system/status
```
**响应示例**:
```json
{
  "success": true,
  "data": {
    "status": "running",
    "version": "1.0.0",
    "uptime": 86400,
    "cpu_usage": 25.5,
    "memory_usage": 60.2,
    "disk_usage": 45.8,
    "active_tasks": 5
  }
}
```

#### 3.4.2 系统配置
```http
GET /api/v1/system/config
PUT /api/v1/system/config
```

### 3.5 部署管理API

#### 3.5.1 创建部署
```http
POST /api/v1/deployments
```
**请求体**:
```json
{
  "app_name": "web-app",
  "version": "1.2.0",
  "environment": "production",
  "strategy": "blue-green",
  "config": {
    "replicas": 3,
    "health_check": "/health",
    "timeout": 300
  }
}
```

#### 3.5.2 查询部署
```http
GET /api/v1/deployments
GET /api/v1/deployments/{deployment_id}
```

#### 3.5.3 部署回滚
```http
POST /api/v1/deployments/{deployment_id}/rollback
```

### 3.6 监控管理API

#### 3.6.1 获取监控数据
```http
GET /api/v1/monitoring/metrics
```
**查询参数**:
- `metric_name`: 指标名称
- `start_time`: 开始时间
- `end_time`: 结束时间
- `interval`: 时间间隔

#### 3.6.2 告警规则管理
```http
GET /api/v1/monitoring/alerts
POST /api/v1/monitoring/alerts
PUT /api/v1/monitoring/alerts/{alert_id}
DELETE /api/v1/monitoring/alerts/{alert_id}
```

### 3.7 日志管理API

#### 3.7.1 日志查询
```http
GET /api/v1/logs
```
**查询参数**:
- `level`: 日志级别
- `service`: 服务名称
- `start_time`: 开始时间
- `end_time`: 结束时间
- `keyword`: 关键词搜索
- `limit`: 返回条数
- `offset`: 偏移量

#### 3.7.2 日志统计
```http
GET /api/v1/logs/stats
```

### 3.8 备份管理API

#### 3.8.1 创建备份任务
```http
POST /api/v1/backups
```
**请求体**:
```json
{
  "name": "daily-backup-20240101",
  "type": "full",
  "source": "/data/app",
  "target": "/backup/app",
  "schedule": "0 2 * * *",
  "retention": 30,
  "compress": true
}
```

#### 3.8.2 备份任务管理
```http
GET /api/v1/backups
GET /api/v1/backups/{backup_id}
DELETE /api/v1/backups/{backup_id}
```

#### 3.8.3 恢复操作
```http
POST /api/v1/backups/{backup_id}/restore
```

## 4. WebSocket接口

### 4.1 实时状态推送
```javascript
// 连接WebSocket
const ws = new WebSocket('ws://localhost:8080/ws/v1/status');

// 订阅事件
ws.send(JSON.stringify({
  action: 'subscribe',
  topics: ['deployments', 'monitoring', 'alerts']
}));

// 接收消息
ws.onmessage = function(event) {
  const message = JSON.parse(event.data);
  console.log('Received:', message);
};
```

### 4.2 消息格式
```json
{
  "type": "event",
  "topic": "deployments",
  "event": "deployment_completed",
  "data": {
    "deployment_id": "deploy-123",
    "app_name": "web-app",
    "status": "success",
    "timestamp": "2024-01-01T00:00:00Z"
  }
}
```

## 5. 认证和授权

### 5.1 认证方式

#### 5.1.1 API Key认证
```http
GET /api/v1/system/status
Authorization: Bearer your-api-key-here
```

#### 5.1.2 JWT Token认证
```http
POST /api/v1/auth/login
Content-Type: application/json

{
  "username": "admin",
  "password": "password"
}
```

### 5.2 权限控制
```json
{
  "user": "admin",
  "roles": ["admin", "deployer"],
  "permissions": [
    "system:read",
    "system:write",
    "deployment:create",
    "deployment:read",
    "monitoring:read"
  ]
}
```

## 6. 错误码定义

### 6.1 HTTP状态码
- `200 OK`: 请求成功
- `201 Created`: 资源创建成功
- `400 Bad Request`: 请求参数错误
- `401 Unauthorized`: 未授权访问
- `403 Forbidden`: 权限不足
- `404 Not Found`: 资源不存在
- `409 Conflict`: 资源冲突
- `500 Internal Server Error`: 服务器内部错误

### 6.2 业务错误码
- `1001`: 参数验证失败
- `1002`: 资源不存在
- `1003`: 操作权限不足
- `2001`: 部署任务创建失败
- `2002`: 部署状态异常
- `3001`: 监控服务不可用
- `3002`: 告警规则配置错误
- `4001`: 备份任务执行失败
- `4002`: 恢复操作失败

## 7. API使用示例

### 7.1 部署应用示例
```bash
# 1. 创建部署任务
curl -X POST http://localhost:8080/api/v1/deployments \
  -H "Authorization: Bearer your-token" \
  -H "Content-Type: application/json" \
  -d '{
    "app_name": "web-app",
    "version": "1.2.0",
    "environment": "production",
    "strategy": "blue-green"
  }'

# 2. 查询部署状态
curl -X GET http://localhost:8080/api/v1/deployments/deploy-123 \
  -H "Authorization: Bearer your-token"

# 3. 监控部署进度 (WebSocket)
wscat -c ws://localhost:8080/ws/v1/status \
  -H "Authorization: Bearer your-token"
```

### 7.2 监控查询示例
```bash
# 查询CPU使用率
curl -X GET "http://localhost:8080/api/v1/monitoring/metrics?metric_name=cpu_usage&start_time=2024-01-01T00:00:00Z&end_time=2024-01-01T23:59:59Z" \
  -H "Authorization: Bearer your-token"

# 创建告警规则
curl -X POST http://localhost:8080/api/v1/monitoring/alerts \
  -H "Authorization: Bearer your-token" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "high-cpu-alert",
    "condition": "cpu_usage > 80",
    "action": "email",
    "config": {
      "recipients": ["admin@example.com"]
    }
  }'
```

## 8. SDK和客户端库

### 8.1 Shell客户端
```bash
# 安装DevOpsHub CLI
curl -sSL https://get.devopshub.io/install.sh | bash

# 配置API端点
devops-hub config set api.endpoint http://localhost:8080
devops-hub config set api.token your-api-token

# 使用CLI
devops-hub deploy create web-app --version 1.2.0 --env production
```

### 8.2 Python客户端示例
```python
from devopshub import DevOpsHubClient

client = DevOpsHubClient(
    endpoint='http://localhost:8080',
    token='your-api-token'
)

# 创建部署
deployment = client.deployments.create(
    app_name='web-app',
    version='1.2.0',
    environment='production'
)

# 监控部署状态
status = client.deployments.get_status(deployment.id)
print(f"Deployment status: {status}")
```

这个API设计文档详细定义了DevOpsHub项目的所有接口规范，包括CLI命令、Web API、WebSocket接口、认证授权、错误处理等，为项目的接口实现提供了完整的设计指导。
