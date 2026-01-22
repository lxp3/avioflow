#!/bin/bash

# 获取脚本所在的绝对路径
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# 项目根目录
PROJECT_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
# 编译产物目录
BUILD_BIN="$PROJECT_ROOT/build/bin"

# 检查编译目录是否存在
if [ ! -d "$BUILD_BIN" ]; then
    echo "错误: 未找到编译目录 $BUILD_BIN"
    echo "请先进行编译: ./build.sh"
    exit 1
fi

# 设置环境变量
# PYTHONPATH 指向 build/bin，这样可以找到 avioflow 文件夹
export PYTHONPATH="$BUILD_BIN:$PYTHONPATH"

# 寻找 FFmpeg 库路径 (FetchContent 下载的位置)
FFMPEG_LIB_PATH=$(find "$PROJECT_ROOT/build/_deps" -name "libavcodec.so*" -exec dirname {} \; | head -n 1)

# LD_LIBRARY_PATH 指向 build/bin (包含 libavioflow.so) 和 FFmpeg 库目录
if [ -n "$FFMPEG_LIB_PATH" ]; then
    export LD_LIBRARY_PATH="$BUILD_BIN:$FFMPEG_LIB_PATH:$LD_LIBRARY_PATH"
else
    export LD_LIBRARY_PATH="$BUILD_BIN:$LD_LIBRARY_PATH"
fi

# 如果没有提供参数，默认使用项目中的测试文件
DEFAULT_AUDIO="$PROJECT_ROOT/public/TownTheme.mp3"
AUDIO_FILE=${1:-$DEFAULT_AUDIO}

echo "--- Python 环境测试 ---"
echo "PYTHONPATH: $PYTHONPATH"
echo "LD_LIBRARY_PATH: $LD_LIBRARY_PATH"
echo "测试文件: $AUDIO_FILE"
echo "----------------------"

# 运行测试
python3 "$SCRIPT_DIR/test_audio_load.py" "$AUDIO_FILE"
