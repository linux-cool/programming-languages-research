// Main JavaScript functionality for Research Portfolio

class ResearchPortfolio {
    constructor() {
        this.projects = [];

        this.init();
    }
    
    async init() {
        await this.loadProjects();
        this.setupEventListeners();
        this.renderProjects();
        this.updateStats();
    }
    
    async loadProjects() {
        try {
            const response = await fetch('projects/projects.json');
            this.projects = await response.json();
            console.log('成功加载', this.projects.length, '个项目');
        } catch (error) {
            console.error('Error loading projects:', error);
            // Fallback to sample data if JSON file is not available
            this.projects = this.getSampleProjects();
            this.filteredProjects = [...this.projects];
        }
    }
    
    getSampleProjects() {
        return [
            {
                id: 1,
                title: "C语言内存管理与性能优化深度研究",
                description: "深入研究C语言内存管理机制，包括动态内存分配、内存泄漏检测、性能优化等核心技术，为系统级编程提供最佳实践。",
                category: "C语言研究",
                year: 2024,
                tags: ["C语言", "内存管理", "性能优化", "系统编程"],
                image: "⚡",
                details: "C语言内存管理项目专注于系统级编程的核心技术。我们深入研究了C语言的内存模型和性能优化策略，实现了高效的内存池管理算法和内存泄漏检测工具。",
                folder: "C语言研究"
            },
            {
                id: 2,
                title: "C++现代特性与模板元编程研究",
                description: "探索C++11/14/17/20现代特性，深入研究模板元编程、智能指针、移动语义等高级技术，提升C++代码质量和性能。",
                category: "C++研究",
                year: 2024,
                tags: ["C++", "模板元编程", "智能指针", "移动语义"],
                image: "🚀",
                details: "C++现代特性研究项目深入探索了C++的高级编程技术。我们基于最新的C++标准，实现了多种现代编程模式，包括模板元编程的类型计算和编译期优化。",
                folder: "C++研究"
            },
            {
                id: 3,
                title: "Python高级编程与性能优化研究",
                description: "研究Python高级编程技术，包括装饰器、生成器、异步编程、性能优化等，探索Python在科学计算和Web开发中的应用。",
                category: "Python研究",
                year: 2024,
                tags: ["Python", "装饰器", "生成器", "异步编程"],
                image: "🐍",
                details: "Python高级编程研究项目专注于Python的高级特性和性能优化。我们深入研究了Python的内部机制和最佳实践，实现了装饰器模式、生成器优化和异步编程性能调优。",
                folder: "Python研究"
            },
            {
                id: 4,
                title: "Shell脚本编程与系统自动化研究",
                description: "深入研究Shell脚本编程技术，包括Bash高级特性、系统自动化、性能优化等，为Linux系统管理和运维提供解决方案。",
                category: "Shell研究",
                year: 2024,
                tags: ["Shell", "Bash", "系统自动化", "性能优化"],
                image: "🐚",
                details: "Shell脚本编程研究项目专注于Linux系统管理和自动化。我们开发了完整的Shell编程工具链，包括高级Bash脚本编程技巧、系统监控和自动化脚本。",
                folder: "Shell研究"
            },
            {
                id: 5,
                title: "跨语言编程接口与互操作研究",
                description: "研究C/C++/Python/Shell之间的互操作技术，包括FFI、扩展模块、进程间通信等，实现多语言协同编程。",
                category: "跨语言研究",
                year: 2024,
                tags: ["跨语言", "FFI", "扩展模块", "进程间通信"],
                image: "🔗",
                details: "跨语言编程研究项目探索了不同编程语言之间的协作模式。我们实现了完整的跨语言编程框架，包括C/C++扩展模块开发、Python与C/C++的FFI接口。",
                folder: "跨语言研究"
            },
            {
                id: 6,
                title: "编程语言性能基准测试与优化",
                description: "建立C/C++/Python/Shell的性能基准测试体系，研究不同语言在算法实现、内存使用、执行效率等方面的差异。",
                category: "性能研究",
                year: 2024,
                tags: ["性能测试", "基准测试", "算法优化", "内存分析"],
                image: "📊",
                details: "性能基准测试项目建立了科学的编程语言性能评估体系。我们开发了全面的性能测试框架，包括标准算法性能基准测试、内存使用和GC性能分析。",
                folder: "性能研究"
            }
        ];
    }
    
