# DevOpsHub Docker镜像
FROM ubuntu:22.04

# 设置环境变量
ENV DEBIAN_FRONTEND=noninteractive
ENV DEVOPS_HOME=/opt/devopshub
ENV DEVOPS_ENV=production
ENV PATH="${DEVOPS_HOME}/bin:${PATH}"

# 设置标签
LABEL maintainer="DevOpsHub Team"
LABEL version="1.0.0"
LABEL description="企业级系统运维自动化平台"

# 安装系统依赖
RUN apt-get update && apt-get install -y \
    bash \
    curl \
    wget \
    git \
    jq \
    sqlite3 \
    htop \
    tree \
    vim \
    ca-certificates \
    gnupg \
    lsb-release \
    shellcheck \
    && rm -rf /var/lib/apt/lists/*

# 安装Docker CLI (用于容器管理功能)
RUN curl -fsSL https://download.docker.com/linux/ubuntu/gpg | gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg \
    && echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | tee /etc/apt/sources.list.d/docker.list > /dev/null \
    && apt-get update \
    && apt-get install -y docker-ce-cli \
    && rm -rf /var/lib/apt/lists/*

# 创建应用目录
RUN mkdir -p ${DEVOPS_HOME}

# 复制项目文件
COPY . ${DEVOPS_HOME}/

# 设置工作目录
WORKDIR ${DEVOPS_HOME}

# 设置权限
RUN chmod +x bin/devops-hub \
    && chmod +x scripts/*.sh \
    && chmod +x quick-start.sh

# 创建数据目录
RUN mkdir -p data/logs data/cache data/db \
    && touch data/logs/.gitkeep data/cache/.gitkeep data/db/.gitkeep

# 创建非root用户
RUN useradd -m -s /bin/bash devops \
    && chown -R devops:devops ${DEVOPS_HOME}

# 切换到非root用户
USER devops

# 健康检查
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD ${DEVOPS_HOME}/bin/devops-hub status || exit 1

# 暴露端口
EXPOSE 8080

# 设置入口点
ENTRYPOINT ["./bin/devops-hub"]

# 默认命令
CMD ["help"]
