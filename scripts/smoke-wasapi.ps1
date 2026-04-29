param(
    [string]$BuildDir = "build-release",
    [int]$DurationSeconds = 600,
    [string]$KeyboardPath = "assets/default.keyboard",
    [string]$ScorePath = "assets/demo.in"
)

$ErrorActionPreference = "Stop"

$exePath = Join-Path $BuildDir "piano_cli.exe"
if (-not (Test-Path $exePath)) {
    throw "Executable not found: $exePath. Run configure/build first."
}

$watch = [System.Diagnostics.Stopwatch]::StartNew()
$round = 0

while ($watch.Elapsed.TotalSeconds -lt $DurationSeconds) {
    $round++
    Write-Host ("[smoke-wasapi] round={0} elapsed={1:n1}s" -f $round, $watch.Elapsed.TotalSeconds)
    & $exePath --keyboard $KeyboardPath --score $ScorePath --audio-backend wasapi --sample-rate 48000 --buffer-ms 40
    if ($LASTEXITCODE -ne 0) {
        throw "WASAPI smoke failed at round $round with exit code $LASTEXITCODE."
    }
}

Write-Host ("[smoke-wasapi] passed duration={0}s rounds={1}" -f $DurationSeconds, $round)
