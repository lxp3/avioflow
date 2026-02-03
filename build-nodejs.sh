#!/bin/bash

# ============================================================================
# .SYNOPSIS
#    Build and test avioflow Node.js addon across multiple Electron versions on Linux.
#
# .DESCRIPTION
#    This script demonstrates Node-API ABI stability:
#    - Builds the native module ONCE using Node-API version 8
#    - Tests the SAME .node file across multiple Electron versions
#    - Proves that Node-API modules don't need version-specific builds
# ============================================================================

set -e # Exit on error

# Default values
SKIP_BUILD=false
TEST_VERSIONS="28,30,32,34,37,38,39"
VERBOSE=false

# Parse arguments
while [[ "$#" -gt 0 ]]; do
    case $1 in
        --skip-build) SKIP_BUILD=true ;;
        --test-versions) TEST_VERSIONS="$2"; shift ;;
        --verbose) VERBOSE=true ;;
        *) echo "Unknown parameter passed: $1"; exit 1 ;;
    esac
    shift
done

PROJECT_DIR=$(pwd)
NODEJS_DIR="$PROJECT_DIR/nodejs"
PREBUILDS_DIR="$NODEJS_DIR/prebuilds"
PLATFORM="linux"
ARCH="x64"
PLATFORM_DIR="$PREBUILDS_DIR/$PLATFORM-$ARCH"
TEST_SCRIPT="$NODEJS_DIR/tests/test-offline-load.js"
NODE_API_VERSION=8

# Colors
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
RED='\033[0;31m'
GRAY='\033[0;90m'
NC='\033[0m' # No Color

function write_header() {
    echo -e "\n${CYAN}$(printf '=%.0s' {1..60})${NC}"
    echo -e "${CYAN}  $1${NC}"
    echo -e "${CYAN}$(printf '=%.0s' {1..60})${NC}\n"
}

function write_step() {
    echo -e "${YELLOW}[$1/$2] $3${NC}"
}

function write_success() {
    echo -e "  ${GREEN}✓ $1${NC}"
}

function write_fail() {
    echo -e "  ${RED}✗ $1${NC}"
}

function write_info() {
    echo -e "  ${GRAY}$1${NC}"
}

# ============================================================================
# Header
# ============================================================================
write_header "Avioflow Node-API Build & Test (Linux)"

echo -e "This script demonstrates Node-API (N-API) ABI stability:"
write_info "→ Build ONCE with Node-API version $NODE_API_VERSION"
write_info "→ Run on Node.js 16+ and ALL Electron versions"
write_info "→ NO version-specific builds required!"
echo ""
write_info "Platform: $PLATFORM-$ARCH"
write_info "Node.js:  $(node --version)"
echo ""

TOTAL_STEPS=4
if [ "$SKIP_BUILD" = true ]; then
    TOTAL_STEPS=3
fi
CURRENT_STEP=0

# ============================================================================
# Step 1: Install Dependencies
# ============================================================================
CURRENT_STEP=$((CURRENT_STEP + 1))
write_step $CURRENT_STEP $TOTAL_STEPS "Installing pnpm dependencies..."

cd "$NODEJS_DIR"
if pnpm install > /dev/null 2>&1; then
    write_success "Dependencies installed"
else
    write_fail "pnpm install failed"
    exit 1
fi
echo ""

