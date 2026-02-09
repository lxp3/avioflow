$ErrorActionPreference = "Stop"

$RootDir = $PSScriptRoot
$EmsdkDir = Join-Path $RootDir "emsdk"

# 1. 检查/安装 Emscripten (emsdk)
if (-not (Test-Path $EmsdkDir)) {
    Write-Host "EMSDK not found. Cloning from GitHub..."
    git clone https://github.com/emscripten-core/emsdk.git $EmsdkDir
}

if (-not (Test-Path (Join-Path $EmsdkDir "upstream"))) {
    Write-Host "Installing and activating latest Emscripten (this may take a while)..."
    Push-Location $EmsdkDir
    try {
        .\emsdk.bat install latest
        .\emsdk.bat activate latest
    } finally {
        Pop-Location
    }
}

# 2. 初始化环境
Write-Host "Initializing Emscripten environment..."
# 调用 emsdk_env.ps1 以加载环境变量到当前会话
. (Join-Path $EmsdkDir "emsdk_env.ps1")

# 验证 emcmake 是否可用
if (-not (Get-Command emcmake -ErrorAction SilentlyContinue)) {
    Write-Error "emcmake not found. Please check if emsdk was installed correctly."
    exit 1
}

$BuildDir = Join-Path $RootDir "build-wasm"

# 3. 清理并创建构建目录
if (Test-Path $BuildDir) {
    Write-Host "Cleaning build directory..."
    Remove-Item -Recurse -Force $BuildDir
}
New-Item -ItemType Directory -Path $BuildDir | Out-Null

# 4. 配置项目 (Using emcmake)
Write-Host "Configuring with Emscripten..."
Push-Location $RootDir
try {
    & emcmake cmake -B build-wasm -S . `
        -DENABLE_WASM=ON `
        -DCMAKE_BUILD_TYPE=Release
} finally {
    Pop-Location
}

# 5. 编译
Write-Host "Building..."
$nproc = $env:NUMBER_OF_PROCESSORS
if ($null -eq $nproc) { $nproc = 2 }

cmake --build build-wasm --config Release --parallel $nproc

Write-Host "`nBuild complete! Output is in wasm/dist/"
if (Test-Path "wasm/dist") {
    Get-ChildItem "wasm/dist" | Select-Object Name, @{Name="Size(KB)";Expression={"{0:N2}" -f ($_.Length / 1KB)}}
}
