<#
.SYNOPSIS
    avioflow Build Script for Windows (PowerShell)
.DESCRIPTION
    Configures and builds the project using CMake on Windows.
#>

$ErrorActionPreference = "Stop"

# --- Configuration ---
$BuildSharedLibs = "OFF"
if ($args -contains "-Shared") { $BuildSharedLibs = "ON" }

if ($BuildSharedLibs -eq "ON") {
    $BuildDir = "build_shared"
} else {
    $BuildDir = "build_static"
}

# --- CMake Configuration ---
Write-Host "Configuring project ($BuildDir)..." -ForegroundColor Cyan

# Windows defaults to MSVC, Enable WASAPI for audio capture
cmake -B $BuildDir -S . `
    -DCMAKE_BUILD_TYPE=Release `
    -DENABLE_WASAPI=ON `
    -DENABLE_BINARY=ON `
    -DENABLE_PYTHON=ON `
    -DBUILD_SHARED_LIBS=$BuildSharedLibs `
    -DENABLE_NODE_JS=OFF

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# --- Build ---
Write-Host "`n--- Building avioflow ---" -ForegroundColor Cyan
cmake --build $BuildDir --config Release

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "`n--- Build Successful! ---" -ForegroundColor Green
