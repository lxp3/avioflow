#!/bin/bash

# avioflow Build Script
# This script configures and builds the project using CMake on Ubuntu

set -e

# Configuration
BUILD_DIR="build"

# Run CMake Configuration
echo "Configuring project..."
cmake -B "$BUILD_DIR" -S . -DCMAKE_BUILD_TYPE=Release -DENABLE_WASAPI=ON -DENABLE_PYTHON=ON

# Build the project
cmake --build "$BUILD_DIR" --config Release