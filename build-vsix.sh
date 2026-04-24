#!/bin/bash
set -e

echo "Building Avioflow VSCode Extension..."

# Step 1: Build WASM (if not already built)
if [ ! -f "wasm/dist/avioflow.wasm" ]; then
    echo "Building WASM..."
    bash build-wasm.sh
fi

# Step 2: Copy WASM files to extension dist
echo "Copying WASM files to extension..."
mkdir -p vscode-extension/out/wasm
cp wasm/dist/avioflow.js vscode-extension/out/wasm/
cp wasm/dist/avioflow.wasm vscode-extension/out/wasm/
echo "✓ WASM files copied to vscode-extension/out/wasm/"

# Step 3: Build extension
echo "Building extension..."
cd vscode-extension
pnpm install --force
pnpm run compile

# Step 4: Package extension
echo "Packaging extension..."
pnpm exec vsce package

echo "✓ Extension built successfully!"
ls -lh *.vsix
