# prebuild-local.ps1
# Local prebuild script to generate prebuilds for Node.js and Electron

param(
    [switch]$SkipElectron = $false,
    [switch]$CreateDist = $true
)

$ErrorActionPreference = "Continue"
$ProjectDir = $PSScriptRoot
$PrebuildsDir = Join-Path $ProjectDir "prebuilds"
$DistDir = Join-Path $ProjectDir "dist"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Avioflow Local Prebuild Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Clean previous builds
Write-Host "[1/5] Cleaning previous builds..." -ForegroundColor Yellow
if (Test-Path $PrebuildsDir) {
    Remove-Item -Recurse -Force $PrebuildsDir
}
if (Test-Path $DistDir) {
    Remove-Item -Recurse -Force $DistDir
}
New-Item -ItemType Directory -Path $PrebuildsDir -Force | Out-Null

# Detect platform
$Platform = if ($IsWindows -or $env:OS -eq "Windows_NT") { "win32" } else { "linux" }
$Arch = "x64"
$PlatformDir = Join-Path $PrebuildsDir "$Platform-$Arch"
New-Item -ItemType Directory -Path $PlatformDir -Force | Out-Null

Write-Host "  Platform: $Platform-$Arch" -ForegroundColor Gray
Write-Host ""

# Install dependencies
Write-Host "[2/5] Installing npm dependencies..." -ForegroundColor Yellow
npm install --silent
if ($LASTEXITCODE -ne 0) {
    Write-Host "  ERROR: npm install failed" -ForegroundColor Red
    exit 1
}
Write-Host "  Done" -ForegroundColor Green
Write-Host ""

# Build for Node.js
Write-Host "[3/5] Building for Node.js..." -ForegroundColor Yellow
$BuildDir = Join-Path $ProjectDir "build"
if (Test-Path $BuildDir) {
    Remove-Item -Recurse -Force $BuildDir
}

npx cmake-js compile 2>&1 | Out-Host
if ($LASTEXITCODE -ne 0) {
    Write-Host "  ERROR: Node.js build failed" -ForegroundColor Red
    exit 1
}

# Find and copy the .node file
$NodeFile = $null
$PossiblePaths = @(
    "build/bin/Release/avioflow.node",
    "build/bin/avioflow.node"
)
foreach ($p in $PossiblePaths) {
    $FullPath = Join-Path $ProjectDir $p
    if (Test-Path $FullPath) {
        $NodeFile = $FullPath
        break
    }
}

if ($NodeFile) {
    Copy-Item $NodeFile (Join-Path $PlatformDir "avioflow.napi.node")
    Write-Host "  Node.js build: SUCCESS" -ForegroundColor Green
    Write-Host "  Output: $PlatformDir\avioflow.napi.node" -ForegroundColor Gray
} else {
    Write-Host "  ERROR: Could not find compiled .node file" -ForegroundColor Red
    exit 1
}
Write-Host ""

# Build for Electron (optional)
if (-not $SkipElectron) {
    Write-Host "[4/5] Building for Electron..." -ForegroundColor Yellow
    $ElectronVersions = @("34.0.0", "32.0.0", "30.0.0", "28.0.0")
    $ElectronSuccess = $false
    
    foreach ($ver in $ElectronVersions) {
        Write-Host "  Trying Electron $ver..." -ForegroundColor Gray
        
        # Clean build directory
        if (Test-Path $BuildDir) {
            Remove-Item -Recurse -Force $BuildDir
        }
        
        try {
            $output = npx cmake-js compile --runtime=electron --runtime-version=$ver 2>&1
            
            # Check if build succeeded
            $BuiltFile = $null
            foreach ($p in $PossiblePaths) {
                $FullPath = Join-Path $ProjectDir $p
                if (Test-Path $FullPath) {
                    $BuiltFile = $FullPath
                    break
                }
            }
            
            if ($BuiltFile) {
                Copy-Item $BuiltFile (Join-Path $PlatformDir "electron.napi.node")
                Write-Host "  Electron $ver build: SUCCESS" -ForegroundColor Green
                Write-Host "  Output: $PlatformDir\electron.napi.node" -ForegroundColor Gray
                $ElectronSuccess = $true
                break
            }
        } catch {
            Write-Host "    Failed: $_" -ForegroundColor DarkGray
        }
    }
    
    if (-not $ElectronSuccess) {
        Write-Host "  WARNING: Electron build failed, using Node.js build as fallback" -ForegroundColor DarkYellow
        Copy-Item (Join-Path $PlatformDir "avioflow.napi.node") (Join-Path $PlatformDir "electron.napi.node")
    }
} else {
    Write-Host "[4/5] Skipping Electron build (--SkipElectron)" -ForegroundColor DarkGray
    # Copy Node.js build as fallback
    Copy-Item (Join-Path $PlatformDir "avioflow.napi.node") (Join-Path $PlatformDir "electron.napi.node")
}
Write-Host ""

# Create dist package
if ($CreateDist) {
    Write-Host "[5/5] Creating distribution package..." -ForegroundColor Yellow
    
    New-Item -ItemType Directory -Path $DistDir -Force | Out-Null
    
    # Copy package files
    Copy-Item (Join-Path $ProjectDir "package.json") $DistDir
    Copy-Item (Join-Path $ProjectDir "README.md") $DistDir -ErrorAction SilentlyContinue
    Copy-Item (Join-Path $ProjectDir "LICENSE") $DistDir -ErrorAction SilentlyContinue
    
    # Copy avioflow/nodejs directory
    $NodejsSrc = Join-Path $ProjectDir "avioflow/nodejs"
    $NodejsDst = Join-Path $DistDir "avioflow/nodejs"
    New-Item -ItemType Directory -Path $NodejsDst -Force | Out-Null
    Copy-Item "$NodejsSrc/*" $NodejsDst -Recurse
    
    # Copy avioflow/include directory
    $IncludeSrc = Join-Path $ProjectDir "avioflow/include"
    $IncludeDst = Join-Path $DistDir "avioflow/include"
    if (Test-Path $IncludeSrc) {
        New-Item -ItemType Directory -Path $IncludeDst -Force | Out-Null
        Copy-Item "$IncludeSrc/*" $IncludeDst -Recurse
    }
    
    # Copy prebuilds
    Copy-Item $PrebuildsDir $DistDir -Recurse
    
    Write-Host "  Distribution package created at: $DistDir" -ForegroundColor Green
    Write-Host ""
    Write-Host "  Contents:" -ForegroundColor Gray
    Get-ChildItem -Path $DistDir -Recurse -File | ForEach-Object {
        $RelPath = $_.FullName.Substring($DistDir.Length + 1)
        $Size = "{0:N0} KB" -f ($_.Length / 1KB)
        Write-Host "    $RelPath ($Size)" -ForegroundColor Gray
    }
} else {
    Write-Host "[5/5] Skipping dist creation (--CreateDist:$false)" -ForegroundColor DarkGray
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  Prebuild Complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Prebuilds location: $PrebuildsDir" -ForegroundColor Cyan
if ($CreateDist) {
    Write-Host "Distribution package: $DistDir" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "To test locally:" -ForegroundColor Yellow
    Write-Host "  cd $DistDir" -ForegroundColor Gray
    Write-Host "  npm pack" -ForegroundColor Gray
    Write-Host "  # This creates avioflow-x.x.x.tgz" -ForegroundColor Gray
}
