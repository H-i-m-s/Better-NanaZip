# Verify an MSIX/MSIXBundle signature.

[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$PackagePath
)

$ErrorActionPreference = 'Stop'
$PackagePath = [System.IO.Path]::GetFullPath($PackagePath)
if (-not (Test-Path -LiteralPath $PackagePath -PathType Leaf)) { throw "Package not found: $PackagePath" }

# Locate signtool from the newest installed Windows SDK (any SDK version).
$signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\x64\\' } |
    Sort-Object FullName -Descending | Select-Object -First 1
if (-not $signtool) { throw 'SignTool not found in Windows Kits' }
& $signtool.FullName verify /pa /v $PackagePath
if ($LASTEXITCODE -ne 0) { throw "Package signature verification failed. Exit code: $LASTEXITCODE" }
Write-Host "Package signature verified: $PackagePath"
