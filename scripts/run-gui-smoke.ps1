param(
    [string]$BuildDir = "build-release",
    [int]$ExitMs = 1200
)

$ErrorActionPreference = "Stop"

$exePath = Join-Path $BuildDir "piano_gui.exe"
if (-not (Test-Path $exePath)) {
    throw "GUI executable not found: $exePath"
}

& $exePath --smoke-exit-ms $ExitMs
if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) {
    throw "GUI smoke failed with exit code $LASTEXITCODE."
}
