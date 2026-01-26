
# avioflow Build Script
# This script configures and builds the project using CMake and MSVC

$ErrorActionPreference = "Stop"

# Configuration
$BUILD_SHARED_LIBS = "OFF"
if ($BUILD_SHARED_LIBS -eq "ON") {
    $BUILD_DIR = "build_shared"
} else {
    $BUILD_DIR = "build_static"
}

if (Test-Path $BUILD_DIR) {
    Write-Host "Cleaning existing build directory: $BUILD_DIR"
    try {
        Remove-Item -Path $BUILD_DIR -Recurse -Force -ErrorAction Stop
    }
    catch {
        Write-Warning "Could not fully clean build directory (some files might be in use). Continuing anyway..."
    }
}
# FFmpeg is now managed via CMake (FetchContent)


# Run CMake Configuration
# Removing -G Ninja allows CMake to use the default Visual Studio generator, 
# which automatically finds the compiler without needing VCVARS_PATH.
Write-Host "Configuring project ($BUILD_DIR)..." -ForegroundColor Cyan
cmake -G "Visual Studio 18 2026" -A x64 -B $BUILD_DIR -S . "-DENABLE_WASAPI=ON" "-DENABLE_PYTHON=ON" "-DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS" "-DENABLE_NODE_JS=ON"

if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed!"
}

Write-Host "`n--- Building avioflow ---" -ForegroundColor Cyan

# Build the project
cmake --build $BUILD_DIR --config Release

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed!"
}

Write-Host "`n--- Build Successful! ---" -ForegroundColor Green