    setupEventListeners() {

        
        // Modal functionality
        this.setupModal();
    }
    
    setupNavigation() {
        const navToggle = document.getElementById('nav-toggle');
        const navMenu = document.getElementById('nav-menu');
        
        navToggle.addEventListener('click', () => {
            navMenu.classList.toggle('active');
            navToggle.classList.toggle('active');
        });
        
        // Close mobile menu when clicking on a link
        const navLinks = document.querySelectorAll('.nav-link');
        navLinks.forEach(link => {
            link.addEventListener('click', () => {
                navMenu.classList.remove('active');
                navToggle.classList.remove('active');
            });
        });
    }
    
    setupModal() {
        const modal = document.getElementById('project-modal');
        const closeBtn = document.querySelector('.close');
        
        closeBtn.addEventListener('click', () => {
            modal.style.display = 'none';
        });
        
        window.addEventListener('click', (e) => {
            if (e.target === modal) {
                modal.style.display = 'none';
            }
        });
    }
    

    



    

    
    renderProjects() {
        const projectsGrid = document.getElementById('projects-grid');

        if (this.projects.length === 0) {
            projectsGrid.innerHTML = '<p>正在加载项目...</p>';
            return;
        }

        projectsGrid.innerHTML = this.projects.map(project => `
            <div class="project-card">
                <div class="project-image">
                    ${project.image}
                </div>
                <div class="project-content">
                    <h3 class="project-title">${project.title}</h3>
                    <p class="project-description">${project.description}</p>
                    <div class="project-meta">
                        <span class="project-category">${project.category}</span>
                        <span class="project-year">${project.year}</span>
                    </div>
                    <div class="project-tags">
                        ${project.tags.map(tag => `<span class="project-tag">${tag}</span>`).join('')}
                    </div>
                    <div class="project-actions">
                        <button class="btn btn-primary" onclick="showProjectDetails(${project.id})">
                            <span class="btn-icon">📋</span>
                            查看详情
                        </button>
                        <button class="btn btn-secondary" onclick="viewProjectContent('${project.folder}')">
                            <span class="btn-icon">📖</span>
                            查看内容
                        </button>
                    </div>
                </div>
            </div>
        `).join('');
        
        console.log('渲染了', this.filteredProjects.length, '个项目卡片');
    }
    
    bindProjectButtonEvents() {
        const projectsGrid = document.getElementById('projects-grid');
        
        // 绑定查看详情按钮事件
        projectsGrid.addEventListener('click', (e) => {
            if (e.target.closest('.view-details-btn')) {
                const projectId = parseInt(e.target.closest('.view-details-btn').dataset.projectId);
                this.showProjectDetails(projectId);
            }
            
            if (e.target.closest('.view-content-btn')) {
                const folder = e.target.closest('.view-content-btn').dataset.folder;
                this.viewProjectContent(folder);
            }
        });
    }
    
    showProjectDetails(projectId) {
        const project = this.projects.find(p => p.id === projectId);
        if (!project) return;
        
        const modal = document.getElementById('project-modal');
        const modalContent = document.getElementById('modal-content');
        
        modalContent.innerHTML = `
            <div class="project-details">
                <h2>${project.title}</h2>
                <div class="project-meta-details">
                    <span class="project-category">${project.category}</span>
                    <span class="project-year">${project.year}</span>
                </div>
                <p class="project-description-full">${project.details}</p>
                <div class="project-tags-full">
                    <strong>技术标签：</strong>
                    ${project.tags.map(tag => `<span class="project-tag">${tag}</span>`).join('')}
                </div>
                <div class="project-actions-modal">
                    ${project.folder ? `
                        <button class="btn btn-primary" onclick="viewProjectContent('${project.folder}')">
                            <span class="btn-icon">📖</span>
                            查看项目内容
                        </button>
                    ` : ''}
                    <button class="btn btn-secondary" onclick="closeModal()">
                        <span class="btn-icon">✕</span>
                        关闭
                    </button>
                </div>
            </div>
        `;
        
        modal.style.display = 'block';
    }
    
