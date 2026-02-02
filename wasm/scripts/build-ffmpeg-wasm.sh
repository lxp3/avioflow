#!/bin/bash
# ============================================================================
# Build FFmpeg for WebAssembly using Emscripten
# ============================================================================
#
# Prerequisites:
#   - Emscripten SDK installed and activated (source emsdk_env.sh)
#   - Basic build tools (make, etc.)
#
# Usage:
#   ./build-ffmpeg-wasm.sh
#
# Output:
#   ../ffmpeg-wasm/include/  - Header files
#   ../ffmpeg-wasm/lib/      - Static libraries (.a)
#
# ============================================================================

set -e

# Configuration
FFMPEG_VERSION="7.1"
FFMPEG_URL="https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/ffmpeg-build"
OUTPUT_DIR="${SCRIPT_DIR}/../ffmpeg-wasm"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo ""
echo "============================================"
echo "  FFmpeg ${FFMPEG_VERSION} WASM Build"
echo "============================================"
echo ""

# Check Emscripten
if ! command -v emcc &> /dev/null; then
    echo -e "${RED}Error: Emscripten not found!${NC}"
    echo ""
    echo "Please install and activate Emscripten SDK:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk && ./emsdk install latest && ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    exit 1
fi

EMCC_VERSION=$(emcc --version | head -n1)
echo -e "${GREEN}Emscripten: ${EMCC_VERSION}${NC}"
echo ""

# Create directories
mkdir -p "${BUILD_DIR}"
mkdir -p "${OUTPUT_DIR}/include"
mkdir -p "${OUTPUT_DIR}/lib"

cd "${BUILD_DIR}"

# Download FFmpeg if not exists
if [ ! -d "ffmpeg-${FFMPEG_VERSION}" ]; then
    echo -e "${YELLOW}[1/4] Downloading FFmpeg ${FFMPEG_VERSION}...${NC}"
    if [ ! -f "ffmpeg-${FFMPEG_VERSION}.tar.xz" ]; then
        curl -L -o "ffmpeg-${FFMPEG_VERSION}.tar.xz" "${FFMPEG_URL}"
    fi
    tar xf "ffmpeg-${FFMPEG_VERSION}.tar.xz"
else
    echo -e "${GREEN}[1/4] FFmpeg source already exists${NC}"
fi

cd "ffmpeg-${FFMPEG_VERSION}"

# Configure FFmpeg for WASM
echo ""
echo -e "${YELLOW}[2/4] Configuring FFmpeg for WebAssembly...${NC}"
echo ""

# Clean previous build
make clean 2>/dev/null || true

# Configure with minimal features for audio decoding
emconfigure ./configure \
    --prefix="${OUTPUT_DIR}" \
    --enable-cross-compile \
    --target-os=none \
    --arch=x86 \
    --disable-runtime-cpudetect \
    --disable-asm \
    --disable-fast-unaligned \
    --disable-pthreads \
    --disable-w32threads \
    --disable-os2threads \
    --disable-debug \
    --disable-stripping \
    --disable-safe-bitstream-reader \
    \
    --disable-all \
    --enable-avcodec \
    --enable-avformat \
    --enable-avutil \
    --enable-swresample \
    \
    --enable-protocol=file \
    --enable-protocol=pipe \
    \
    --enable-demuxer=mp3 \
    --enable-demuxer=wav \
    --enable-demuxer=aac \
    --enable-demuxer=flac \
    --enable-demuxer=ogg \
    --enable-demuxer=mov \
    --enable-demuxer=matroska \
    --enable-demuxer=pcm_s16le \
    --enable-demuxer=pcm_s16be \
    --enable-demuxer=pcm_f32le \
    \
    --enable-decoder=mp3 \
    --enable-decoder=mp3float \
    --enable-decoder=aac \
    --enable-decoder=aac_fixed \
    --enable-decoder=flac \
    --enable-decoder=vorbis \
    --enable-decoder=opus \
    --enable-decoder=pcm_s16le \
    --enable-decoder=pcm_s16be \
    --enable-decoder=pcm_f32le \
    --enable-decoder=pcm_f32be \
    --enable-decoder=pcm_s24le \
    --enable-decoder=pcm_s32le \
    --enable-decoder=pcm_u8 \
    --enable-decoder=pcm_alaw \
    --enable-decoder=pcm_mulaw \
    \
    --enable-parser=aac \
    --enable-parser=mp3 \
    --enable-parser=flac \
    --enable-parser=vorbis \
    --enable-parser=opus \
    \
    --disable-programs \
    --disable-doc \
    --disable-htmlpages \
    --disable-manpages \
    --disable-podpages \
    --disable-txtpages \
    \
    --disable-network \
    --disable-iconv \
    --disable-sdl2 \
    --disable-vaapi \
    --disable-vdpau \
    --disable-videotoolbox \
    --disable-audiotoolbox \
    --disable-appkit \
    --disable-coreimage \
    --disable-avfoundation \
    --disable-securetransport \
    \
    --nm="llvm-nm" \
    --ar="emar" \
    --ranlib="emranlib" \
    --cc="emcc" \
    --cxx="em++" \
    --objcc="emcc" \
    --dep-cc="emcc" \
    \
    --extra-cflags="-O3 -fno-exceptions" \
    --extra-cxxflags="-O3 -fno-exceptions"

# Build
echo ""
echo -e "${YELLOW}[3/4] Building FFmpeg (this may take a while)...${NC}"
echo ""

emmake make -j$(nproc 2>/dev/null || echo 4)

# Install
echo ""
echo -e "${YELLOW}[4/4] Installing to ${OUTPUT_DIR}...${NC}"
echo ""

emmake make install

# Summary
echo ""
echo "============================================"
echo -e "${GREEN}  FFmpeg WASM Build Complete!${NC}"
echo "============================================"
echo ""
echo "Output directory: ${OUTPUT_DIR}"
echo ""
echo "Libraries built:"
ls -lh "${OUTPUT_DIR}/lib/"*.a 2>/dev/null || echo "  (no .a files found)"
echo ""
echo "You can now build avioflow WASM:"
echo "  cd avioflow && ./wasm/build.ps1"
echo ""
