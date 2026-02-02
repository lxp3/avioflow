<#
.SYNOPSIS
    Build avioflow WebAssembly module using Emscripten

.DESCRIPTION
    This script builds the avioflow library as a WebAssembly module.
    Requires Emscripten SDK to be installed and activated.

.PARAMETER Clean
    Clean build directory before building

.EXAMPLE
    .\build.ps1
    
.EXAMPLE
    .\build.ps1 -Clean
#>

param(
    [switch]$Clean = $false
)

$ErrorActionPreference = "Stop"
$ProjectDir = Split-Path -Parent $PSScriptRoot  # Root project directory
$WasmDir = $PSScriptRoot
$BuildDir = Join-Path $WasmDir "build"
$DistDir = Join-Path $WasmDir "dist"

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  avioflow WebAssembly Build" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# Check for Emscripten
$emcmake = Get-Command "emcmake" -ErrorAction SilentlyContinue
if (-not $emcmake) {
    Write-Host "Error: Emscripten not found!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install and activate Emscripten SDK:"
    Write-Host "  1. git clone https://github.com/emscripten-core/emsdk.git"
    Write-Host "  2. cd emsdk && ./emsdk install latest && ./emsdk activate latest"
    Write-Host "  3. source ./emsdk_env.sh (or emsdk_env.bat on Windows)"
    Write-Host ""
    exit 1
}

Write-Host "Emscripten found: $($emcmake.Source)" -ForegroundColor Green
Write-Host ""

# Clean if requested
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "[1/3] Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

# Create directories
New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
New-Item -ItemType Directory -Path $DistDir -Force | Out-Null

# Configure with Emscripten
Write-Host "[2/3] Configuring with Emscripten..." -ForegroundColor Yellow
Push-Location $BuildDir
try {
    # Run emcmake to configure
    $configResult = cmd /c "emcmake cmake `"$ProjectDir`" -DENABLE_WASM=ON -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTS=OFF -DENABLE_BINARY=OFF 2>&1"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Configuration failed!" -ForegroundColor Red
        $configResult | ForEach-Object { Write-Host $_ }
        exit 1
    }
    Write-Host "  Configuration successful" -ForegroundColor Green
} finally {
    Pop-Location
}

# Build
Write-Host "[3/3] Building WASM module..." -ForegroundColor Yellow
Push-Location $BuildDir
try {
    $buildResult = cmd /c "cmake --build . --config Release -j 8 2>&1"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed!" -ForegroundColor Red
        $buildResult | ForEach-Object { Write-Host $_ }
        exit 1
    }
    Write-Host "  Build successful" -ForegroundColor Green
} finally {
    Pop-Location
}

# Check output
$wasmJs = Join-Path $DistDir "avioflow.js"
$wasmBin = Join-Path $DistDir "avioflow.wasm"

if ((Test-Path $wasmJs) -and (Test-Path $wasmBin)) {
    $jsSize = (Get-Item $wasmJs).Length / 1KB
    $wasmSize = (Get-Item $wasmBin).Length / 1MB
    
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Green
    Write-Host "  Build Successful!" -ForegroundColor Green
    Write-Host "============================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Output files:" -ForegroundColor White
    Write-Host "  wasm/dist/avioflow.js   ($([math]::Round($jsSize, 1)) KB)" -ForegroundColor Gray
    Write-Host "  wasm/dist/avioflow.wasm ($([math]::Round($wasmSize, 2)) MB)" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Usage in browser:" -ForegroundColor White
    Write-Host '  import createAvioflow from "./avioflow.js";' -ForegroundColor DarkGray
    Write-Host '  const avioflow = await createAvioflow();' -ForegroundColor DarkGray
    Write-Host '  const { metadata, samples } = avioflow.loadBuffer(arrayBuffer);' -ForegroundColor DarkGray
    Write-Host ""
} else {
    Write-Host "Error: Output files not found!" -ForegroundColor Red
    exit 1
}
