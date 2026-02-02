<#
.SYNOPSIS
    Bump version number across all project configuration files.

.DESCRIPTION
    Updates version in package.json (main and platform packages), pyproject.toml, 
    and optionally creates a git tag.

.PARAMETER Version
    The new version number (e.g., "0.1.7")

.PARAMETER Tag
    If specified, creates a git tag with the version and pushes to origin.

.EXAMPLE
    .\bump-version.ps1 -Version "0.1.7"
    
.EXAMPLE
    .\bump-version.ps1 -Version "0.1.7" -Tag
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$Version,
    
    [switch]$Tag
)

$ErrorActionPreference = "Stop"

Write-Host "Bumping version to $Version..." -ForegroundColor Cyan

# Helper function to update version in a JSON file
function Update-JsonVersion {
    param(
        [string]$Path,
        [string]$Version
    )
    if (Test-Path $Path) {
        $content = Get-Content $Path -Raw
        $content = $content -replace '"version": ".*?"', "`"version`": `"$Version`""
        Set-Content $Path $content -NoNewline
        Write-Host "  Updated $Path" -ForegroundColor Green
        return $true
    }
    return $false
}

# --- Update main Node.js package.json ---
$nodejsPackageJson = Join-Path $PSScriptRoot "nodejs/package.json"
if (Update-JsonVersion -Path $nodejsPackageJson -Version $Version) {
    # Also update optionalDependencies versions
    $content = Get-Content $nodejsPackageJson -Raw
    $content = $content -replace '"@avioflow/win32-x64": ".*?"', "`"@avioflow/win32-x64`": `"$Version`""
    $content = $content -replace '"@avioflow/linux-x64": ".*?"', "`"@avioflow/linux-x64`": `"$Version`""
    Set-Content $nodejsPackageJson $content -NoNewline
    Write-Host "  Updated optionalDependencies versions" -ForegroundColor Green
}

# --- Update platform-specific packages ---
$platformPackages = @(
    "nodejs/npm-packages/@avioflow/win32-x64/package.json",
    "nodejs/npm-packages/@avioflow/linux-x64/package.json"
)

foreach ($pkgPath in $platformPackages) {
    $fullPath = Join-Path $PSScriptRoot $pkgPath
    Update-JsonVersion -Path $fullPath -Version $Version | Out-Null
}

# --- Update Python pyproject.toml ---
$pyprojectPath = Join-Path $PSScriptRoot "python/pyproject.toml"
if (Test-Path $pyprojectPath) {
    $content = Get-Content $pyprojectPath -Raw
    $content = $content -replace 'version = ".*?"', "version = `"$Version`""
    Set-Content $pyprojectPath $content -NoNewline
    Write-Host "  Updated python/pyproject.toml" -ForegroundColor Green
}

# --- Update VS Code extension package.json (if needed) ---
$vscodePackageJson = Join-Path $PSScriptRoot "vscode-extension/package.json"
if (Test-Path $vscodePackageJson) {
    Update-JsonVersion -Path $vscodePackageJson -Version $Version | Out-Null
}

Write-Host "`nVersion updated to $Version" -ForegroundColor Green

# --- Git tag (optional) ---
if ($Tag) {
    Write-Host "`nCreating git tag v$Version..." -ForegroundColor Cyan
    
    # Stage changes
    $filesToStage = @(
        "nodejs/package.json",
        "nodejs/npm-packages/@avioflow/win32-x64/package.json",
        "nodejs/npm-packages/@avioflow/linux-x64/package.json",
        "python/pyproject.toml",
        "vscode-extension/package.json"
    )
    
    foreach ($file in $filesToStage) {
        $fullPath = Join-Path $PSScriptRoot $file
        if (Test-Path $fullPath) {
            git add $fullPath
        }
    }
    
    # Commit
    git commit -m "chore: bump version to $Version"
    
    # Create tag
    git tag "v$Version"
    
    Write-Host "Tag v$Version created." -ForegroundColor Green
    Write-Host ""
    Write-Host "To push to remote:" -ForegroundColor Yellow
    Write-Host "  git push origin main --tags" -ForegroundColor White
}
