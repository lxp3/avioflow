# change-version.ps1
# Usage: ./change-version.ps1 -NewVersion 0.2.2

param (
    [Parameter(Mandatory=$true)]
    [string]$NewVersion
)

$ErrorActionPreference = "Stop"
$RootDir = $PSScriptRoot

Write-Host "=== Bumping version to $NewVersion ===" -ForegroundColor Cyan

# 1. Update CMakeLists.txt (Project version)
$CMakeFile = Join-Path $RootDir "CMakeLists.txt"
if (Test-Path $CMakeFile) {
    Write-Host "Updating $CMakeFile..."
    (Get-Content $CMakeFile) -replace 'project\(avioflow VERSION \d+\.\d+\.\d+\)', "project(avioflow VERSION $NewVersion)" | Set-Content $CMakeFile
}

# 2. Update Python pyproject.toml
$PyProjectFile = Join-Path $RootDir "python/pyproject.toml"
if (Test-Path $PyProjectFile) {
    Write-Host "Updating $PyProjectFile..."
    (Get-Content $PyProjectFile) -replace 'version = "\d+\.\d+\.\d+"', "version = `"$NewVersion`"" | Set-Content $PyProjectFile
}

# 3. Update Node.js package.json files
$PackageJsonFiles = @(
    "nodejs/package.json",
    "nodejs/npm-packages/@lxp3/linux-x64/package.json",
    "nodejs/npm-packages/@lxp3/win32-x64/package.json",
    "wasm/package.json",
    "vscode-extension/package.json"
)

foreach ($RelPath in $PackageJsonFiles) {
    $FilePath = Join-Path $RootDir $RelPath
    if (Test-Path $FilePath) {
        Write-Host "Updating $FilePath..."
        # Use regex to replace version field, handling potential trailing commas
        (Get-Content $FilePath) -replace '"version": "\d+\.\d+\.\d+"', "`"version`": `"$NewVersion`"" | Set-Content $FilePath
    }
}

# 4. Update VS Code Extension README (VSIX references)
$ReadmeFile = Join-Path $RootDir "vscode-extension/README.md"
if (Test-Path $ReadmeFile) {
    Write-Host "Updating $ReadmeFile..."
    (Get-Content $ReadmeFile) -replace 'avioflow-\d+\.\d+\.\d+\.vsix', "avioflow-$NewVersion.vsix" | Set-Content $ReadmeFile
}

# 5. Update Lockfiles (pnpm)
Write-Host "`n=== Updating Lockfiles ===" -ForegroundColor Cyan

# Update nodejs/pnpm-lock.yaml
if (Test-Path (Join-Path $RootDir "nodejs")) {
    Write-Host "Updating nodejs lockfile..."
    Push-Location (Join-Path $RootDir "nodejs")
    try {
        pnpm install --no-frozen-lockfile
    } finally {
        Pop-Location
    }
}

# Update vscode-extension/pnpm-lock.yaml
if (Test-Path (Join-Path $RootDir "vscode-extension")) {
    Write-Host "Updating vscode-extension lockfile..."
    Push-Location (Join-Path $RootDir "vscode-extension")
    try {
        pnpm install --no-frozen-lockfile
    } finally {
        Pop-Location
    }
}

Write-Host "`n✅ Successfully updated version to $NewVersion" -ForegroundColor Green
Write-Host "Files modified:"
git status -s
