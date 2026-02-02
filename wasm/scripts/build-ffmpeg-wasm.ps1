<#
.SYNOPSIS
    Build FFmpeg for WebAssembly using Emscripten (Windows PowerShell version)

.DESCRIPTION
    Downloads and compiles FFmpeg 7.1 to WebAssembly format.
    Requires Emscripten SDK to be installed and activated.

.EXAMPLE
    ./build-ffmpeg-wasm.ps1
#>

$ErrorActionPreference = "Stop"

# Configuration
$FFMPEG_VERSION = "7.1"
$FFMPEG_URL = "https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz"
$ScriptDir = $PSScriptRoot
$BuildDir = Join-Path $ScriptDir "ffmpeg-build"
$OutputDir = Join-Path $ScriptDir "..\ffmpeg-wasm"

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  FFmpeg $FFMPEG_VERSION WASM Build" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# Check Emscripten
$emcc = Get-Command "emcc" -ErrorAction SilentlyContinue
if (-not $emcc) {
    Write-Host "Error: Emscripten not found!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install and activate Emscripten SDK:"
    Write-Host "  git clone https://github.com/emscripten-core/emsdk.git"
    Write-Host "  cd emsdk && .\emsdk install latest && .\emsdk activate latest"
    Write-Host "  .\emsdk_env.bat"
    exit 1
}

$emccVersion = & emcc --version | Select-Object -First 1
Write-Host "Emscripten: $emccVersion" -ForegroundColor Green
Write-Host ""

# Note: FFmpeg configure script requires Unix-like environment
Write-Host "IMPORTANT: FFmpeg compilation requires a Unix-like shell." -ForegroundColor Yellow
Write-Host ""
Write-Host "On Windows, please use one of these options:" -ForegroundColor White
Write-Host "  1. WSL (Windows Subsystem for Linux) - Recommended" -ForegroundColor Gray
Write-Host "  2. MSYS2 / Git Bash" -ForegroundColor Gray
Write-Host "  3. Docker with Emscripten image" -ForegroundColor Gray
Write-Host ""
Write-Host "Run the shell script instead:" -ForegroundColor White
Write-Host "  wsl ./build-ffmpeg-wasm.sh" -ForegroundColor Cyan
Write-Host "  # or in Git Bash:" -ForegroundColor Gray
Write-Host "  bash ./build-ffmpeg-wasm.sh" -ForegroundColor Cyan
Write-Host ""

# Try to run via WSL if available
$wsl = Get-Command "wsl" -ErrorAction SilentlyContinue
if ($wsl) {
    Write-Host "WSL detected. Attempting to build via WSL..." -ForegroundColor Yellow
    Write-Host ""
    
    $shScript = Join-Path $ScriptDir "build-ffmpeg-wasm.sh"
    $shScriptWsl = $shScript -replace '\\', '/' -replace '^([A-Z]):', '/mnt/$1'.ToLower()
    
    Write-Host "Running: wsl bash $shScriptWsl" -ForegroundColor Gray
    wsl bash $shScriptWsl
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host ""
        Write-Host "FFmpeg WASM build completed successfully!" -ForegroundColor Green
    } else {
        Write-Host ""
        Write-Host "Build failed. Please check the error messages above." -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "WSL not found. Please install WSL or use Git Bash to run:" -ForegroundColor Red
    Write-Host "  bash ./build-ffmpeg-wasm.sh" -ForegroundColor Yellow
    exit 1
}
