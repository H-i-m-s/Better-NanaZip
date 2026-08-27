# Remove only the SSS development certificate created by these scripts.

[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$Subject = 'CN=SSS NanaZip Development',
    [switch]$DeleteFiles
)

$ErrorActionPreference = 'Stop'
Get-ChildItem Cert:\CurrentUser\TrustedPeople | Where-Object Subject -eq $Subject | ForEach-Object {
    if ($PSCmdlet.ShouldProcess($_.Thumbprint, 'Remove from TrustedPeople')) { Remove-Item $_.PSPath }
}
Get-ChildItem Cert:\CurrentUser\My | Where-Object Subject -eq $Subject | ForEach-Object {
    if ($PSCmdlet.ShouldProcess($_.Thumbprint, 'Remove from Personal store')) { Remove-Item $_.PSPath }
}

if ($DeleteFiles) {
    $dir = Join-Path (Split-Path -Parent $PSScriptRoot) '..\.local\SSSDevSigning'
    $dir = [System.IO.Path]::GetFullPath($dir)
    if (Test-Path -LiteralPath $dir) {
        if ($PSCmdlet.ShouldProcess($dir, 'Remove local certificate files')) { Remove-Item -LiteralPath $dir -Recurse -Force }
    }
}
