# avioflow Wheel Build Script
# This script packages the project into a standard Python wheel (.whl) for local testing.

$ErrorActionPreference = "Stop"

# Get the script directory (python/) and project root (parent dir)
$ScriptDir = $PSScriptRoot
$ProjectDir = Split-Path $ScriptDir -Parent

# 1. Check for build tools
Write-Host "Checking for build tools..." -ForegroundColor Cyan
$UseUv = $false
if (Get-Command "uv" -ErrorAction SilentlyContinue) {
    Write-Host "Found uv! Using 'uv build' for a faster and more reliable process." -ForegroundColor Green
    $UseUv = $true
} elseif (!(Get-Command "python" -ErrorAction SilentlyContinue)) {
    Write-Error "Neither 'uv' nor 'python' found. Please install one of them."
}

# 2. Configuration & Clean
$BUILD_DIR = Join-Path $ProjectDir "build_py"
$DIST_DIR = Join-Path $ScriptDir "dist"

if (Test-Path $DIST_DIR) {
    Write-Host "Cleaning dist/ directory..."
    Remove-Item -Path $DIST_DIR -Recurse -Force
}

if (Test-Path $BUILD_DIR) {
    Write-Host "Cleaning $BUILD_DIR directory..."
    try {
        Remove-Item -Path $BUILD_DIR -Recurse -Force
    } catch {
        Write-Warning "Could not fully clean build directory. Continuing..."
    }
}

# 3. Build the wheel
# Yes, this will recompile the C++ project to create the Python extension.
# We set the build directory explicitly to avoid conflicts with your C++ build folder.
Write-Host "`n--- Packaging avioflow into Wheel ---" -ForegroundColor Cyan

Push-Location $ScriptDir
try {
    if ($UseUv) {
        # -C/--config-setting allows passing arguments to scikit-build-core
        uv build --wheel "-Cbuild-dir=$BUILD_DIR" -v
    } else {
        # Fallback to pip wheel if uv is not present
        Write-Warning "uv not found, falling back to 'python -m pip wheel'."
        python -m pip wheel . --wheel-dir dist --no-deps "-Cbuild-dir=$BUILD_DIR"
    }
} finally {
    Pop-Location
}

if ($LASTEXITCODE -ne 0) {
    Write-Error "Wheel build failed!"
}

# 4. Success message and location
$WheelFile = Get-ChildItem "$DIST_DIR\*.whl" | Select-Object -First 1
if ($WheelFile) {
    Write-Host "`n--- Wheel Build Successful! ---" -ForegroundColor Green
    Write-Host "Wheel location: $($WheelFile.FullName)" -ForegroundColor Green
    Write-Host "`nYou can now install it using:"
    Write-Host "python -m pip install $($WheelFile.FullName) --force-reinstall" -ForegroundColor Yellow
} else {
    Write-Error "Wheel file not found in dist/ directory."
}
