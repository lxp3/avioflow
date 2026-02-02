<#
.SYNOPSIS
    Build and test avioflow Node.js addon across multiple Electron versions.

.DESCRIPTION
    This script demonstrates Node-API ABI stability:
    - Builds the native module ONCE using Node-API version 8
    - Tests the SAME .node file across multiple Electron versions
    - Proves that Node-API modules don't need version-specific builds

.PARAMETER SkipBuild
    Skip the build step and only run tests (assumes avioflow.napi.node exists)

.PARAMETER TestVersions
    Comma-separated list of Electron versions to test (default: 28,30,32,34,37,38,39)

.PARAMETER Verbose
    Enable verbose output

.EXAMPLE
    .\build-nodejs.ps1
    
.EXAMPLE
    .\build-nodejs.ps1 -SkipBuild -TestVersions "34,38,39"
#>

param(
    [switch]$SkipBuild = $false,
    [string]$TestVersions = "28,30,32,34,37,38,39",
    [switch]$VerboseOutput = $false
)

$ErrorActionPreference = "Stop"
$ProjectDir = $PSScriptRoot
$NodejsDir = Join-Path $ProjectDir "nodejs"
$PrebuildsDir = Join-Path $NodejsDir "prebuilds"
$Platform = if ($IsWindows -or $env:OS -eq "Windows_NT") { "win32" } else { "linux" }
$Arch = "x64"
$PlatformDir = Join-Path $PrebuildsDir "$Platform-$Arch"
$TestScript = Join-Path $NodejsDir "tests\test-offline-load.js"
$NodeAPIVersion = 8

# Colors and formatting
function Write-Header($text) {
    Write-Host ""
    Write-Host ("=" * 60) -ForegroundColor Cyan
    Write-Host "  $text" -ForegroundColor Cyan
    Write-Host ("=" * 60) -ForegroundColor Cyan
    Write-Host ""
}

function Write-Step($step, $total, $text) {
    Write-Host "[$step/$total] $text" -ForegroundColor Yellow
}

function Write-Success($text) {
    Write-Host "  ✓ $text" -ForegroundColor Green
}

function Write-Fail($text) {
    Write-Host "  ✗ $text" -ForegroundColor Red
}

function Write-Info($text) {
    Write-Host "  $text" -ForegroundColor Gray
}

# ============================================================================
# Header
# ============================================================================
Write-Header "Avioflow Node-API Build & Test"

Write-Host "This script demonstrates Node-API (N-API) ABI stability:" -ForegroundColor White
Write-Host "  → Build ONCE with Node-API version $NodeAPIVersion" -ForegroundColor Gray
Write-Host "  → Run on Node.js 16+ and ALL Electron versions" -ForegroundColor Gray
Write-Host "  → NO version-specific builds required!" -ForegroundColor Gray
Write-Host ""
Write-Host "Platform: $Platform-$Arch" -ForegroundColor Gray
Write-Host "Node.js:  $(node --version)" -ForegroundColor Gray
Write-Host ""

$TotalSteps = if ($SkipBuild) { 3 } else { 4 }
$CurrentStep = 0

# ============================================================================
# Step 1: Install Dependencies
# ============================================================================
$CurrentStep++
Write-Step $CurrentStep $TotalSteps "Installing pnpm dependencies..."

Push-Location $NodejsDir
try {
    $installResult = cmd /c "pnpm install 2>&1"
    if ($LASTEXITCODE -ne 0) {
        Write-Fail "pnpm install failed"
        exit 1
    }
    Write-Success "Dependencies installed"
} finally {
    Pop-Location
}
Write-Host ""

