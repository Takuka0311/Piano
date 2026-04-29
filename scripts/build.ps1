param(
    [ValidateSet("Debug", "Release")]
    [string]$BuildType = "Debug",
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

if (Get-Command cmake -ErrorAction SilentlyContinue) {
    cmake --build $BuildDir --config $BuildType
    if ($LASTEXITCODE -ne 0) {
        throw "cmake build failed."
    }
    Write-Host "Build completed for $BuildType"
    exit 0
}

$vsCmake = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vsCmake) -or -not (Test-Path $vcvars)) {
    throw "cmake not in PATH, and Visual Studio Build Tools bundled cmake not found."
}

$cmd = "`"$vcvars`" && `"$vsCmake`" --build $BuildDir --config $BuildType"
cmd /c $cmd
if ($LASTEXITCODE -ne 0) {
    throw "cmake build failed via VS toolchain."
}
Write-Host "Build completed for $BuildType"
