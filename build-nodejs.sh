#!/bin/bash
set -e

# Get directories
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
NODEJS_DIR="$ROOT_DIR/nodejs"

echo "=== Building Node.js Binding (from Root) ==="
cd "$ROOT_DIR"

# 1. Install dependencies if missing
if [ ! -d "nodejs/node_modules" ]; then
    echo "Installing Node.js dependencies..."
    if command -v pnpm &> /dev/null; then
        (cd nodejs && pnpm install)
    else
        (cd nodejs && npm install)
    fi
fi

# 2. Compile using cmake-js from Root
# cmake-js expects package.json in the current directory to read configuration
# We symlink it temporarily from nodejs/
if [ ! -f "package.json" ]; then
    echo "Creating temporary package.json symlink..."
    ln -s nodejs/package.json .
    CLEANUP_PACKAGE_JSON=true
fi

# Ensure we use the cmake-js installed in nodejs/node_modules
export PATH="$NODEJS_DIR/node_modules/.bin:$PATH"

cmake-js compile --out build-nodejs \
    --CDENABLE_NODE_JS=ON \
    --CDENABLE_PYTHON=OFF \
    --CDENABLE_BINARY=OFF \
    --CDENABLE_WASAPI=OFF

# Cleanup symlink
if [ "$CLEANUP_PACKAGE_JSON" = true ]; then
    rm package.json
fi

# 3. Test Phase
echo -e "\n=== Testing ABI Compatibility ==="
cd "$NODEJS_DIR"

# Point to the build artifact in root directory
# Actual output path found via find: build-nodejs/bin/avioflow.node
export AVIOFLOW_BINDINGS_PATH="$ROOT_DIR/build-nodejs/bin/avioflow.node"

if [ ! -f "$AVIOFLOW_BINDINGS_PATH" ]; then
    echo "Error: Build artifact not found at $AVIOFLOW_BINDINGS_PATH"
    exit 1
fi

run_test() {
    local cmd=$1
    local v=$($cmd -v)
    echo -n "Testing $v... "
    if $cmd tests/test-offline-load.js; then
        echo "PASS"
    else
        echo "FAIL"
        exit 1
    fi
}

# Test with available Node versions
if [ -s "$HOME/.nvm/nvm.sh" ]; then
    source "$HOME/.nvm/nvm.sh"
    for ver in $(nvm list --no-colors | grep -o 'v[0-9]*\.[0-9]*\.[0-9]*' | sort -uV); do
        major=${ver%%.*}; major=${major#v}
        if [ "$major" -ge 16 ]; then
            nvm use "$ver" >/dev/null 2>&1
            run_test "node"
        fi
    done
else
    run_test "node"
fi