# ============================================================================
# Step 2: Build Native Module (Node-API 8)
# ============================================================================
if (-not $SkipBuild) {
    $CurrentStep++
    Write-Step $CurrentStep $TotalSteps "Building Node-API $NodeAPIVersion addon (universal build)..."
    
    # Clean previous build
    $BuildDir = Join-Path $NodejsDir "build"
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir 2>$null
    }
    
    # Build with Node-API 8 using Electron runtime
    # This ensures compatibility with VS Code's Electron environment
    # Note: cmake-js outputs info to stderr, which PowerShell treats as errors
    # We redirect stderr to stdout and check for the actual output file to determine success
    $ElectronVersion = "37.0.0"  # VS Code's current Electron version
    Write-Info "Running: pnpm exec cmake-js compile --CDNAPI_VERSION=$NodeAPIVersion --runtime electron --runtime-version $ElectronVersion --directory $ProjectDir --out $BuildDir"
    
    # Use cmd to avoid PowerShell stderr issues
    # cmake-js needs to run from nodejs dir (for node_modules) but point to root CMakeLists.txt
    Push-Location $NodejsDir
    try {
        $buildResult = cmd /c "pnpm exec cmake-js compile --CDNAPI_VERSION=$NodeAPIVersion --runtime electron --runtime-version $ElectronVersion --directory `"$ProjectDir`" --out `"$BuildDir`" 2>&1"
    } finally {
        Pop-Location
    }
    
    if ($VerboseOutput) { 
        $buildResult | ForEach-Object { Write-Host $_ }
    }
    
    # Find and copy the .node file
    $NodeFile = $null
    $PossiblePaths = @(
        "nodejs/build/bin/Release/avioflow.node",
        "nodejs/build/bin/avioflow.node"
    )
    foreach ($p in $PossiblePaths) {
        $FullPath = Join-Path $ProjectDir $p
        if (Test-Path $FullPath) {
            $NodeFile = $FullPath
            break
        }
    }
    
    if (-not $NodeFile) {
        Write-Fail "Could not find compiled .node file"
        Write-Info "Searched paths:"
        $PossiblePaths | ForEach-Object { Write-Info "  - $_" }
        exit 1
    }
    
    # Create prebuilds directory and copy
    New-Item -ItemType Directory -Path $PlatformDir -Force | Out-Null
    Copy-Item $NodeFile (Join-Path $PlatformDir "avioflow.napi.node") -Force
    
    $FileSize = (Get-Item $NodeFile).Length / 1MB
    Write-Success "Build successful!"
    Write-Info "Output: nodejs\prebuilds\$Platform-$Arch\avioflow.napi.node"
    Write-Info "Size: $([math]::Round($FileSize, 2)) MB"
    Write-Info "Node-API Version: $NodeAPIVersion (ABI Stable)"
    Write-Host ""
}

# ============================================================================
# Step 3: Test with Node.js
# ============================================================================
$CurrentStep++
Write-Step $CurrentStep $TotalSteps "Testing with Node.js..."

$NodeVersion = (node --version)
Write-Info "Node.js version: $NodeVersion"

# Run test using cmd to avoid PowerShell stderr issues (FFmpeg logs to stderr)
$testOutput = cmd /c "node `"$TestScript`" 2>&1"
$testExitCode = $LASTEXITCODE

if ($VerboseOutput) {
    $testOutput | ForEach-Object { Write-Info "  $_" }
}

if ($testExitCode -eq 0) {
    Write-Success "Node.js test PASSED"
}
else {
    Write-Fail "Node.js test FAILED (exit code: $testExitCode)"
    if (-not $VerboseOutput) {
        $testOutput | ForEach-Object { Write-Info "  $_" }
    }
    exit 1
}
Write-Host ""

# ============================================================================
# Step 4: Test with Multiple Electron Versions
# ============================================================================
$CurrentStep++
Write-Step $CurrentStep $TotalSteps "Testing with Electron versions..."

$ElectronVersions = $TestVersions.Split(",") | ForEach-Object { $_.Trim() }
$TestResults = @{}

Write-Host ""
Write-Host "  Testing the SAME avioflow.napi.node file across Electron versions:" -ForegroundColor White
Write-Host "  (This demonstrates Node-API ABI stability - no recompilation needed)" -ForegroundColor DarkGray
Write-Host ""

foreach ($ver in $ElectronVersions) {
    $fullVersion = "$ver.0.0"
    Write-Host "  Electron $ver... " -NoNewline
    
    # Set Electron mirror for faster downloads (especially useful in China)
    $env:ELECTRON_MIRROR = "https://npmmirror.com/mirrors/electron/"
    
    # Install specific Electron version (saved to devDependencies, kept locally)
    Push-Location $NodejsDir
    $installOutput = cmd /c "pnpm add -D electron@$fullVersion 2>&1"
    Pop-Location
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "SKIP (install failed)" -ForegroundColor DarkYellow
        $TestResults[$ver] = "SKIP"
        continue
    }
    
    # Manually run install.js to ensure binary is downloaded (pnpm may skip postinstall)
    $electronInstallScript = Join-Path $NodejsDir "node_modules\.pnpm\electron@$fullVersion\node_modules\electron\install.js"
    if (Test-Path $electronInstallScript) {
        $null = cmd /c "node `"$electronInstallScript`" 2>&1"
    }
    
    # Create a simple test script that runs our test in Electron's Node.js context
    # Note: We load the .node file directly to avoid ESM/CommonJS issues
    $ElectronTestScript = @"
