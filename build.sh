#!/bin/bash

# avioflow Build Script
# This script configures and builds the project using CMake on Ubuntu

set -e

# Configuration
BUILD_SHARED_LIBS="OFF"
if [ "$BUILD_SHARED_LIBS" = "ON" ]; then
    BUILD_DIR="build_shared"
else
    BUILD_DIR="build_static"
fi

# Run CMake Configuration
echo "Configuring project ($BUILD_DIR)..."
cmake -B "$BUILD_DIR" -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_WASAPI=ON \
    -DENABLE_BINARY=ON \
    -DENABLE_PYTHON=ON \
    -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS} \
    -DENABLE_NODE_JS=OFF

# Build the project
cmake --build "$BUILD_DIR" --config Release

# Package Python Wheel if enabled
if [[ "$*" == *"-DENABLE_PYTHON=ON"* ]] || grep -q "ENABLE_PYTHON:BOOL=ON" "$BUILD_DIR/CMakeCache.txt"; then
    echo -e "\n--- Building Python Wheel ---"

    # Use uv if available, otherwise fallback to pip
    if command -v uv &> /dev/null; then
        (cd python && uv build --wheel --out-dir dist)
    else
        echo "uv not found, using pip..."
        python3 -m pip wheel ./python -w ./python/dist --no-deps
    fi

    echo -e "\n--- Wheel Build Check ---"
    if ls python/dist/*.whl 1> /dev/null 2>&1; then
        ls -lh python/dist/*.whl
        # Optional: Auditwheel check (if available) for manylinux compliance
        if command -v auditwheel &> /dev/null; then
             echo "Checking wheel compliance..."
             # Redirect stderr to suppress "bad interpreter" errors from broken environments
             if ! auditwheel show python/dist/*.whl 2>/dev/null | head -n 5; then
                echo "Warning: Skipped auditwheel check (tool configuration issue in this container)."
             fi
        fi
    else
        echo "Error: No wheel file generated in python/dist/"
    fi
fi