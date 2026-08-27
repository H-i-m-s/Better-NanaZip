# Create a local SSS NanaZip development package-signing certificate.
# The store-only mode keeps the private key in Cert:\CurrentUser\My.

[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$Subject = 'CN=SSS NanaZip Development',
    [string]$OutputDirectory,
    [string]$PfxPassword,
    [switch]$StoreOnly,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path (Split-Path -Parent $PSScriptRoot) '..\.local\SSSDevSigning'
}

$secure = $null
if (-not $StoreOnly) {
    if ([string]::IsNullOrWhiteSpace($PfxPassword)) {
        $secure = Read-Host 'Enter PFX password' -AsSecureString
    } else {
        $secure = ConvertTo-SecureString $PfxPassword -AsPlainText -Force
    }
}

$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$pfxPath = Join-Path $OutputDirectory 'SSS-NanaZip-Development.pfx'
$cerPath = Join-Path $OutputDirectory 'SSS-NanaZip-Development.cer'
$metaPath = Join-Path $OutputDirectory 'certificate-info.txt'

$existing = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -eq $Subject -and $_.HasPrivateKey -and $_.NotAfter -gt (Get-Date) } | Select-Object -First 1
if ($existing -and -not $Force) {
    throw "A valid development certificate already exists: $($existing.Thumbprint). Use it, or pass -Force to replace it."
}

$cert = New-SelfSignedCertificate `
    -Type Custom `
    -Subject $Subject `
    -FriendlyName 'SSS NanaZip Development Package Signing' `
    -CertStoreLocation 'Cert:\CurrentUser\My' `
    -KeyAlgorithm RSA `
    -KeyLength 2048 `
    -HashAlgorithm SHA256 `
    -KeyExportPolicy Exportable `
    -NotAfter (Get-Date).AddYears(5) `
    -TextExtension @(
        '2.5.29.37={text}1.3.6.1.5.5.7.3.3',
        '2.5.29.19={text}false'
    )

if (-not $StoreOnly) {
    Export-PfxCertificate -Cert $cert -FilePath $pfxPath -Password $secure | Out-Null
}
Export-Certificate -Cert $cert -FilePath $cerPath | Out-Null

@(
    "Subject=$($cert.Subject)"
    "Issuer=$($cert.Issuer)"
    "Thumbprint=$($cert.Thumbprint)"
    "NotBefore=$($cert.NotBefore.ToString('s'))"
    "NotAfter=$($cert.NotAfter.ToString('s'))"
    "HasPrivateKey=$($cert.HasPrivateKey)"
    "Publisher=$Subject"
    "PfxPath=$(if ($StoreOnly) { '[store-only; private key remains in Cert:\CurrentUser\My]' } else { $pfxPath })"
    "CerPath=$cerPath"
) | Set-Content -LiteralPath $metaPath -Encoding UTF8

Write-Host 'Development certificate created.'
Write-Host "Subject:    $($cert.Subject)"
Write-Host "Thumbprint: $($cert.Thumbprint)"
if ($StoreOnly) {
    Write-Host 'Private key: kept in Cert:\CurrentUser\My; no PFX exported.'
} else {
    Write-Host "PFX:        $pfxPath"
}
Write-Host "CER:        $cerPath"
Write-Host 'Next: run Install-SSSDevCertificate.ps1.'
