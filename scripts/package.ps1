param(
    [ValidateSet("Debug", "Release")]
    [string]$BuildType = "Release",
    [string]$BuildDir = "build-release",
    [string]$OutputDir = "dist"
)

$ErrorActionPreference = "Stop"

$exePath = Join-Path $BuildDir "piano_cli.exe"
if (-not (Test-Path $exePath)) {
    throw "Executable not found: $exePath. Run configure/build first."
}

$stageDir = Join-Path $OutputDir "piano-win-x64"
$zipPath = Join-Path $OutputDir "piano-win-x64.zip"

if (Test-Path $stageDir) {
    Remove-Item $stageDir -Recurse -Force
}
if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}

New-Item -ItemType Directory -Path $stageDir -Force | Out-Null
Copy-Item $exePath (Join-Path $stageDir "piano_cli.exe")
Copy-Item "README.md" (Join-Path $stageDir "README.md")
Copy-Item "assets" (Join-Path $stageDir "assets") -Recurse -Force

Compress-Archive -Path (Join-Path $stageDir "*") -DestinationPath $zipPath
Write-Host "Package generated: $zipPath"
