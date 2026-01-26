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
cmake -B "$BUILD_DIR" -S . -DCMAKE_BUILD_TYPE=Release -DENABLE_WASAPI=ON -DENABLE_PYTHON=ON -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS} -DENABLE_NODE_JS=OFF

# Build the project
cmake --build "$BUILD_DIR" --config Release