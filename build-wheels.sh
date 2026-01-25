#!/bin/bash

# avioflow Wheel Build Script for Linux
# This script packages the project into a standard Python wheel (.whl) for local testing.

set -e

# 1. Check for build tools
echo "Checking for build tools..."
USE_UV=false

if command -v uv &> /dev/null; then
    echo "Found uv! Using 'uv build' for a faster and more reliable process."
    USE_UV=true
elif ! command -v python3 &> /dev/null && ! command -v python &> /dev/null; then
    echo "Error: Neither 'uv' nor 'python' found. Please install one of them."
    exit 1
fi

PYTHON_CMD=$(command -v python3 || command -v python)

# 2. Configuration & Clean
BUILD_DIR="build_py"

if [ -d "dist" ]; then
    echo "Cleaning dist/ directory..."
    rm -rf dist
fi

if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning $BUILD_DIR directory..."
    rm -rf "$BUILD_DIR"
fi

# 3. Build the wheel
echo -e "\n--- Packaging avioflow into Wheel ---"

if [ "$USE_UV" = true ]; then
    # -C/--config-setting allows passing arguments to scikit-build-core
    uv build --wheel "-Cbuild-dir=$BUILD_DIR" -v
else
    echo "uv not found, falling back to 'pip wheel'."
    $PYTHON_CMD -m pip wheel . --wheel-dir dist --no-deps "-Cbuild-dir=$BUILD_DIR"
fi

# 4. Repair the wheel with auditwheel
WHEEL_FILE=$(ls dist/*.whl 2>/dev/null | grep -v "manylinux" | head -n 1)

if [ -n "$WHEEL_FILE" ]; then
    echo -e "\n--- Repairing Wheel with auditwheel ---"
    if command -v auditwheel &> /dev/null; then
        # Repair the wheel and place it in dist/
        # auditwheel will automatically detect the appropriate manylinux tag
        auditwheel repair "$WHEEL_FILE" -w dist/
        
        # Remove the original non-compliant wheel
        rm "$WHEEL_FILE"
        echo "Wheel repaired and tagged as manylinux."
    else
        echo "Warning: 'auditwheel' not found. Skipping repair. The wheel may not be installable on all systems."
    fi
fi

# 5. Success message and location
WHEEL_FILE=$(ls dist/*manylinux*.whl 2>/dev/null | head -n 1)
if [ -z "$WHEEL_FILE" ]; then
    WHEEL_FILE=$(ls dist/*.whl 2>/dev/null | head -n 1)
fi

if [ -n "$WHEEL_FILE" ]; then
    echo -e "\n--- Wheel Build Successful! ---"
    echo "Wheel location: $WHEEL_FILE"
    echo -e "\nYou can now install it using:"
    echo "$PYTHON_CMD -m pip install $WHEEL_FILE --force-reinstall"
else
    echo "Error: Wheel file not found in dist/ directory."
    exit 1
fi
