$ErrorActionPreference = "Stop"

$RootDir = $PSScriptRoot
$VscodeExtDir = Join-Path $RootDir "vscode-extension"
$WasmArtifact = Join-Path $RootDir "wasm\dist\avioflow.wasm"

Write-Host "=== Building Avioflow VSCode Extension ==="

# Step 1: 确保 WASM 已编译
if (-not (Test-Path $WasmArtifact)) {
    Write-Host "WASM artifact not found. Running build-wasm.ps1..."
    & "$RootDir\build-wasm.ps1"
}

# Step 2: 编译与打包扩展
Write-Host "`nEntering vscode-extension directory..."
Push-Location $VscodeExtDir

try {
    # 检查 pnpm
    if (-not (Get-Command pnpm -ErrorAction SilentlyContinue)) {
        Write-Error "pnpm is required but not found."
        exit 1
    }

    Write-Host "Installing extension dependencies..."
    & pnpm install

    Write-Host "Compiling extension (Webview + TS)..."
    & pnpm run compile

    # Step 3: 手动拷贝 WASM 文件 (与 .sh 脚本逻辑对齐)
    Write-Host "Copying WASM files to extension/out/wasm..."
    $DestWasmDir = Join-Path $VscodeExtDir "out\wasm"
    if (-not (Test-Path $DestWasmDir)) {
        New-Item -ItemType Directory -Path $DestWasmDir | Out-Null
    }
    Copy-Item (Join-Path $RootDir "wasm\dist\avioflow.js") (Join-Path $DestWasmDir "avioflow.js") -Force
    Copy-Item (Join-Path $RootDir "wasm\dist\avioflow.wasm") (Join-Path $DestWasmDir "avioflow.wasm") -Force

    Write-Host "Packaging extension to VSIX..."
    # 使用 pnpm exec 调用本地安装的 vsce
    & pnpm exec vsce package

    Write-Host "`n✓ Extension built successfully!" -ForegroundColor Green
    Get-ChildItem "*.vsix" | Select-Object Name, @{Name="Size(MB)";Expression={"{0:N2}" -f ($_.Length / 1MB)}}
}
catch {
    Write-Error "Build failed: $_"
    exit 1
}
finally {
    Pop-Location
}
