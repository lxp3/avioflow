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

echo -e "${CYAN}>>> Step 2.5: Copying native modules to out/node_modules...${NC}"
# Physical copy to dereference pnpm junctions
mkdir -p out/node_modules
# Use -L to follow symlinks (dereference) which is critical for pnpm
cp -L -r node_modules/avioflow out/node_modules/

# Using npx to ensure vsce is available without global installation
# Added --allow-missing-repository to bypass warnings
echo -e "${CYAN}>>> Step 3: Packaging extension...${NC}"

# Temporary disable vscode:prepublish in package.json to prevent vsce from running it
# This avoids double compilation and potential errors
sed -i.bak 's/"vscode:prepublish":/"vscode:prepublish-disabled":/' package.json

npx -y @vscode/vsce package --no-dependencies --allow-missing-repository -o avioflow-0.1.0.vsix

# Restore package.json
mv package.json.bak package.json

echo -e "\n${GREEN}[Success] Build complete! You can now install the generated .vsix file.${NC}"
echo -e "${GRAY}To install in VS Code: code --install-extension avioflow-0.1.0.vsix${NC}"
