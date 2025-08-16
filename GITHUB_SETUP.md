# GitHub 仓库设置指南

## 🚀 创建 GitHub 仓库

1. 访问 [GitHub](https://github.com) 并登录你的账户
2. 点击右上角的 "+" 号，选择 "New repository"
3. 填写仓库信息：
   - **Repository name**: `programming-languages-research`
   - **Description**: `C/C++/Python/Shell编程语言深度研究项目展示网站`
   - **Visibility**: 选择 Public 或 Private
   - **Initialize this repository with**: 不要勾选任何选项（我们已经有了本地代码）

## 🔗 连接远程仓库

创建仓库后，GitHub会显示仓库的URL。使用以下命令连接远程仓库：

```bash
# 添加远程仓库（替换 YOUR_USERNAME 为你的GitHub用户名）
git remote add origin https://github.com/YOUR_USERNAME/programming-languages-research.git

# 或者使用SSH（如果你配置了SSH密钥）
git remote add origin git@github.com:YOUR_USERNAME/programming-languages-research.git
```

## 📤 推送到 GitHub

```bash
# 推送到远程仓库
git push -u origin main
```

## 🌐 启用 GitHub Pages

1. 在仓库页面，点击 "Settings" 标签
2. 在左侧菜单中找到 "Pages"
3. 在 "Source" 部分：
   - 选择 "Deploy from a branch"
   - Branch 选择 "main"
   - Folder 选择 "/ (root)"
4. 点击 "Save"

## 📱 访问你的网站

GitHub Pages 部署完成后，你的网站将在以下地址可用：
```
https://YOUR_USERNAME.github.io/programming-languages-research/
```

## 🔄 后续更新

每次修改代码后，使用以下命令更新网站：

```bash
git add .
git commit -m "更新描述"
git push origin main
```

GitHub Pages 会自动重新部署你的网站。

## 📋 项目结构说明

```
programming-languages-research/
├── index.html              # 主页面
├── css/                    # 样式文件
├── js/                     # JavaScript文件
├── projects/               # 研究项目目录
│   ├── C语言研究/          # C语言相关项目
│   ├── C++研究/            # C++相关项目
│   ├── Python研究/         # Python相关项目
│   ├── Shell研究/          # Shell相关项目
│   ├── 跨语言研究/         # 跨语言编程项目
│   ├── 性能研究/           # 性能优化项目
│   ├── 安全研究/           # 安全编程项目
│   └── 工具链研究/         # 开发工具项目
├── images/                 # 图片资源
└── README.md               # 项目说明
```

## 🎯 研究领域

- **C语言研究**: 内存管理、性能优化、系统编程
- **C++研究**: 现代特性、模板元编程、智能指针
- **Python研究**: 高级特性、性能优化、异步编程
- **Shell研究**: 脚本编程、系统自动化、性能优化
- **跨语言研究**: 语言间互操作、FFI接口、进程通信
- **性能研究**: 基准测试、性能对比、优化策略
- **安全研究**: 安全编程、内存安全、权限控制
- **工具链研究**: 编译器优化、调试工具、开发环境

## 🛠️ 技术特点

- 纯前端实现，无需后端服务
- 响应式设计，支持各种设备
- 智能搜索和分类筛选
- 现代化的Material Design风格
- 支持GitHub Pages免费托管

## 📞 支持

如果你遇到任何问题，可以：
1. 检查GitHub Pages的设置
2. 查看浏览器的开发者工具
3. 提交Issue到GitHub仓库

---

**祝你使用愉快！** 🚀
