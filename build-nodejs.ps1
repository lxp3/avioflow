$ErrorActionPreference = "Stop"

# 获取目录
$RootDir = $PSScriptRoot
$NodejsDir = Join-Path $RootDir "nodejs"

Write-Host "=== Building Node.js Binding (from Root) ==="

# 1. 如果没有 node_modules，安装依赖
if (-not (Test-Path (Join-Path $NodejsDir "node_modules"))) {
    Write-Host "Installing Node.js dependencies..."
    Push-Location $NodejsDir
    try {
        if (Get-Command pnpm -ErrorAction SilentlyContinue) {
            pnpm install --no-frozen-lockfile
        } else {
            Write-Host "pnpm not found, falling back to npm..."
            npm install
        }
    } finally {
        Pop-Location
    }
}

# 2. 使用 cmake-js 编译
# cmake-js 需要在当前目录找到 package.json
$packageJsonPath = Join-Path $RootDir "package.json"
$cleanupPackageJson = $false

if (-not (Test-Path $packageJsonPath)) {
    Write-Host "Creating temporary package.json link..."
    # 使用 HardLink 以避免 SymbolicLink 可能需要的管理员权限问题
    New-Item -ItemType HardLink -Path $packageJsonPath -Value (Join-Path $NodejsDir "package.json")
    $cleanupPackageJson = $true
}

# 将本地 node_modules 的 bin 目录添加到 PATH
$env:Path = "$(Join-Path $NodejsDir "node_modules\.bin");$env:Path"

try {
    Write-Host "Running cmake-js compile..."
    # 执行编译命令
    & cmake-js compile --out build-nodejs `
        --CDENABLE_NODE_JS=ON `
        --CDENABLE_PYTHON=OFF `
        --CDENABLE_BINARY=OFF `
        --CDENABLE_WASAPI=OFF
} catch {
    Write-Error "Build failed: $_"
    exit 1
} finally {
    # 清理临时链接
    if ($cleanupPackageJson) {
        Remove-Item $packageJsonPath
    }
}

# 3. 测试阶段
Write-Host "`n=== Testing ABI Compatibility ==="
Push-Location $NodejsDir

try {
    # 查找编译产物
    $AvioflowBindingsPath = Join-Path $RootDir "build-nodejs\bin\avioflow.node"
    if (-not (Test-Path $AvioflowBindingsPath)) {
        # 兼容某些生成器输出到 Release 目录的情况
        $AvioflowBindingsPath = Join-Path $RootDir "build-nodejs\bin\Release\avioflow.node"
    }

    if (-not (Test-Path $AvioflowBindingsPath)) {
        Write-Error "Error: Build artifact not found at $AvioflowBindingsPath"
        exit 1
    }

    $env:AVIOFLOW_BINDINGS_PATH = $AvioflowBindingsPath

    # 运行测试脚本
    $nodeVersion = & node -v
    Write-Host -NoNewline "Testing $nodeVersion... "
    & node tests/test-offline-load.js
    if ($LASTEXITCODE -eq 0) {
        Write-Host "PASS" -ForegroundColor Green
    } else {
        Write-Host "FAIL" -ForegroundColor Red
        exit 1
    }
} finally {
    Pop-Location
}
