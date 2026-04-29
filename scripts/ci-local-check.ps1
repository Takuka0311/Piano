param(
    [string]$DebugDir = "build",
    [string]$ReleaseDir = "build-release"
)

$ErrorActionPreference = "Stop"

powershell -ExecutionPolicy Bypass -File scripts/configure.ps1 -BuildType Debug -BuildDir $DebugDir
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -BuildType Debug -BuildDir $DebugDir

if (Get-Command ctest -ErrorAction SilentlyContinue) {
    ctest --test-dir $DebugDir --output-on-failure
} else {
    $ctest = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
    if (-not (Test-Path $ctest)) {
        throw "ctest not found."
    }
    & $ctest --test-dir $DebugDir --output-on-failure
}
if ($LASTEXITCODE -ne 0) {
    throw "ctest failed."
}

powershell -ExecutionPolicy Bypass -File scripts/configure.ps1 -BuildType Release -BuildDir $ReleaseDir
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -BuildType Release -BuildDir $ReleaseDir
powershell -ExecutionPolicy Bypass -File scripts/run-demo.ps1 -BuildDir $ReleaseDir -AudioBackend log -KeyboardPath assets/default.keyboard -ScorePath assets/demo.in -ProbeKey Q
powershell -ExecutionPolicy Bypass -File scripts/package.ps1 -BuildType Release -BuildDir $ReleaseDir -OutputDir dist

Write-Host "Local CI check completed."
