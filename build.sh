#!/bin/bash

# avioflow Build Script
# This script configures and builds the project using CMake on Ubuntu

set -e

# Configuration
BUILD_SHARED_LIBS="ON"
BUILD_DIR="build"

# Run CMake Configuration
echo "Configuring project ($BUILD_DIR)..."
cmake -B "$BUILD_DIR" -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DAVIOFLOW_ENABLE_WASAPI=ON \
    -DAVIOFLOW_ENABLE_BINARY=ON \
    -DAVIOFLOW_ENABLE_PYTHON=ON \
    -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS} \
    -DAVIOFLOW_ENABLE_NODE_JS=OFF

# Build the project
cmake --build "$BUILD_DIR" --config Release
