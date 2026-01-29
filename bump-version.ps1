<#
.SYNOPSIS
    Bump version number across all project configuration files.

.DESCRIPTION
    Updates version in package.json, pyproject.toml, and optionally creates a git tag.

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

# --- Update package.json ---
$packageJsonPath = Join-Path $PSScriptRoot "package.json"
if (Test-Path $packageJsonPath) {
    $content = Get-Content $packageJsonPath -Raw
    $content = $content -replace '"version": ".*?"', "`"version`": `"$Version`""
    Set-Content $packageJsonPath $content -NoNewline
    Write-Host "  Updated package.json" -ForegroundColor Green
}

# --- Update package-lock.json ---
$packageLockPath = Join-Path $PSScriptRoot "package-lock.json"
if (Test-Path $packageLockPath) {
    $content = Get-Content $packageLockPath -Raw
    # Update root version
    $content = $content -replace '("name": "avioflow",\s*"version": )".*?"', "`$1`"$Version`""
    Set-Content $packageLockPath $content -NoNewline
    Write-Host "  Updated package-lock.json" -ForegroundColor Green
}

# --- Update pyproject.toml ---
$pyprojectPath = Join-Path $PSScriptRoot "pyproject.toml"
if (Test-Path $pyprojectPath) {
    $content = Get-Content $pyprojectPath -Raw
    $content = $content -replace 'version = ".*?"', "version = `"$Version`""
    Set-Content $pyprojectPath $content -NoNewline
    Write-Host "  Updated pyproject.toml" -ForegroundColor Green
}

Write-Host "`nVersion updated to $Version" -ForegroundColor Green

# --- Git tag (optional) ---
if ($Tag) {
    Write-Host "`nCreating git tag v$Version..." -ForegroundColor Cyan
    
    # Stage changes
    git add package.json package-lock.json pyproject.toml
    
    # Commit
    git commit -m "chore: bump version to $Version"
    
    # Create tag
    git tag "v$Version"
    
    Write-Host "Tag v$Version created." -ForegroundColor Green
    Write-Host ""
    Write-Host "To push to remote:" -ForegroundColor Yellow
    Write-Host "  git push origin main --tags" -ForegroundColor White
}
