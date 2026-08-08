# Verify an MSIX/MSIXBundle signature.

[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$PackagePath
)

$ErrorActionPreference = 'Stop'
$PackagePath = [System.IO.Path]::GetFullPath($PackagePath)
if (-not (Test-Path -LiteralPath $PackagePath -PathType Leaf)) { throw "Package not found: $PackagePath" }

$signtool = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe'
if (-not (Test-Path -LiteralPath $signtool)) { throw "SignTool not found: $signtool" }
& $signtool verify /pa /v $PackagePath
if ($LASTEXITCODE -ne 0) { throw "Package signature verification failed. Exit code: $LASTEXITCODE" }
Write-Host "Package signature verified: $PackagePath"
