# avioflow Wheel Build Script
# This script packages the project into a standard Python wheel (.whl) for local testing.

$ErrorActionPreference = "Stop"
$ProjectDir = $PSScriptRoot
$PythonDir = Join-Path $ProjectDir "python"

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
$BUILD_DIR = "build_py"
$DIST_DIR = Join-Path $ProjectDir "dist"

if (Test-Path $DIST_DIR) {
    Write-Host "Cleaning dist/ directory..."
    Remove-Item -Path $DIST_DIR -Recurse -Force
}
$PythonBuildDir = Join-Path $PythonDir $BUILD_DIR
if (Test-Path $PythonBuildDir) {
    Write-Host "Cleaning $BUILD_DIR directory..."
    try {
        Remove-Item -Path $PythonBuildDir -Recurse -Force
    } catch {
        Write-Warning "Could not fully clean build directory. Continuing..."
    }
}

# 3. Build the wheel from python/ directory
# Yes, this will recompile the C++ project to create the Python extension.
# We set the build directory explicitly to avoid conflicts with your C++ build folder.
Write-Host "`n--- Packaging avioflow into Wheel ---" -ForegroundColor Cyan
Push-Location $PythonDir
try {
    if ($UseUv) {
        # -C/--config-setting allows passing arguments to scikit-build-core
        # Use double quotes to ensure PowerShell expands $BUILD_DIR
        uv build --wheel "-Cbuild-dir=$BUILD_DIR" -v --out-dir $DIST_DIR
    } else {
        # Fallback to pip wheel if uv is not present
        Write-Warning "uv not found, falling back to 'python -m pip wheel'."
        python -m pip wheel . --wheel-dir $DIST_DIR --no-deps "-Cbuild-dir=$BUILD_DIR"
    }
} finally {
    Pop-Location
}

if ($LASTEXITCODE -ne 0) {
    Write-Error "Wheel build failed!"
}

# 4. Success message and location
$WheelFile = Get-ChildItem "dist\*.whl" | Select-Object -First 1
Write-Host "`n--- Wheel Build Successful! ---" -ForegroundColor Green
Write-Host "Wheel location: $($WheelFile.FullName)" -ForegroundColor Green
Write-Host "`nYou can now install it using:"
Write-Host "python -m pip install $($WheelFile.FullName) --force-reinstall" -ForegroundColor Yellow
