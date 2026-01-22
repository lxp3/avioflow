
# avioflow Build Script
# This script configures and builds the project using CMake and MSVC

$ErrorActionPreference = "Stop"

# Configuration
$BUILD_DIR = "build"

# if (Test-Path $BUILD_DIR) {
#     Write-Host "Cleaning existing build directory..."
#     try {
#         Remove-Item -Path $BUILD_DIR -Recurse -Force -ErrorAction Stop
#     } catch {
#         Write-Warning "Could not fully clean build directory (some files might be in use). Continuing anyway..."
#     }
# }
# FFmpeg is now managed via CMake (FetchContent)


# Run CMake Configuration
# Removing -G Ninja allows CMake to use the default Visual Studio generator, 
# which automatically finds the compiler without needing VCVARS_PATH.
Write-Host "Configuring project..." -ForegroundColor Cyan
cmake -B $BUILD_DIR -S . -DENABLE_WASAPI=ON -DENABLE_PYTHON=ON

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
