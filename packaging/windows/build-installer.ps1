#!/usr/bin/env pwsh
param(
    [string]$Version = "0.3.0",
    [string]$QtDir = "C:/Qt/6.8.3/msvc2022_64",
    [string]$QtIFWDir = "C:/Qt/QtIFW-4.6.0",
    [string]$BuildDir = "build",
    [string]$OutputDir = "dist"
)

$windeployqt = Join-Path $QtDir "bin/windeployqt.exe"
$binarycreator = Join-Path $QtIFWDir "bin/binarycreator.exe"
$appBinary = Join-Path $BuildDir "Release/Papiro.exe"

if (-not (Test-Path $appBinary)) {
    Write-Error "Binary not found: $appBinary. Build first with: cmake --build build --config Release"
    exit 1
}

Write-Host "Running windeployqt..."
& $windeployqt --release --no-translations --no-compiler-runtime --no-angle --no-opengl-sw --no-pch $appBinary

$installDir = Join-Path $OutputDir "Papiro-$Version-windows"
if (Test-Path $installDir) { Remove-Item $installDir -Recurse -Force }
New-Item -ItemType Directory -Path $installDir | Out-Null

Copy-Item (Join-Path $BuildDir "Release/*") $installDir -Recurse -Force

$packageDir = "packaging/windows/qtifw/packages"
$configFile = "packaging/windows/qtifw/config/config.xml"
$installerName = "Papiro-$Version-windows-installer.exe"

Write-Host "Creating QtIFW installer..."
& $binarycreator -c $configFile -p $packageDir -f $installerName

Write-Host "Installer created: $installerName"
Write-Host "Portable folder: $installDir"