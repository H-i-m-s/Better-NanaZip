# Import the SSS development certificate public key into CurrentUser Trusted People.
# The PFX/private key is not imported here.

[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$CertificatePath
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($CertificatePath)) {
    $CertificatePath = Join-Path (Split-Path -Parent $PSScriptRoot) '..\.local\SSSDevSigning\SSS-NanaZip-Development.cer'
}
$CertificatePath = [System.IO.Path]::GetFullPath($CertificatePath)
if (-not (Test-Path -LiteralPath $CertificatePath -PathType Leaf)) {
    throw "Certificate not found: $CertificatePath. Run New-SSSDevCertificate.ps1 first."
}

$cert = Get-PfxCertificate -FilePath $CertificatePath
$existing = Get-ChildItem Cert:\CurrentUser\TrustedPeople | Where-Object Thumbprint -eq $cert.Thumbprint
if (-not $existing) {
    Import-Certificate -FilePath $CertificatePath -CertStoreLocation Cert:\CurrentUser\TrustedPeople | Out-Null
    Write-Host "Imported into Trusted People: $($cert.Subject)"
} else {
    Write-Host "Already present in Trusted People: $($cert.Subject)"
}
Write-Host "Thumbprint: $($cert.Thumbprint)"
