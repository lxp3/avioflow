# Avioflow VS Code Extension Build Script
# This script compiles the extension and packages it into a .vsix file.

$ErrorActionPreference = "Stop"

Write-Host ">>> Step 1: Cleaning up previous builds..." -ForegroundColor Cyan
if (Test-Path "out") { Remove-Item -Recurse -Force "out" }
if (Test-Path "*.vsix") { Remove-Item -Force "*.vsix" }

Write-Host ">>> Step 2: Installing dependencies and compiling..." -ForegroundColor Cyan
# Run pnpm compile (which runs build:webview and tsc)
pnpm run compile

Write-Host ">>> Step 2.5: Copying native modules to out/node_modules..." -ForegroundColor Cyan
# Physical copy to dereference pnpm junctions
if (!(Test-Path "out/node_modules")) { New-Item -ItemType Directory -Path "out/node_modules" }
Copy-Item -Recurse -Force "node_modules/avioflow" "out/node_modules/"
# Using npx to ensure vsce is available without global installation
# Added --allow-missing-repository to bypass warnings
npx -y @vscode/vsce package --no-dependencies --allow-missing-repository -o avioflow-0.1.0.vsix

Write-Host "`n[Success] Build complete! You can now install the generated .vsix file." -ForegroundColor Green
Write-Host "To install in VS Code: code --install-extension <filename>.vsix" -ForegroundColor Gray
