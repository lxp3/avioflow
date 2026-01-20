
# avioflow Build Script
# This script configures and builds the project using CMake and MSVC

$ErrorActionPreference = "Stop"

# Configuration
$VCVARS_PATH = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
$BUILD_DIR = "build"

Write-Host "--- Configuring avioflow (VS 2026) ---" -ForegroundColor Cyan

if (Test-Path $BUILD_DIR) {
    Write-Host "Cleaning existing build directory..."
    Remove-Item -Path $BUILD_DIR -Recurse -Force
}

# --- FFmpeg Dependency Management ---
$FFMPEG_URL = "https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2025-11-30-12-53/ffmpeg-n7.1.3-7-gf65fc0b137-win64-lgpl-shared-7.1.zip"
$PUBLIC_DIR = "public"
$FFMPEG_ZIP = Join-Path $PUBLIC_DIR (Split-Path -Leaf $FFMPEG_URL)
$FFMPEG_DEST = "$PUBLIC_DIR/ffmpeg"
$DONE_FILE = "$PUBLIC_DIR/.ffmpeg.done"

if (!(Test-Path $PUBLIC_DIR)) {
    New-Item -ItemType Directory -Path $PUBLIC_DIR
}

if (!(Test-Path $FFMPEG_ZIP)) {
    Write-Host "Downloading FFmpeg from $FFMPEG_URL..." -ForegroundColor Cyan
    Invoke-WebRequest -Uri $FFMPEG_URL -OutFile $FFMPEG_ZIP -UseBasicParsing
}

if (!(Test-Path $DONE_FILE)) {
    Write-Host "Extracting FFmpeg to $FFMPEG_DEST..." -ForegroundColor Cyan
    if (Test-Path $FFMPEG_DEST) { Remove-Item -Path $FFMPEG_DEST -Recurse -Force }
    Expand-Archive -Path $FFMPEG_ZIP -DestinationPath "$FFMPEG_DEST-temp"
    
    # Move the inner directory to $FFMPEG_DEST
    $InnerDir = Get-ChildItem -Path "$FFMPEG_DEST-temp" -Directory | Select-Object -First 1
    Move-Item -Path $InnerDir.FullName -Destination $FFMPEG_DEST
    Remove-Item -Path "$FFMPEG_DEST-temp" -Recurse -Force
    
    New-Item -ItemType File -Path $DONE_FILE
    Write-Host "FFmpeg extraction complete." -ForegroundColor Green
}


# Run CMake Configuration inside a CMD environment with vcvarsall.bat
# Using Ninja generator (CMake 4.2.1 compatible)
$CmakeConfigCmd = "cmake -B $BUILD_DIR -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_WASAPI=ON"
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