const { app } = require('electron');
const path = require('path');
const fs = require('fs');

// Disable GPU to avoid issues in CI/headless environments
app.disableHardwareAcceleration();

app.whenReady().then(async () => {
    console.log('=== Electron Native Module Test ===');
    console.log('Electron version:', process.versions.electron);
    console.log('Node version:', process.versions.node);
    console.log('Platform:', process.platform, process.arch);
    console.log('');
    
    try {
        // Load the native module directly (bypasses ESM loader issues)
        const prebuildPath = path.join(__dirname, 'nodejs', 'prebuilds', process.platform + '-' + process.arch, 'avioflow.napi.node');
        console.log('Loading native module from:', prebuildPath);
        
        if (!fs.existsSync(prebuildPath)) {
            throw new Error('Prebuild not found: ' + prebuildPath);
        }
        
        const avioflow = require(prebuildPath);
        console.log('Native module loaded successfully!');
        console.log('');
        
        // Test 1: List audio devices
        console.log('[Test 1] listAudioDevices()...');
        const devices = avioflow.listAudioDevices();
        console.log('  Result:', devices.length, 'devices found');
        
        // Test 2: Load audio file
        const testFile = path.join(__dirname, 'public/wavs/TownTheme.mp3');
        if (fs.existsSync(testFile)) {
            console.log('[Test 2] load() with audio file...');
            const result = avioflow.load(testFile, {
                outputSampleRate: 16000,
                outputNumChannels: 1
            });
            console.log('  Metadata:', JSON.stringify({
                duration: result.metadata.duration,
                sampleRate: result.metadata.sampleRate,
                channels: result.metadata.numChannels
            }));
            console.log('  Samples:', result.samples.length, 'channel(s),', result.samples[0].length, 'samples');
        } else {
            console.log('[Test 2] SKIP - test file not found');
        }
        
        console.log('');
        console.log('=== TEST PASSED ===');
        app.exit(0);
    } catch (e) {
        console.error('');
        console.error('=== TEST FAILED ===');
        console.error('Error:', e.message);
        if (e.stack) console.error(e.stack);
        app.exit(1);
    }
});

app.on('window-all-closed', () => {});
"@
    
    $TempTestFile = Join-Path $ProjectDir "_electron_test_temp.cjs"
    $ElectronTestScript | Out-File -FilePath $TempTestFile -Encoding UTF8 -Force
    
    try {
        # Run Electron with our test script
        $electronPath = Join-Path $NodejsDir "node_modules\.bin\electron.cmd"
        if (-not (Test-Path $electronPath)) {
            $electronPath = Join-Path $NodejsDir "node_modules\.bin\electron"
        }
        
        $pinfo = New-Object System.Diagnostics.ProcessStartInfo
        $pinfo.FileName = $electronPath
        $pinfo.Arguments = $TempTestFile
        $pinfo.RedirectStandardOutput = $true
        $pinfo.RedirectStandardError = $true
        $pinfo.UseShellExecute = $false
        $pinfo.CreateNoWindow = $true
        $pinfo.WorkingDirectory = $ProjectDir
        
        $process = New-Object System.Diagnostics.Process
        $process.StartInfo = $pinfo
        $process.Start() | Out-Null
        
        # Wait with timeout
        $completed = $process.WaitForExit(30000)  # 30 second timeout
        
        if (-not $completed) {
            $process.Kill()
            Write-Host "TIMEOUT" -ForegroundColor DarkYellow
            $TestResults[$ver] = "TIMEOUT"
        }
        elseif ($process.ExitCode -eq 0) {
            Write-Host "PASSED [OK]" -ForegroundColor Green
            $TestResults[$ver] = "PASSED"
            
            if ($VerboseOutput) {
                $stdout = $process.StandardOutput.ReadToEnd()
                $stdout.Split("`n") | ForEach-Object { Write-Info "    $_" }
            }
        }
        elseif ($process.ExitCode -lt 0) {
            # Negative exit code indicates a crash (signal termination)
            Write-Host "CRASHED (exit code: $($process.ExitCode))" -ForegroundColor Red
            $TestResults[$ver] = "CRASHED"
            
            $stderr = $process.StandardError.ReadToEnd()
            $stdout = $process.StandardOutput.ReadToEnd()
            if ($stdout) { 
                Write-Info "    STDOUT:"
                $stdout.Split("`n") | ForEach-Object { Write-Info "      $_" } 
            }
            if ($stderr) { 
                Write-Info "    STDERR:"
                $stderr.Split("`n") | ForEach-Object { Write-Info "      $_" } 
            }
        }
        else {
            Write-Host "FAILED (exit code: $($process.ExitCode))" -ForegroundColor Red
            $TestResults[$ver] = "FAILED"
            
            $stderr = $process.StandardError.ReadToEnd()
            $stdout = $process.StandardOutput.ReadToEnd()
            if ($stderr) { $stderr.Split("`n") | ForEach-Object { Write-Info "    $_" } }
            if ($stdout) { $stdout.Split("`n") | ForEach-Object { Write-Info "    $_" } }
        }
    }
    catch {
        Write-Host "ERROR" -ForegroundColor Red
        $TestResults[$ver] = "ERROR"
        Write-Info "    $_"
    }
    finally {
        # Clean up temp file
        if (Test-Path $TempTestFile) {
            Remove-Item $TempTestFile -Force 2>$null
        }
    }
}

