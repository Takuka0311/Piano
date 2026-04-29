param(
    [string]$ZipPath = "dist/piano-win-x64.zip"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $ZipPath)) {
    throw "Package not found: $ZipPath"
}

$tempDir = Join-Path $env:TEMP ("piano-verify-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

try {
    Expand-Archive -Path $ZipPath -DestinationPath $tempDir -Force
    $required = @(
        "piano_cli.exe",
        "README.md",
        "assets/default.keyboard",
        "assets/demo.in"
    )
    foreach ($relative in $required) {
        $full = Join-Path $tempDir $relative
        if (-not (Test-Path $full)) {
            throw "Package validation failed, missing: $relative"
        }
    }
    Write-Host "Package validation passed: $ZipPath"
}
finally {
    if (Test-Path $tempDir) {
        Remove-Item $tempDir -Recurse -Force
    }
}
