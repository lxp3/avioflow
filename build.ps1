
# avioflow Build Script
# This script configures and builds the project using CMake and MSVC

$ErrorActionPreference = "Stop"

# Configuration
$VCVARS_PATH = "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
$BUILD_DIR = "build"

Write-Host "--- Configuring avioflow (VS 2026) ---" -ForegroundColor Cyan

if (Test-Path $BUILD_DIR) {
    Write-Host "Cleaning existing build directory..."
    Remove-Item -Path $BUILD_DIR -Recurse -Force
}

# Run CMake Configuration inside a CMD environment with vcvarsall.bat
# Using Ninja for faster builds and cleaner output (filters /showIncludes)
# CMAKE_EXPORT_COMPILE_COMMANDS generates compile_commands.json for IntelliSense
$CmakeConfigCmd = "cmake -B $BUILD_DIR -S . -G `"Ninja`" -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
cmd.exe /c "`"$VCVARS_PATH`" x64 && $CmakeConfigCmd"

if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed!"
}

Write-Host "`n--- Building avioflow ---" -ForegroundColor Cyan

# Build the project inside the same environment
$CmakeBuildCmd = "cmake --build $BUILD_DIR --config Release"
cmd.exe /c "`"$VCVARS_PATH`" x64 && $CmakeBuildCmd"

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed!"
}

Write-Host "`n--- Build Successful! ---" -ForegroundColor Green
Write-Host "Test executable location: $BUILD_DIR/avioflow_test.exe"