Write-Host ""

# ============================================================================
# Summary
# ============================================================================
Write-Header "Test Results Summary"

$Passed = ($TestResults.Values | Where-Object { $_ -eq "PASSED" }).Count
$Failed = ($TestResults.Values | Where-Object { $_ -eq "FAILED" }).Count
$Crashed = ($TestResults.Values | Where-Object { $_ -eq "CRASHED" }).Count
$Skipped = ($TestResults.Values | Where-Object { $_ -eq "SKIP" -or $_ -eq "TIMEOUT" -or $_ -eq "ERROR" }).Count
# Count empty/null results as unknown (likely crashed without proper capture)
$Unknown = ($ElectronVersions | Where-Object { -not $TestResults[$_] -or $TestResults[$_] -eq "" }).Count
$TotalTests = $ElectronVersions.Count
$TotalProblems = $Failed + $Crashed + $Unknown

Write-Host "Node.js Test: PASSED" -ForegroundColor Green
Write-Host ""
Write-Host "Electron Tests:" -ForegroundColor White

foreach ($ver in $ElectronVersions) {
    $result = $TestResults[$ver]
    if (-not $result -or $result -eq "") {
        $result = "UNKNOWN (no result captured)"
        $color = "Red"
    }
    else {
        $color = switch ($result) {
            "PASSED" { "Green" }
            "FAILED" { "Red" }
            "CRASHED" { "Red" }
            default { "DarkYellow" }
        }
    }
    Write-Host "  Electron $ver`: $result" -ForegroundColor $color
}

Write-Host ""
$SummaryColor = if ($TotalProblems -eq 0) { "Green" } else { "Red" }
Write-Host ("=" * 60) -ForegroundColor $SummaryColor

if ($TotalProblems -eq 0 -and $Passed -eq $TotalTests) {
    Write-Host "  [OK] ALL TESTS PASSED!" -ForegroundColor Green
    Write-Host ""
    Write-Host "  This proves Node-API ABI stability:" -ForegroundColor White
    Write-Host "  -> One build (Node-API $NodeAPIVersion) works across ALL versions" -ForegroundColor Gray
    Write-Host "  -> No need for Electron-specific builds" -ForegroundColor Gray
}
else {
    Write-Host "  [FAIL] TESTS FAILED OR CRASHED" -ForegroundColor Red
    Write-Host ""
    Write-Host "  Summary:" -ForegroundColor White
    Write-Host "    Passed:  $Passed / $TotalTests" -ForegroundColor $(if ($Passed -eq $TotalTests) { "Green" } else { "Gray" })
    if ($Failed -gt 0) { Write-Host "    Failed:  $Failed" -ForegroundColor Red }
    if ($Crashed -gt 0) { Write-Host "    Crashed: $Crashed" -ForegroundColor Red }
    if ($Unknown -gt 0) { Write-Host "    Unknown: $Unknown (likely crashed)" -ForegroundColor Red }
    if ($Skipped -gt 0) { Write-Host "    Skipped: $Skipped" -ForegroundColor DarkYellow }
}

Write-Host ("=" * 60) -ForegroundColor $SummaryColor
Write-Host ""

exit $(if ($TotalProblems -eq 0 -and $Passed -eq $TotalTests) { 0 } else { 1 })
