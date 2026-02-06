#!/bin/bash
set -e

# Setup Emscripten paths directly
EMSDK_ROOT="/home/liuxiaopeng/emsdk"
source "${EMSDK_ROOT}/emsdk_env.sh"

# Define emcmake path if not in path (fallback)
if ! command -v emcmake &> /dev/null; then
    EMCMAKE="${EMSDK_ROOT}/upstream/emscripten/emcmake"
else
    EMCMAKE="emcmake"
fi

echo "Using emcmake: ${EMCMAKE}"

# Clean build directory
rm -rf build-wasm
mkdir -p build-wasm

# Configure with Emscripten
# -DENABLE_WASM=ON enables the WASM build target
# -DCMAKE_BUILD_TYPE=Release ensures optimizations
"${EMCMAKE}" cmake -B build-wasm -S . \
    -DENABLE_WASM=ON \
    -DCMAKE_BUILD_TYPE=Release

# Build
# -j automatically uses available cores
cmake --build build-wasm -- -j$(nproc)

echo "Build complete! Output is in wasm/dist/"
ls -lh wasm/dist/
