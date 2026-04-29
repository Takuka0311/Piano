param(
    [string]$BuildDir = "build",
    [string]$KeyboardPath = "assets/default.keyboard",
    [string]$ScorePath = "assets/demo.in",
    [string]$ProbeKey = "Q"
)

$ErrorActionPreference = "Stop"

$exePath = Join-Path $BuildDir "piano_cli.exe"
if (-not (Test-Path $exePath)) {
    throw "Executable not found: $exePath. Please run scripts/configure.ps1 and scripts/build.ps1 first."
}

& $exePath --keyboard $KeyboardPath --score $ScorePath --probe-key $ProbeKey
