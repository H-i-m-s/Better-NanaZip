# Build the NanaZip package, then sign the generated MSIX/MSIXBundle locally.
# This invokes the repository's BuildAllTargets.cmd, which cleans Output first.

[CmdletBinding()]
param(
    [string]$RepositoryRoot,
    [string]$Subject = 'CN=SSS NanaZip Development',
    [switch]$SkipBuild,
    [switch]$SignAllMsix
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}
$RepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
$buildScript = Join-Path $RepositoryRoot 'BuildAllTargets.cmd'
$outputRoot = Join-Path $RepositoryRoot 'Output\Binaries\AppPackages'
if (-not (Test-Path -LiteralPath $buildScript -PathType Leaf)) { throw "Build script not found: $buildScript" }

if (-not $SkipBuild) {
    Push-Location $RepositoryRoot
    try {
        & $buildScript
        if ($LASTEXITCODE -ne 0) { throw "NanaZip build failed. Exit code: $LASTEXITCODE" }
    } finally {
        Pop-Location
    }
}

if (-not (Test-Path -LiteralPath $outputRoot -PathType Container)) {
    throw "AppPackages output directory not found: $outputRoot"
}

$packages = @(Get-ChildItem -LiteralPath $outputRoot -File -Include *.msix,*.msixbundle,*.appx,*.appxbundle -Recurse)
if ($packages.Count -eq 0) { throw "No MSIX/AppX package was found under $outputRoot" }

Write-Host 'Generated packages:'
$packages | Select-Object -ExpandProperty FullName | ForEach-Object { Write-Host "  $_" }

if ($SignAllMsix) {
    $signScript = Join-Path $PSScriptRoot 'Sign-SSSNanaZipPackage.ps1'
    foreach ($package in $packages) {
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $signScript -PackagePath $package.FullName -Subject $Subject
        if ($LASTEXITCODE -ne 0) { throw "Signing failed: $($package.FullName)" }
    }
    Write-Host 'All generated packages were signed and verified.'
}

Write-Host "Output directory: $outputRoot"