# ============================================================================
# Step 2: Build Native Module (Node-API 8)
# ============================================================================
if [ "$SKIP_BUILD" = false ]; then
    CURRENT_STEP=$((CURRENT_STEP + 1))
    write_step $CURRENT_STEP $TOTAL_STEPS "Building Node-API $NODE_API_VERSION addon (universal build)..."
    
    BUILD_DIR="$NODEJS_DIR/build"
    rm -rf "$BUILD_DIR"
    
    ELECTRON_VERSION="37.0.0"
    write_info "Running: pnpm exec cmake-js compile --CDNAPI_VERSION=$NODE_API_VERSION --runtime electron --runtime-version $ELECTRON_VERSION --directory $PROJECT_DIR --out $BUILD_DIR"
    
    if [ "$VERBOSE" = true ]; then
        pnpm exec cmake-js compile --CDNAPI_VERSION=$NODE_API_VERSION --runtime electron --runtime-version $ELECTRON_VERSION --directory "$PROJECT_DIR" --out "$BUILD_DIR"
    else
        pnpm exec cmake-js compile --CDNAPI_VERSION=$NODE_API_VERSION --runtime electron --runtime-version $ELECTRON_VERSION --directory "$PROJECT_DIR" --out "$BUILD_DIR" > /dev/null 2>&1
    fi

    # Find and copy the .node file
    NODE_FILE=""
    POSSIBLE_PATHS=(
        "nodejs/build/bin/Release/avioflow.node"
        "nodejs/build/bin/avioflow.node"
    )
    
    for p in "${POSSIBLE_PATHS[@]}"; do
        FULL_PATH="$PROJECT_DIR/$p"
        if [ -f "$FULL_PATH" ]; then
            NODE_FILE="$FULL_PATH"
            break
        fi
    done
    
    if [ -z "$NODE_FILE" ]; then
        write_fail "Could not find compiled .node file"
        exit 1
    fi
    
    mkdir -p "$PLATFORM_DIR"
    cp "$NODE_FILE" "$PLATFORM_DIR/avioflow.napi.node"
    
    FILE_SIZE=$(du -m "$NODE_FILE" | cut -f1)
    write_success "Build successful!"
    write_info "Output: nodejs/prebuilds/$PLATFORM-$ARCH/avioflow.napi.node"
    write_info "Size: $FILE_SIZE MB"
    write_info "Node-API Version: $NODE_API_VERSION (ABI Stable)"
    echo ""
fi

# ============================================================================
# Step 3: Test with Node.js
# ============================================================================
CURRENT_STEP=$((CURRENT_STEP + 1))
write_step $CURRENT_STEP $TOTAL_STEPS "Testing with Node.js..."

write_info "Node.js version: $(node --version)"

if [ "$VERBOSE" = true ]; then
    node "$TEST_SCRIPT"
else
    node "$TEST_SCRIPT" > /dev/null 2>&1
fi

if [ $? -eq 0 ]; then
    write_success "Node.js test PASSED"
else
    write_fail "Node.js test FAILED"
    exit 1
fi
echo ""

# ============================================================================
# Step 4: Test with Multiple Electron Versions
# ============================================================================
CURRENT_STEP=$((CURRENT_STEP + 1))
write_step $CURRENT_STEP $TOTAL_STEPS "Testing with Electron versions..."

IFS=',' read -ra VERSIONS <<< "$TEST_VERSIONS"
declare -A TEST_RESULTS

echo ""
echo -e "  Testing the SAME avioflow.napi.node file across Electron versions:"
write_info "(This demonstrates Node-API ABI stability - no recompilation needed)"
echo ""

for ver in "${VERSIONS[@]}"; do
    ver=$(echo $ver | xargs) # trim
    FULL_VERSION="$ver.0.0"
    echo -n "  Electron $ver... "
    
    # export ELECTRON_MIRROR="https://npmmirror.com/mirrors/electron/"
    
    cd "$NODEJS_DIR"
    if ! pnpm add -D "electron@$FULL_VERSION" > /dev/null 2>&1; then
        echo -e "${YELLOW}SKIP (install failed)${NC}"
        TEST_RESULTS[$ver]="SKIP"
        continue
    fi
    
    # Run install.js if exists
    ELECTRON_INSTALL_SCRIPT=$(find node_modules/.pnpm -name "install.js" | grep "electron@$FULL_VERSION" | head -n 1)
    if [ -f "$ELECTRON_INSTALL_SCRIPT" ]; then
        node "$ELECTRON_INSTALL_SCRIPT" > /dev/null 2>&1
    fi
    
    # Create temp test script
    TEMP_TEST_FILE="$PROJECT_DIR/_electron_test_temp.cjs"
    cat > "$TEMP_TEST_FILE" <<EOF
