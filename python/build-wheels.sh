#!/bin/bash

# avioflow Wheel Build Script for Linux
# This script packages the project into a standard Python wheel (.whl) for local testing.

set -e

# Get the script directory (python/) and project root (parent dir)
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(dirname "$SCRIPT_DIR")

# 1. Check for build tools
echo "Checking for build tools..."
USE_UV=false

if command -v uv &> /dev/null; then
    echo "Found uv! Using 'uv build' for a faster and more reliable process."
    USE_UV=true
fi

# 2. Configuration & Clean
BUILD_DIR="$PROJECT_ROOT/build_py"

# Find Python executable
# Enable alias expansion for detecting aliased python3
shopt -s expand_aliases
source ~/.bashrc 2>/dev/null || true

# Try multiple possible locations
if command -v python3 &> /dev/null; then
    PYTHON_CMD=$(command -v python3)
    # If it's an alias, extract the actual path
    if [[ $PYTHON_CMD == alias* ]]; then
        PYTHON_CMD=$(echo "$PYTHON_CMD" | sed "s/alias python3='\(.*\)'/\1/")
    fi
elif command -v python &> /dev/null; then
    PYTHON_CMD=$(command -v python)
elif [ -f /usr/local/bin/python3.12 ]; then
    PYTHON_CMD=/usr/local/bin/python3.12
elif [ -f /usr/local/bin/python3 ]; then
    PYTHON_CMD=/usr/local/bin/python3
elif [ -f /usr/bin/python3 ]; then
    PYTHON_CMD=/usr/bin/python3
else
    echo "Error: Python not found. Please ensure python3 is installed and in PATH."
    exit 1
fi

echo "Using Python: $PYTHON_CMD"
$PYTHON_CMD --version

if [ -d "$SCRIPT_DIR/dist" ]; then
    echo "Cleaning dist/ directory..."
    rm -rf "$SCRIPT_DIR/dist"
fi

if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning $BUILD_DIR directory..."
    rm -rf "$BUILD_DIR"
fi

# 3. Build the wheel
echo -e "\n--- Packaging avioflow into Wheel ---"

cd "$SCRIPT_DIR"

if [ "$USE_UV" = true ]; then
    # -C/--config-setting allows passing arguments to scikit-build-core
    # Pass PYTHON_EXECUTABLE to ensure the correct python version is used during build
    uv build --wheel "-Cbuild-dir=$BUILD_DIR" "-Ccmake.define.PYTHON_EXECUTABLE=$PYTHON_CMD" -v
else
    echo "uv not found, falling back to 'pip wheel'."
    $PYTHON_CMD -m pip wheel . --wheel-dir dist --no-deps "-Cbuild-dir=$BUILD_DIR" "-Ccmake.define.PYTHON_EXECUTABLE=$PYTHON_CMD"
fi

# 4. Repair the wheel with auditwheel
WHEEL_FILE=$(ls "$SCRIPT_DIR"/dist/*.whl 2>/dev/null | grep -v "manylinux" | head -n 1)

if [ -n "$WHEEL_FILE" ]; then
    echo -e "\n--- Repairing Wheel with auditwheel ---"
    if command -v auditwheel &> /dev/null; then
        # Repair the wheel and place it in dist/
        # auditwheel will automatically detect the appropriate manylinux tag
        auditwheel repair "$WHEEL_FILE" -w "$SCRIPT_DIR/dist/"
        
        # Remove the original non-compliant wheel
        rm "$WHEEL_FILE"
        echo "Wheel repaired and tagged as manylinux."
    else
        echo "Warning: 'auditwheel' not found. Skipping repair. The wheel may not be installable on all systems."
    fi
fi

# 5. Success message and location
WHEEL_FILE=$(ls "$SCRIPT_DIR"/dist/*manylinux*.whl 2>/dev/null | head -n 1)
if [ -z "$WHEEL_FILE" ]; then
    WHEEL_FILE=$(ls "$SCRIPT_DIR"/dist/*.whl 2>/dev/null | head -n 1)
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