    updateStats() {
        const projectCount = document.getElementById('project-count');
        const categoryCount = document.getElementById('category-count');
        const yearCount = document.getElementById('year-count');
        
        if (projectCount) projectCount.textContent = this.projects.length;
        if (categoryCount) categoryCount.textContent = [...new Set(this.projects.map(p => p.category))].length;
        if (yearCount) yearCount.textContent = [...new Set(this.projects.map(p => p.year))].length;
    }

    viewProjectContent(folderName) {
        // 构建文件夹路径
        const folderPath = `projects/${folderName}`;
        
        // 检查文件夹是否存在并显示内容
        fetch(folderPath + '/README.md')
            .then(response => {
                if (response.ok) {
                    // 如果文件夹存在，显示项目内容
                    this.showProjectContent(folderPath, folderName);
                } else {
                    // 如果文件夹不存在，显示提示
                    this.showFolderNotFoundMessage(folderName);
                }
            })
            .catch(error => {
                console.error('Error checking folder:', error);
                this.showFolderNotFoundMessage(folderName);
            });
    }
    
    showProjectContent(folderPath, folderName) {
        // 显示项目内容模态框
        const modal = document.getElementById('project-modal');
        const modalContent = document.getElementById('modal-content');
        
        // 先显示加载状态
        modalContent.innerHTML = `
            <div class="project-content-loading">
                <h3>📖 正在加载项目内容...</h3>
                <div class="loading-spinner"></div>
            </div>
        `;
        modal.style.display = 'block';
        
        // 读取README.md内容
        fetch(folderPath + '/README.md')
            .then(response => response.text())
            .then(content => {
                // 将Markdown内容转换为HTML（简单处理）
                const htmlContent = this.convertMarkdownToHtml(content);
                
                modalContent.innerHTML = `
                    <div class="project-content-view">
                        <div class="content-header">
                            <h2>📖 ${folderName}</h2>
                            <button class="btn btn-secondary" onclick="closeModal()">
                                <span class="btn-icon">✕</span>
                                关闭
                            </button>
                        </div>
                        <div class="content-body">
                            <div class="readme-content">
                                ${htmlContent}
                            </div>
                        </div>
                        <div class="content-footer">
                            <button class="btn btn-primary" onclick="showFolderStructure('${folderPath}')">
                                <span class="btn-icon">📁</span>
                                查看文件结构
                            </button>
                            <button class="btn btn-secondary" onclick="closeModal()">
                                <span class="btn-icon">←</span>
                                返回项目详情
                            </button>
                        </div>
                    </div>
                `;
            })
            .catch(error => {
                console.error('Error loading project content:', error);
                modalContent.innerHTML = `
                    <div class="project-content-error">
                        <h3>❌ 加载失败</h3>
                        <p>无法加载项目内容：${error.message}</p>
                        <button class="btn btn-secondary" onclick="closeModal()">
                            <span class="btn-icon">✕</span>
                            关闭
                        </button>
                    </div>
                `;
            });
    }
    
