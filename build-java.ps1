param(
    [string]$Classifier = "",
    [string]$TargetArch = "x64"
)

$ErrorActionPreference = "Stop"
$RootDir = Split-Path -Parent $MyInvocation.MyCommand.Path

function Invoke-NativeCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,
        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$Arguments
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE: $Command $($Arguments -join ' ')"
    }
}

if (-not $Classifier) {
    if ($TargetArch -eq "arm64") {
        $Classifier = "windows-aarch64"
    } else {
        $Classifier = "windows-x86_64"
    }
}

$BuildDir = Join-Path $RootDir "build-java"
$Platform = if ($TargetArch -eq "arm64") { "ARM64" } else { "x64" }

Invoke-NativeCommand cmake -S $RootDir -B $BuildDir `
    -A $Platform `
    -DBUILD_SHARED_LIBS=OFF `
    -DENABLE_WASAPI=ON `
    -DENABLE_PYTHON=OFF `
    -DENABLE_NODE_JS=OFF `
    -DENABLE_JAVA=ON `
    -DENABLE_BINARY=OFF `
    -DBUILD_TESTS=OFF

Invoke-NativeCommand cmake --build $BuildDir --config Release -j 4

$NativeDir = Join-Path $RootDir "java/build/native/$Classifier"
New-Item -ItemType Directory -Force -Path $NativeDir | Out-Null

$Dll = Join-Path $BuildDir "bin/Release/avioflow_jni.dll"
if (-not (Test-Path $Dll)) {
    $Dll = Join-Path $BuildDir "bin/avioflow_jni.dll"
}
Copy-Item $Dll $NativeDir

$GradleCmd = if ($env:GRADLE_CMD) { $env:GRADLE_CMD } else { "gradle" }
$GradleTasks = if ($env:AVIOFLOW_SKIP_TESTS -eq "1") { @("nativeJar") } else { @("test", "nativeJar") }
Invoke-NativeCommand $GradleCmd -p "$RootDir/java" `
    "-Pavioflow.nativeClassifier=$Classifier" `
    "-Pavioflow.nativeLibraryDir=$NativeDir" `
    $GradleTasks
