param(
    [string]$BuildDir = "build-release",
    [string]$KeyboardPath = "assets/default.keyboard",
    [string]$ScorePath = "assets/demo.in",
    [string]$MidiOutDevice = ""
)

$ErrorActionPreference = "Stop"

powershell -ExecutionPolicy Bypass -File scripts/run-demo.ps1 -BuildDir $BuildDir -AudioBackend midiout -MidiOutDevice $MidiOutDevice -KeyboardPath $KeyboardPath -ScorePath $ScorePath -ProbeKey Q
Write-Host "MIDI out smoke completed."