    convertMarkdownToHtml(markdown) {
        // 简单的Markdown转HTML转换
        return markdown
            // 标题
            .replace(/^### (.*$)/gim, '<h3>$1</h3>')
            .replace(/^## (.*$)/gim, '<h2>$1</h2>')
            .replace(/^# (.*$)/gim, '<h1>$1</h1>')
            // 粗体和斜体
            .replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>')
            .replace(/\*(.*?)\*/g, '<em>$1</em>')
            // 代码块
            .replace(/```([\s\S]*?)```/g, '<pre><code>$1</code></pre>')
            .replace(/`([^`]+)`/g, '<code>$1</code>')
            // 链接
            .replace(/\[([^\]]+)\]\(([^)]+)\)/g, '<a href="$2" target="_blank">$1</a>')
            // 列表
            .replace(/^\* (.*$)/gim, '<li>$1</li>')
            .replace(/^- (.*$)/gim, '<li>$1</li>')
            // 段落
            .replace(/\n\n/g, '</p><p>')
            .replace(/^(?!<[h|li|pre|ul|ol]).*$/gm, '<p>$&</p>')
            // 清理多余的标签
            .replace(/<p><\/p>/g, '')
            .replace(/<p>(<[h|li|pre|ul|ol])/g, '$1')
            .replace(/(<\/[h|li|pre|ul|ol]>)<\/p>/g, '$1');
    }
    
    copyFolderPath(folderPath) {
        // 复制文件夹路径到剪贴板
        navigator.clipboard.writeText(folderPath).then(() => {
            this.showToast('文件夹路径已复制到剪贴板！', 'success');
        }).catch(() => {
            // 如果剪贴板API不可用，使用传统方法
            const textArea = document.createElement('textarea');
            textArea.value = folderPath;
            document.body.appendChild(textArea);
            textArea.select();
            document.execCommand('copy');
            document.body.removeChild(textArea);
            this.showToast('文件夹路径已复制到剪贴板！', 'success');
        });
    }
    
    openInFileManager(folderPath) {
        // 尝试使用系统默认文件管理器打开
        try {
            // 在Linux系统上，尝试使用xdg-open
            if (navigator.platform.indexOf('Linux') !== -1) {
                // 这里可以尝试调用系统命令，但需要用户授权
                this.showToast('请在终端中运行: xdg-open ' + folderPath, 'info');
            } else {
                this.showToast('请手动在文件管理器中打开: ' + folderPath, 'info');
            }
        } catch (error) {
            this.showToast('无法自动打开文件管理器，请手动导航到: ' + folderPath, 'info');
        }
    }
    
    showFolderStructure(folderPath) {
        // 显示文件夹结构
        const modal = document.getElementById('project-modal');
        const modalContent = document.getElementById('modal-content');
        
        modalContent.innerHTML = `
            <div class="folder-structure-view">
                <div class="content-header">
                    <h2>📁 文件结构</h2>
                    <button class="btn btn-secondary" onclick="closeModal()">
                        <span class="btn-icon">✕</span>
                        关闭
                    </button>
                </div>
                <div class="content-body">
                    <div class="folder-tree">
                        <div class="folder-item">
                            <span class="folder-icon">📁</span>
                            <span class="folder-name">${folderPath.split('/').pop()}</span>
                        </div>
                        <div class="file-item">
                            <span class="file-icon">📄</span>
                            <span class="file-name">README.md</span>
                        </div>
                        <div class="file-item">
                            <span class="file-icon">📄</span>
                            <span class="file-name">项目文档</span>
                        </div>
                        <div class="file-item">
                            <span class="file-icon">📄</span>
                            <span class="file-name">技术方案</span>
                        </div>
                    </div>
                </div>
                <div class="content-footer">
                    <button class="btn btn-primary" onclick="viewProjectContent('${folderPath.split('/').pop()}')">
                        <span class="btn-icon">📖</span>
                        返回项目内容
                    </button>
                    <button class="btn btn-secondary" onclick="closeModal()">
                        <span class="btn-icon">✕</span>
                        关闭
                    </button>
                </div>
            </div>
        `;
    }
    
    async showFolderContents(folderPath) {
        try {
            // 尝试读取文件夹内容
            const response = await fetch(folderPath + '/README.md');
            if (response.ok) {
                const content = await response.text();
                const message = `
                    <div class="folder-contents">
                        <h3>📁 ${folderPath.split('/').pop()} 文件夹内容</h3>
                        <div class="readme-content">
                            <pre>${content}</pre>
                        </div>
                        <div class="folder-actions">
                            <button class="btn btn-primary" onclick="copyFolderPath('${folderPath}')">
                                📋 复制路径
                            </button>
                            <button class="btn btn-secondary" onclick="closeCustomModal()">
                                关闭
                            </button>
                        </div>
                    </div>
                `;
                this.showCustomModal('文件夹内容', message);
            } else {
                this.showToast('无法读取文件夹内容', 'error');
            }
        } catch (error) {
            this.showToast('读取文件夹内容时出错', 'error');
        }
    }
    
    showFolderNotFoundMessage(folderName) {
        this.showToast(`文件夹 "${folderName}" 不存在或无法访问`, 'error');
    }
    
    showCustomModal(title, content) {
        // 创建自定义模态框
        let modal = document.getElementById('custom-modal');
        if (!modal) {
            modal = document.createElement('div');
            modal.id = 'custom-modal';
            modal.className = 'modal custom-modal';
            modal.innerHTML = `
                <div class="modal-content custom-modal-content">
                    <div class="modal-header">
                        <h2 id="custom-modal-title"></h2>
                        <span class="close" onclick="closeCustomModal()">&times;</span>
                    </div>
                    <div id="custom-modal-body"></div>
                </div>
            `;
            document.body.appendChild(modal);
        }
        
        document.getElementById('custom-modal-title').textContent = title;
        document.getElementById('custom-modal-body').innerHTML = content;
        modal.style.display = 'block';
    }
    
    closeCustomModal() {
        const modal = document.getElementById('custom-modal');
        if (modal) {
            modal.style.display = 'none';
        }
    }
    
    closeModal() {
        const modal = document.getElementById('project-modal');
        if (modal) {
            modal.style.display = 'none';
        }
    }
    
    showToast(message, type = 'info') {
        // 创建toast通知
        const toast = document.createElement('div');
        toast.className = `toast toast-${type}`;
        toast.innerHTML = `
            <span class="toast-message">${message}</span>
            <button class="toast-close" onclick="this.parentElement.remove()">&times;</button>
        `;
        
        document.body.appendChild(toast);
        
        // 自动移除
        setTimeout(() => {
            if (toast.parentElement) {
                toast.remove();
            }
        }, 5000);
    }
}

// Initialize the portfolio when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    window.portfolio = new ResearchPortfolio();
});

// Global functions for button clicks
function showProjectDetails(projectId) {
    if (window.portfolio) {
        window.portfolio.showProjectDetails(projectId);
    }
}

function viewProjectContent(folder) {
    if (window.portfolio) {
        window.portfolio.viewProjectContent(folder);
    }
}

function closeModal() {
    if (window.portfolio) {
        window.portfolio.closeModal();
    }
}

function closeCustomModal() {
    if (window.portfolio) {
        window.portfolio.closeCustomModal();
    }
}

function showFolderStructure(folderPath) {
    if (window.portfolio) {
        window.portfolio.showFolderStructure(folderPath);
    }
}

function copyFolderPath(folderPath) {
    if (window.portfolio) {
        window.portfolio.copyFolderPath(folderPath);
    }
}

// Smooth scrolling for navigation links
document.querySelectorAll('a[href^="#"]').forEach(anchor => {
    anchor.addEventListener('click', function (e) {
        e.preventDefault();
        const target = document.querySelector(this.getAttribute('href'));
        if (target) {
            target.scrollIntoView({
                behavior: 'smooth',
                block: 'start'
            });
        }
    });
});

// Add loading animation
window.addEventListener('load', () => {
    document.body.classList.add('loaded');
});

// Add scroll effect for header
window.addEventListener('scroll', () => {
    const header = document.querySelector('.header');
    if (window.scrollY > 100) {
        header.style.background = 'rgba(255, 255, 255, 0.98)';
    } else {
        header.style.background = 'rgba(255, 255, 255, 0.95)';
    }
});