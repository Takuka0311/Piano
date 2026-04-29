param(
    [string]$BuildDir = "build-release",
    [string]$KeyboardPath = "assets/default.keyboard",
    [string]$ScorePath = "assets/demo.in",
    [string]$OutputPath = "dist/smoke-export.wav",
    [int]$SampleRate = 16000
)

$ErrorActionPreference = "Stop"

$exePath = Join-Path $BuildDir "piano_cli.exe"
if (-not (Test-Path $exePath)) {
    throw "Executable not found: $exePath"
}

$outDir = Split-Path -Parent $OutputPath
if (-not [string]::IsNullOrWhiteSpace($outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

& $exePath --keyboard $KeyboardPath --score $ScorePath --sample-rate $SampleRate --export-wav $OutputPath
if ($LASTEXITCODE -ne 0) {
    throw "WAV export failed."
}
if (-not (Test-Path $OutputPath)) {
    throw "WAV file not generated: $OutputPath"
}
Write-Host "WAV export smoke passed: $OutputPath"
