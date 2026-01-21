$ErrorActionPreference = "Stop"

# 1. 配置路径
$BuildBinDir = "e:\Repos\demo-v1\avioflow\build\bin\Release"
$FFmpegBinDir = "E:\Repos\demo-v1\avioflow\build\_deps\ffmpeg_bin-src\bin"
$OutputDir = "e:\Repos\demo-v1\avioflow\build\stripped_test"
$EditBinPath = "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\editbin.exe"

if (Test-Path $OutputDir) { Remove-Item $OutputDir -Recurse -Force }
New-Item -Path $OutputDir -ItemType Directory | Out-Null

function Get-FileSize($path) {
    return (Get-Item $path).Length
}

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "   AVIOFLOW BINARY STRIP TEST (WINDOWS)   " -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# 2. 收集文件 (使用更可靠的文件匹配)
Write-Host "`n[1/3] Collecting files..." -ForegroundColor Yellow
$TargetFiles = @()
if (Test-Path $BuildBinDir) {
    $TargetFiles += Get-ChildItem -Path $BuildBinDir -File | Where-Object { $_.Extension -match "dll|exe" }
}
if (Test-Path $FFmpegBinDir) {
    $TargetFiles += Get-ChildItem -Path $FFmpegBinDir -File | Where-Object { $_.Name -like "av*.dll" }
}

if ($TargetFiles.Count -eq 0) {
    Write-Error "No files found to process!"
}

foreach ($File in $TargetFiles) {
    $Dest = Join-Path $OutputDir $File.Name
    Copy-Item $File.FullName $Dest -Force
    Write-Host "  + Ready: $($File.Name)"
}

# 3. 执行脱敏 (Stripping)
Write-Host "`n[2/3] Stripping debug symbols and optimizing..." -ForegroundColor Yellow
$Header = "{0,-35} | {1,14} | {2,14} | {3,10}" -f "Filename", "Before(KB)", "After(KB)", "Saved(KB)"
Write-Host $Header
Write-Host ("-" * $Header.Length)

$ProcessedFiles = Get-ChildItem -Path $OutputDir -File | Where-Object { $_.Extension -match "dll|exe" }
foreach ($File in $ProcessedFiles) {
    $OldSize = Get-FileSize $File.FullName
    
    # 执行剥离
    & $EditBinPath /STRIP:DEBUG $File.FullName | Out-Null
    
    $NewSize = Get-FileSize $File.FullName
    $Saved = $OldSize - $NewSize
    
    Write-Host ("{0,-35} | {1,14:N2} | {2,14:N2} | {3,10:N2}" -f $File.Name, ($OldSize/1KB), ($NewSize/1KB), ($Saved/1KB))
}

# 4. 隐私检查 (同时检查 ASCII 和 Unicode 路径)
Write-Host "`n[3/3] Privacy Scan (Checking for 'E:\Repos')..." -ForegroundColor Yellow
foreach ($File in $ProcessedFiles) {
    $Bytes = [System.IO.File]::ReadAllBytes($File.FullName)
    $TextAscii = [System.Text.Encoding]::ASCII.GetString($Bytes)
    $TextUnicode = [System.Text.Encoding]::Unicode.GetString($Bytes)
    
    if ($TextAscii -match "E:\\Repos" -or $TextUnicode -match "E:\\Repos") {
        Write-Host "  [!] WARNING: leakage found in $($File.Name)" -ForegroundColor Red
    } else {
        Write-Host "  [?] CLEAN: $($File.Name)" -ForegroundColor Green
    }
}

Write-Host "`nDONE. Files: $OutputDir" -ForegroundColor Cyan
