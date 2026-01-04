#!/bin/bash
# EEPROM可视化仿真平台 - 启动脚本

echo "🚀 启动EEPROM可视化仿真平台..."
echo ""

# 检查uv是否安装
if ! command -v uv &> /dev/null; then
    echo "❌ 错误: uv未安装"
    echo "请安装uv: curl -LsSf https://astral.sh/uv/install.sh | sh"
    exit 1
fi

# 检查依赖
echo "📦 检查依赖..."
if [ ! -d ".venv" ]; then
    echo "正在安装依赖..."
    uv add Flask crcmod
fi

echo ""
echo "✅ 启动Flask服务器..."
echo "📍 访问地址: http://localhost:5000"
echo "📍 物理结构: http://localhost:5000/physical"
echo "📍 擦除-写入演示: http://localhost:5000/erase_write_demo"
echo ""
echo "按 Ctrl+C 停止服务器"
echo ""

# 启动服务器
uv run python app.py