const { app } = require('electron');
const path = require('path');
const fs = require('fs');

app.disableHardwareAcceleration();

app.whenReady().then(async () => {
    try {
        const prebuildPath = path.join(__dirname, 'nodejs', 'prebuilds', process.platform + '-' + process.arch, 'avioflow.napi.node');
        if (!fs.existsSync(prebuildPath)) {
            throw new Error('Prebuild not found: ' + prebuildPath);
        }
        
        const avioflow = require(prebuildPath);
        
        // Test 1: List audio devices
        avioflow.listAudioDevices();
        
        // Test 2: Load audio file
        const testFile = path.join(__dirname, 'public/wavs/TownTheme.mp3');
        if (fs.existsSync(testFile)) {
            avioflow.load(testFile, {
                outputSampleRate: 16000,
                outputNumChannels: 1
            });
        }
        
        app.exit(0);
    } catch (e) {
        console.error(e);
        app.exit(1);
    }
});
EOF

    ELECTRON_BIN="$NODEJS_DIR/node_modules/.bin/electron"
    
    # Run with timeout (30s)
    if timeout 30s "$ELECTRON_BIN" "$TEMP_TEST_FILE" --no-sandbox > /dev/null 2>&1; then
        echo -e "${GREEN}PASSED [OK]${NC}"
        TEST_RESULTS[$ver]="PASSED"
    else
        EXIT_CODE=$?
        if [ $EXIT_CODE -eq 124 ]; then
            echo -e "${YELLOW}TIMEOUT${NC}"
            TEST_RESULTS[$ver]="TIMEOUT"
        else
            echo -e "${RED}FAILED (exit code: $EXIT_CODE)${NC}"
            TEST_RESULTS[$ver]="FAILED"
        fi
    fi
    
    rm -f "$TEMP_TEST_FILE"
done

# ============================================================================
# Summary
# ============================================================================
write_header "Test Results Summary"

PASSED=0
FAILED=0
SKIPPED=0

for ver in "${VERSIONS[@]}"; do
    ver=$(echo $ver | xargs)
    res=${TEST_RESULTS[$ver]}
    case $res in
        PASSED) PASSED=$((PASSED + 1)); echo -e "  Electron $ver: ${GREEN}PASSED${NC}" ;;
        FAILED) FAILED=$((FAILED + 1)); echo -e "  Electron $ver: ${RED}FAILED${NC}" ;;
        *) SKIPPED=$((SKIPPED + 1)); echo -e "  Electron $ver: ${YELLOW}$res${NC}" ;;
    done
done

echo ""
if [ $FAILED -eq 0 ] && [ $PASSED -eq ${#VERSIONS[@]} ]; then
    echo -e "${GREEN}$(printf '=%.0s' {1..60})${NC}"
    echo -e "  ${GREEN}[OK] ALL TESTS PASSED!${NC}"
    echo ""
    echo -e "  This proves Node-API ABI stability:"
    write_info "-> One build (Node-API $NODE_API_VERSION) works across ALL versions"
    write_info "-> No need for Electron-specific builds"
    echo -e "${GREEN}$(printf '=%.0s' {1..60})${NC}"
    exit 0
else
    echo -e "${RED}$(printf '=%.0s' {1..60})${NC}"
    echo -e "  ${RED}[FAIL] SOME TESTS FAILED${NC}"
    echo ""
    write_info "Summary:"
    write_info "  Passed:  $PASSED / ${#VERSIONS[@]}"
    [ $FAILED -gt 0 ] && write_info "  Failed:  $FAILED"
    [ $SKIPPED -gt 0 ] && write_info "  Skipped: $SKIPPED"
    echo -e "${RED}$(printf '=%.0s' {1..60})${NC}"
    exit 1
fi
