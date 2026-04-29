param(
    [string]$BuildDir = "build",
    [string]$KeyboardPath = "assets/default.keyboard",
    [string]$ScorePath = "assets/demo.in",
    [string]$ProbeKey = "Q",
    [ValidateSet("vsti", "midiout", "wasapi", "dsound", "log")]
    [string]$AudioBackend = "wasapi",
    [string]$MidiOutDevice = "",
    [string]$VstiPluginPath = "",
    [int]$SampleRate = 48000,
    [int]$BufferMs = 40
)

$ErrorActionPreference = "Stop"

$exePath = Join-Path $BuildDir "piano_cli.exe"
if (-not (Test-Path $exePath)) {
    throw "Executable not found: $exePath. Please run scripts/configure.ps1 and scripts/build.ps1 first."
}

& $exePath --keyboard $KeyboardPath --score $ScorePath --probe-key $ProbeKey --output-mode $AudioBackend --audio-backend $AudioBackend --midi-out-device $MidiOutDevice --vsti-plugin $VstiPluginPath --sample-rate $SampleRate --buffer-ms $BufferMs
if ($LASTEXITCODE -ne 0) {
    throw "Demo run failed."
}
