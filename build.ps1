<#
.SYNOPSIS
    avioflow Build Script for Windows (PowerShell)
.DESCRIPTION
    Configures and builds the project using CMake on Windows.
    Automatically builds Python wheels if ENABLE_PYTHON is ON.
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

# --- Python Wheel Packaging ---
$CacheFile = "$BuildDir/CMakeCache.txt"
$PythonEnabled = $false
if (Test-Path $CacheFile) {
    if (Select-String -Path $CacheFile -Pattern "ENABLE_PYTHON:BOOL=ON") {
        $PythonEnabled = $true
    }
}

if ($PythonEnabled) {
    Write-Host "`n--- Building Python Wheel ---" -ForegroundColor Yellow

    $PythonDir = "python"
    $DistDir = "python/dist"

    if (Get-Command "uv" -ErrorAction SilentlyContinue) {
        Write-Host "Found uv! Using 'uv build'..."
        Push-Location $PythonDir
        try {
            uv build --wheel --out-dir dist
        } finally {
            Pop-Location
        }
    } else {
        Write-Host "uv not found, using pip..."
        if (Get-Command "python" -ErrorAction SilentlyContinue) {
            python -m pip wheel ./python -w ./python/dist --no-deps
        } else {
            Write-Warning "Python not found, skipping wheel build."
        }
    }

    Write-Host "`n--- Wheel Build Check ---" -ForegroundColor Green
    if (Test-Path "$DistDir/*.whl") {
        Get-ChildItem "$DistDir/*.whl" | Select-Object Name, @{Name="Size(MB)";Expression={"{0:N2}" -f ($_.Length/1MB)}}
    } else {
        Write-Warning "No wheel file found in $DistDir"
    }
}

Write-Host "`n--- Build Successful! ---" -ForegroundColor Green
