param(
    [string]$BuildDir = "build",
    [string]$KeyboardPath = "assets/default.keyboard",
    [string]$ScorePath = "assets/demo.in",
    [string]$ProbeKey = "Q",
    [ValidateSet("wasapi", "dsound", "log")]
    [string]$AudioBackend = "wasapi",
    [int]$SampleRate = 48000,
    [int]$BufferMs = 40
)

$ErrorActionPreference = "Stop"

$exePath = Join-Path $BuildDir "piano_cli.exe"
if (-not (Test-Path $exePath)) {
    throw "Executable not found: $exePath. Please run scripts/configure.ps1 and scripts/build.ps1 first."
}

& $exePath --keyboard $KeyboardPath --score $ScorePath --probe-key $ProbeKey --audio-backend $AudioBackend --sample-rate $SampleRate --buffer-ms $BufferMs
if ($LASTEXITCODE -ne 0) {
    throw "Demo run failed."
}
