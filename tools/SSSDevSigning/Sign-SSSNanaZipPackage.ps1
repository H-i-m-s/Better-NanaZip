# Sign an MSIX/MSIXBundle with the SSS development certificate.
# The default mode uses the current user's certificate store and needs no PFX password.
# Use -PfxPath and -PfxPassword only when signing on another machine.

[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$PackagePath,
    [string]$Subject = 'CN=SSS NanaZip Development',
    [string]$PfxPath,
    [string]$PfxPassword,
    [string]$TimestampUrl = ''
)

$ErrorActionPreference = 'Stop'
$PackagePath = [System.IO.Path]::GetFullPath($PackagePath)
if (-not (Test-Path -LiteralPath $PackagePath -PathType Leaf)) { throw "Package not found: $PackagePath" }

$signtool = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe'
if (-not (Test-Path -LiteralPath $signtool)) { throw "SignTool not found: $signtool" }

$args = @('sign', '/fd', 'SHA256')
if ([string]::IsNullOrWhiteSpace($PfxPath)) {
    $cert = Get-ChildItem Cert:\CurrentUser\My | Where-Object {
        $_.Subject -eq $Subject -and $_.HasPrivateKey -and $_.NotAfter -gt (Get-Date)
    } | Sort-Object NotAfter -Descending | Select-Object -First 1
    if (-not $cert) { throw "No valid certificate with private key found for $Subject. Run New-SSSDevCertificate.ps1 -StoreOnly first." }
    $args += @('/sha1', $cert.Thumbprint)
    Write-Host "Using certificate store entry: $($cert.Subject) [$($cert.Thumbprint)]"
} else {
    $PfxPath = [System.IO.Path]::GetFullPath($PfxPath)
    if (-not (Test-Path -LiteralPath $PfxPath -PathType Leaf)) { throw "PFX not found: $PfxPath" }
    if ([string]::IsNullOrWhiteSpace($PfxPassword)) {
        $secure = Read-Host 'Enter PFX password' -AsSecureString
        $ptr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
        try { $PfxPassword = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr) }
        finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr) }
    }
    $args += @('/f', $PfxPath, '/p', $PfxPassword)
}

if (-not [string]::IsNullOrWhiteSpace($TimestampUrl)) {
    $args += @('/tr', $TimestampUrl, '/td', 'SHA256')
}
$args += $PackagePath

& $signtool @args
if ($LASTEXITCODE -ne 0) { throw "SignTool signing failed. Exit code: $LASTEXITCODE" }

& $signtool verify /pa /v $PackagePath
if ($LASTEXITCODE -ne 0) { throw "SignTool verification failed. Exit code: $LASTEXITCODE" }
Write-Host "Signing and verification succeeded: $PackagePath"
