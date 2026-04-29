param(
    [string]$BuildDir = "build-release",
    [string]$KeyboardPath = "assets/default.keyboard",
    [string]$ScorePath = "assets/demo.in",
    [string]$VstiPluginPath
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($VstiPluginPath)) {
    throw "Please provide -VstiPluginPath, for example mdaPiano.dll"
}

powershell -ExecutionPolicy Bypass -File scripts/run-demo.ps1 -BuildDir $BuildDir -AudioBackend vsti -VstiPluginPath $VstiPluginPath -KeyboardPath $KeyboardPath -ScorePath $ScorePath -ProbeKey Q
Write-Host "VSTi smoke completed."
