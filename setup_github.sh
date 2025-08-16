#!/bin/bash

# GitHub 仓库设置快速启动脚本
# 使用方法: ./setup_github.sh YOUR_GITHUB_USERNAME

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 检查参数
if [ $# -eq 0 ]; then
    echo -e "${RED}错误: 请提供GitHub用户名${NC}"
    echo "使用方法: $0 YOUR_GITHUB_USERNAME"
    exit 1
fi

GITHUB_USERNAME=$1
REPO_NAME="programming-languages-research"
REPO_URL="https://github.com/$GITHUB_USERNAME/$REPO_NAME.git"

echo -e "${BLUE}🚀 开始设置GitHub仓库...${NC}"
echo -e "${YELLOW}GitHub用户名: $GITHUB_USERNAME${NC}"
echo -e "${YELLOW}仓库名称: $REPO_NAME${NC}"
echo ""

# 检查git状态
if [ ! -d ".git" ]; then
    echo -e "${RED}错误: 当前目录不是git仓库${NC}"
    exit 1
fi

# 检查远程仓库是否已存在
if git remote get-url origin >/dev/null 2>&1; then
    echo -e "${YELLOW}⚠️  远程仓库已存在，正在更新...${NC}"
    git remote set-url origin $REPO_URL
else
    echo -e "${GREEN}➕ 添加远程仓库...${NC}"
    git remote add origin $REPO_URL
fi

echo -e "${GREEN}✅ 远程仓库设置完成${NC}"
echo ""

# 显示下一步操作说明
echo -e "${BLUE}📋 下一步操作:${NC}"
echo "1. 访问 https://github.com/new 创建新仓库"
echo "   - Repository name: $REPO_NAME"
echo "   - Description: C/C++/Python/Shell编程语言深度研究项目展示网站"
echo "   - 选择 Public 或 Private"
echo "   - 不要勾选 'Initialize this repository with' 选项"
echo ""
echo "2. 创建仓库后，运行以下命令推送代码:"
echo -e "${GREEN}   git push -u origin main${NC}"
echo ""
echo "3. 启用 GitHub Pages:"
echo "   - 进入仓库 Settings > Pages"
echo "   - Source: Deploy from a branch"
echo "   - Branch: main, Folder: / (root)"
echo ""
echo -e "${BLUE}🌐 网站地址将是: https://$GITHUB_USERNAME.github.io/$REPO_NAME/${NC}"
echo ""

# 询问是否现在推送
read -p "是否现在推送代码到GitHub? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${GREEN}🚀 正在推送代码...${NC}"
    git push -u origin main
    echo -e "${GREEN}✅ 代码推送完成！${NC}"
    echo ""
    echo -e "${BLUE}🎉 恭喜！你的编程语言研究项目已经成功推送到GitHub！${NC}"
    echo -e "${BLUE}📱 记得在仓库设置中启用GitHub Pages来部署你的网站${NC}"
else
    echo -e "${YELLOW}好的，你可以稍后手动推送代码${NC}"
fi

echo ""
echo -e "${GREEN}✨ 设置完成！${NC}"
