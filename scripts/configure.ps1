param(
    [ValidateSet("Debug", "Release")]
    [string]$BuildType = "Debug",
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

if (Get-Command cmake -ErrorAction SilentlyContinue) {
    cmake -S . -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=$BuildType -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    Write-Host "Configured with $BuildType in $BuildDir"
    exit 0
}

$vsCmake = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$vsNinja = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vsCmake) -or -not (Test-Path $vsNinja) -or -not (Test-Path $vcvars)) {
    throw "cmake/ninja not in PATH, and Visual Studio Build Tools bundled cmake/ninja not found."
}

$cmd = "`"$vcvars`" && `"$vsCmake`" -S . -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=$BuildType -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_MAKE_PROGRAM=`"$vsNinja`""
cmd /c $cmd
Write-Host "Configured with $BuildType in $BuildDir"
