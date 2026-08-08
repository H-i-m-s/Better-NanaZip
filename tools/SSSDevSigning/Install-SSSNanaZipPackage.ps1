# Install an SSS development MSIX/MSIXBundle for the current user.
# The public certificate is imported into Trusted People before Add-AppxPackage.

[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$PackagePath,
    [string]$CertificatePath,
    [switch]$ForceUpdate
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($CertificatePath)) {
    $CertificatePath = Join-Path (Split-Path -Parent $PSScriptRoot) '..\.local\SSSDevSigning\SSS-NanaZip-Development.cer'
}
$PackagePath = [System.IO.Path]::GetFullPath($PackagePath)
$CertificatePath = [System.IO.Path]::GetFullPath($CertificatePath)
if (-not (Test-Path -LiteralPath $PackagePath -PathType Leaf)) { throw "Package not found: $PackagePath" }
if (-not (Test-Path -LiteralPath $CertificatePath -PathType Leaf)) { throw "Public certificate not found: $CertificatePath" }

$cert = Get-PfxCertificate -FilePath $CertificatePath
$trusted = Get-ChildItem Cert:\CurrentUser\TrustedPeople | Where-Object Thumbprint -eq $cert.Thumbprint
if (-not $trusted) {
    Import-Certificate -FilePath $CertificatePath -CertStoreLocation Cert:\CurrentUser\TrustedPeople | Out-Null
    Write-Host 'Imported development certificate into CurrentUser Trusted People.'
}

$params = @{ Path = $PackagePath; ErrorAction = 'Stop' }
if ($ForceUpdate) { $params['ForceUpdateFromAnyVersion'] = $true }
Add-AppxPackage @params
Write-Host "Installation succeeded: $PackagePath"
