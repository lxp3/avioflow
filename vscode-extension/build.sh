#!/bin/bash

# Avioflow VS Code Extension Build Script for Linux
# This script compiles the extension and packages it into a .vsix file.

set -e

# Colors for output
CYAN='\033[0;36m'
GREEN='\033[0;32m'
GRAY='\033[0;90m'
NC='\033[0m' # No Color

echo -e "${CYAN}>>> Step 1: Cleaning up previous builds...${NC}"
rm -rf out
rm -f *.vsix

echo -e "${CYAN}>>> Step 2: Installing dependencies and compiling...${NC}"
# Run pnpm compile (which runs build:webview and tsc)
pnpm run compile

echo -e "${CYAN}>>> Step 2.4: Copying .mjs worker file...${NC}"
# Copy the ESM worker file (not processed by TypeScript)
cp src/avioflowWorker.mjs out/src/

echo -e "${CYAN}>>> Step 2.5: Copying native modules from root...${NC}"
# Physical copy to dereference pnpm junctions
mkdir -p out/node_modules/avioflow
# Copy from the root project directory to ensure we use the latest local build
# Fix: Ensure the directory structure matches what Node.js expect (avioflow/nodejs/index.js)
mkdir -p out/node_modules/avioflow/avioflow/nodejs
cp ../avioflow/nodejs/index.js out/node_modules/avioflow/avioflow/nodejs/
cp ../avioflow/nodejs/index.d.ts out/node_modules/avioflow/avioflow/nodejs/
cp ../package.json out/node_modules/avioflow/
cp -r ../prebuilds out/node_modules/avioflow/

echo -e "${CYAN}>>> Step 2.6: Cleaning up non-Linux binaries...${NC}"
# Remove Windows and macOS prebuilds to reduce package size and ensure only Linux binaries are included
rm -rf out/node_modules/avioflow/prebuilds/win32*
rm -rf out/node_modules/avioflow/prebuilds/darwin*
# Verify linux-x64 exists
if [ ! -d "out/node_modules/avioflow/prebuilds/linux-x64" ]; then
    echo -e "\n${CYAN}WARNING: linux-x64 prebuild not found!${NC}"
    ls -la out/node_modules/avioflow/prebuilds/ || echo "prebuilds directory not found"
fi

# Using npx to ensure vsce is available without global installation
# Added --allow-missing-repository to bypass warnings
echo -e "${CYAN}>>> Step 3: Packaging extension...${NC}"

# Temporary disable vscode:prepublish in package.json to prevent vsce from running it
# This avoids double compilation and potential errors
sed -i.bak 's/"vscode:prepublish":/"vscode:prepublish-disabled":/' package.json

npx -y @vscode/vsce package --no-dependencies --allow-missing-repository -o avioflow-0.1.13.vsix

# Restore package.json
mv package.json.bak package.json

echo -e "\n${GREEN}[Success] Build complete! You can now install the generated .vsix file.${NC}"
echo -e "${GRAY}To install in VS Code: code --install-extension avioflow-0.1.13.vsix${NC}"
