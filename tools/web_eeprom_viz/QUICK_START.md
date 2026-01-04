# EEPROM可视化平台 - 快速启动指南

## 🚀 快速开始

### 1. 启动服务器

```bash
cd tools/web_eeprom_viz
uv run python app.py
```

或者使用普通Python:

```bash
cd tools/web_eeprom_viz
python3 app.py
```

### 2. 访问平台

打开浏览器访问:

```
http://localhost:5000
```

### 3. 推荐学习路径

#### 🎯 快速体验（30分钟）
1. **模块2+**: 擦除-写入依赖 (5分钟) - http://localhost:5000/erase_write_demo_v2
2. **模块3+**: 写放大现象 (8分钟) - http://localhost:5000/write_amplification_v2
3. **模块4+**: 磨损均衡 (10分钟) - http://localhost:5000/wear_leveling_v2

#### 🎮 完整体验（90分钟）
1. **模块2+** - 陷阱实验
2. **模块3+** - Page网格可视化
3. **模块4+** - Page寿命竞赛
4. **模块5+** - 数据破坏侦探（3关）
5. **模块6+** - Job调度指挥官
6. **模块7+** - 掉电拯救者
7. **模块8+** - 数据保险柜

---

## 📋 模块清单

### ⭐ 推荐交互式模块（V2版本）

| 模块 | 标题 | 时间 | 特点 | 链接 |
|-----|------|------|------|------|
| 2+ | 擦除-写入依赖 | 5分钟 | 陷阱实验 | `/erase_write_demo_v2` |
| 3+ | 写放大现象 | 8分钟 | 网格可视化 | `/write_amplification_v2` |
| 4+ | 磨损均衡 | 10分钟 | 寿命竞赛 | `/wear_leveling_v2` |
| 5+ | 位翻转故障 | 10分钟 | 侦探游戏 | `/bit_flip_v2` |
| 6+ | NvM队列 | 10分钟 | Job调度 | `/nvm_queue_v2` |
| 7+ | WriteAll一致性 | 12分钟 | 掉电测试 | `/writeall_consistency_v2` |
| 8+ | Block类型 | 12分钟 | 灾难挑战 | `/block_types_v2` |

### 📚 原版模块（知识呈现）

| 模块 | 标题 | 链接 |
|-----|------|------|
| 1 | 物理结构 | `/physical` |
| 2 | 擦除-写入 | `/erase_write_demo` |
| 3 | 写放大 | `/write_amplification` |
| 4 | 磨损均衡 | `/wear_leveling` |
| 5 | 位翻转 | `/bit_flip` |
| 6 | NvM队列 | `/nvm_queue` |
| 7 | WriteAll | `/writeall_consistency` |
| 8 | Block类型 | `/block_types` |

---

## 🎨 新特性

### UI美化 (v1.3.0)
- ✅ 现代化导航栏（下拉菜单）
- ✅ 渐变Hero Section
- ✅ 卡片式模块展示
- ✅ 统一的设计系统
- ✅ 响应式适配

### 交互式学习 (v1.2.0-1.3.0)
- ✅ 100%交互式覆盖率
- ✅ 7个V2模块
- ✅ 游戏化设计
- ✅ 实时动画反馈
- ✅ 可视化网格

---

## 🛠️ 技术栈

- **后端**: Python Flask 3.1.2
- **前端**: Bootstrap 5.3.0
- **样式**: 自定义CSS（渐变、动画）
- **架构**: RESTful APIs
- **包管理**: uv

---

## 📊 项目统计

- **总代码量**: ~5,000行
- **V2交互式代码**: ~3,100行
- **V2模块数**: 7个
- **交互覆盖率**: 100%
- **平均学习时间**: 30-90分钟

---

## 🔧 故障排查

### 端口被占用
```bash
# 使用其他端口
python3 app.py
# 然后访问显示的端口（通常是5000）
```

### 依赖缺失
```bash
# 安装依赖
cd tools/web_eeprom_viz
uv pip install flask
```

### 模块无法加载
- 检查app.py中的路由是否正确
- 确认templates目录下存在对应的html文件
- 查看浏览器控制台的错误信息

---

## 📞 获取帮助

### 文档
- `README.md` - 项目说明
- `INTERACTIVE_PHASE3_REPORT.md` - Phase 3报告
- `UI_ENHANCEMENT_SUMMARY.md` - UI美化报告

### 反馈
- 提交Issue
- 查看文档
- 检查日志

---

## ✨ 开始体验

现在就启动服务器，开始你的EEPROM交互式学习之旅！

```bash
cd tools/web_eeprom_viz
uv run python app.py
```

然后访问: http://localhost:5000

🎮 **祝你学习愉快！**
