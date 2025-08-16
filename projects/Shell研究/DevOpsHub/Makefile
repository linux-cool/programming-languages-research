# DevOpsHub Makefile
# 简化开发和部署流程

.PHONY: help install dev test lint clean status demo

# 默认目标
.DEFAULT_GOAL := help

# 项目变量
PROJECT_NAME := DevOpsHub
VERSION := 1.0.0
SHELL := /bin/bash

# 颜色定义
GREEN := \033[0;32m
YELLOW := \033[1;33m
BLUE := \033[0;34m
RED := \033[0;31m
NC := \033[0m # No Color

# 帮助信息
help: ## 显示帮助信息
	@echo -e "$(BLUE)$(PROJECT_NAME) v$(VERSION) - 企业级系统运维自动化平台$(NC)"
	@echo
	@echo -e "$(GREEN)可用命令:$(NC)"
	@awk 'BEGIN {FS = ":.*?## "} /^[a-zA-Z_-]+:.*?## / {printf "  $(YELLOW)%-15s$(NC) %s\n", $$1, $$2}' $(MAKEFILE_LIST)
	@echo
	@echo -e "$(GREEN)示例:$(NC)"
	@echo "  make install    # 安装开发环境"
	@echo "  make dev        # 启动开发模式"
	@echo "  make test       # 运行测试"
	@echo "  make demo       # 运行演示"

# 安装开发环境
install: ## 安装开发环境和依赖
	@echo -e "$(GREEN)安装DevOpsHub开发环境...$(NC)"
	@chmod +x scripts/setup-dev.sh
	@./scripts/setup-dev.sh
	@echo -e "$(GREEN)✅ 安装完成！$(NC)"

# 快速启动
start: ## 快速启动项目
	@echo -e "$(GREEN)快速启动DevOpsHub...$(NC)"
	@chmod +x quick-start.sh
	@./quick-start.sh

# 开发模式
dev: ## 启动开发模式
	@echo -e "$(GREEN)启动开发模式...$(NC)"
	@export DEVOPS_ENV=development && \
	 export DEVOPS_LOG_LEVEL=DEBUG && \
	 ./bin/devops-hub status

# 运行测试
test: ## 运行所有测试
	@echo -e "$(GREEN)运行测试套件...$(NC)"
	@if command -v bats >/dev/null 2>&1; then \
		if [ -d "tests/unit" ] && [ -n "$$(ls -A tests/unit/*.bats 2>/dev/null)" ]; then \
			echo "运行单元测试..."; \
			bats tests/unit/*.bats; \
		else \
			echo -e "$(YELLOW)没有找到单元测试文件$(NC)"; \
		fi; \
		if [ -d "tests/integration" ] && [ -n "$$(ls -A tests/integration/*.bats 2>/dev/null)" ]; then \
			echo "运行集成测试..."; \
			bats tests/integration/*.bats; \
		else \
			echo -e "$(YELLOW)没有找到集成测试文件$(NC)"; \
		fi; \
	else \
		echo -e "$(RED)bats测试框架未安装，请先运行 make install$(NC)"; \
		exit 1; \
	fi

# 代码检查
lint: ## 运行代码质量检查
	@echo -e "$(GREEN)运行代码质量检查...$(NC)"
	@if command -v shellcheck >/dev/null 2>&1; then \
		echo "运行ShellCheck..."; \
		find . -name "*.sh" -not -path "./tests/*" | xargs shellcheck || true; \
	else \
		echo -e "$(RED)shellcheck未安装，请先运行 make install$(NC)"; \
	fi
	@if command -v shfmt >/dev/null 2>&1; then \
		echo "检查代码格式..."; \
		find . -name "*.sh" -not -path "./tests/*" | xargs shfmt -d || true; \
	else \
		echo -e "$(YELLOW)shfmt未安装，跳过格式检查$(NC)"; \
	fi

# 格式化代码
format: ## 格式化Shell代码
	@echo -e "$(GREEN)格式化代码...$(NC)"
	@if command -v shfmt >/dev/null 2>&1; then \
		find . -name "*.sh" -not -path "./tests/*" | xargs shfmt -w -i 4; \
		echo -e "$(GREEN)✅ 代码格式化完成$(NC)"; \
	else \
		echo -e "$(RED)shfmt未安装，请先运行 make install$(NC)"; \
		exit 1; \
	fi

# 查看状态
status: ## 查看项目状态
	@echo -e "$(GREEN)DevOpsHub项目状态:$(NC)"
	@if [ -f "bin/devops-hub" ]; then \
		./bin/devops-hub version; \
		echo; \
		./bin/devops-hub status; \
	else \
		echo -e "$(RED)项目未初始化，请先运行 make install$(NC)"; \
	fi

# 运行演示
demo: ## 运行交互式演示
	@echo -e "$(GREEN)启动交互式演示...$(NC)"
	@chmod +x quick-start.sh
	@./quick-start.sh --demo

# 构建文档
docs: ## 生成项目文档
	@echo -e "$(GREEN)生成项目文档...$(NC)"
	@mkdir -p docs/generated
	@echo "# DevOpsHub 文档索引" > docs/generated/index.md
	@echo "" >> docs/generated/index.md
	@echo "## 项目文档" >> docs/generated/index.md
	@echo "- [项目概述](../README.md)" >> docs/generated/index.md
	@echo "- [需求文档](../REQUIREMENTS.md)" >> docs/generated/index.md
	@echo "- [架构文档](../ARCHITECTURE.md)" >> docs/generated/index.md
	@echo "- [API设计](../API_DESIGN.md)" >> docs/generated/index.md
	@echo "- [开发指南](../DEVELOPMENT.md)" >> docs/generated/index.md
	@echo "- [项目规划](../PROJECT_PLAN.md)" >> docs/generated/index.md
	@echo -e "$(GREEN)✅ 文档生成完成: docs/generated/index.md$(NC)"

# 清理项目
clean: ## 清理临时文件和缓存
	@echo -e "$(GREEN)清理项目...$(NC)"
	@rm -rf data/logs/*.log
	@rm -rf data/cache/*
	@rm -rf data/db/*.db
	@rm -rf /tmp/devopshub-*
	@find . -name "*.tmp" -delete
	@find . -name "*.bak" -delete
	@find . -name "*~" -delete
	@echo -e "$(GREEN)✅ 清理完成$(NC)"

# 重置项目
reset: clean ## 重置项目到初始状态
	@echo -e "$(YELLOW)重置项目到初始状态...$(NC)"
	@rm -rf bin/ lib/ config/ modules/ data/ tests/
	@echo -e "$(GREEN)✅ 项目重置完成，请运行 make install 重新初始化$(NC)"

# 检查依赖
check-deps: ## 检查系统依赖
	@echo -e "$(GREEN)检查系统依赖...$(NC)"
	@echo -n "Bash: "
	@if command -v bash >/dev/null 2>&1; then \
		echo -e "$(GREEN)✓$(NC) $$(bash --version | head -n1)"; \
	else \
		echo -e "$(RED)✗ 未安装$(NC)"; \
	fi
	@echo -n "Git: "
	@if command -v git >/dev/null 2>&1; then \
		echo -e "$(GREEN)✓$(NC) $$(git --version)"; \
	else \
		echo -e "$(RED)✗ 未安装$(NC)"; \
	fi
	@echo -n "curl: "
	@if command -v curl >/dev/null 2>&1; then \
		echo -e "$(GREEN)✓$(NC) $$(curl --version | head -n1)"; \
	else \
		echo -e "$(RED)✗ 未安装$(NC)"; \
	fi
	@echo -n "jq: "
	@if command -v jq >/dev/null 2>&1; then \
		echo -e "$(GREEN)✓$(NC) $$(jq --version)"; \
	else \
		echo -e "$(RED)✗ 未安装$(NC)"; \
	fi
	@echo -n "Docker: "
	@if command -v docker >/dev/null 2>&1; then \
		echo -e "$(GREEN)✓$(NC) $$(docker --version)"; \
	else \
		echo -e "$(YELLOW)? 未安装$(NC)"; \
	fi
	@echo -n "ShellCheck: "
	@if command -v shellcheck >/dev/null 2>&1; then \
		echo -e "$(GREEN)✓$(NC) $$(shellcheck --version | grep version)"; \
	else \
		echo -e "$(YELLOW)? 未安装$(NC)"; \
	fi
	@echo -n "bats: "
	@if command -v bats >/dev/null 2>&1; then \
		echo -e "$(GREEN)✓$(NC) $$(bats --version)"; \
	else \
		echo -e "$(YELLOW)? 未安装$(NC)"; \
	fi

# 创建发布包
package: ## 创建发布包
	@echo -e "$(GREEN)创建发布包...$(NC)"
	@mkdir -p dist
	@tar -czf dist/devopshub-$(VERSION).tar.gz \
		--exclude='dist' \
		--exclude='data/logs/*' \
		--exclude='data/cache/*' \
		--exclude='data/db/*' \
		--exclude='.git' \
		--exclude='*.tmp' \
		--exclude='*.log' \
		.
	@echo -e "$(GREEN)✅ 发布包创建完成: dist/devopshub-$(VERSION).tar.gz$(NC)"

# 安装到系统
install-system: ## 安装到系统目录
	@echo -e "$(GREEN)安装到系统目录...$(NC)"
	@sudo mkdir -p /opt/devopshub
	@sudo cp -r . /opt/devopshub/
	@sudo ln -sf /opt/devopshub/bin/devops-hub /usr/local/bin/devops-hub
	@sudo chmod +x /usr/local/bin/devops-hub
	@echo -e "$(GREEN)✅ 系统安装完成，可以使用 devops-hub 命令$(NC)"

# 卸载系统安装
uninstall-system: ## 卸载系统安装
	@echo -e "$(YELLOW)卸载系统安装...$(NC)"
	@sudo rm -f /usr/local/bin/devops-hub
	@sudo rm -rf /opt/devopshub
	@echo -e "$(GREEN)✅ 系统卸载完成$(NC)"

# 显示项目信息
info: ## 显示项目信息
	@echo -e "$(BLUE)项目信息:$(NC)"
	@echo "  名称: $(PROJECT_NAME)"
	@echo "  版本: $(VERSION)"
	@echo "  类型: Shell脚本项目"
	@echo "  许可: MIT"
	@echo
	@echo -e "$(BLUE)目录结构:$(NC)"
	@if command -v tree >/dev/null 2>&1; then \
		tree -L 2 -I 'data'; \
	else \
		find . -maxdepth 2 -type d | sort; \
	fi

# 开发者工具
dev-tools: ## 安装开发者工具
	@echo -e "$(GREEN)安装开发者工具...$(NC)"
	@if [[ "$$OSTYPE" == "darwin"* ]]; then \
		brew install shellcheck shfmt bats-core; \
	elif command -v apt-get >/dev/null 2>&1; then \
		sudo apt-get update && sudo apt-get install -y shellcheck; \
	elif command -v yum >/dev/null 2>&1; then \
		sudo yum install -y ShellCheck; \
	fi
	@echo -e "$(GREEN)✅ 开发者工具安装完成$(NC)"
