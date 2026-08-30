# ============================================================
# NanaZip 壳包注册（供 RegisterShellExt.cmd 调起，也可手动运行）
# 职责：把 NanaZipShellExt.x64.msix 注册为 sparse package（Win11 右键一级菜单）
# 幂等：已注册则先移除再注册（支持升级/目录移动后重装）
# 日志：同目录 ShellExt注册日志.txt（排查"别人机器没有右键菜单"的关键证据）
# 前提：签名证书必须在 LocalMachine\TrustedPeople（exe 安装器会在本步之前提权导入）
# ============================================================
$ErrorActionPreference = 'Stop'
$dir = $PSScriptRoot
$log = Join-Path $dir 'ShellExt注册日志.txt'
$hash = '0E7475D74EFAEDDB26D4BDA02FBC6551DC8D72A5'
$pkgName = 'SSS.NanaZip.ShellExtension'
$msix = Join-Path $dir 'NanaZipShellExt.x64.msix'

function Log([string]$msg) {
    $line = '[{0:yyyy-MM-dd HH:mm:ss}] {1}' -f (Get-Date), $msg
    Write-Host $msg
    try { Add-Content -LiteralPath $log -Value $line -Encoding UTF8 } catch { }
}

$exitCode = 1
Log '==== NanaZip 壳包注册开始 ===='
try {
    $os = [System.Environment]::OSVersion.Version
    Log ('系统版本: {0}.{1}.{2}' -f $os.Major, $os.Minor, $os.Build)

    if ($os.Build -lt 17763) {
        throw ("Windows 版本过旧（build {0} < 17763），不支持 sparse package，右键菜单不可用" -f $os.Build)
    }
    if (-not (Test-Path -LiteralPath $msix)) {
        throw "找不到壳包文件: $msix"
    }

    # 证书检查（未安装则必败，提前给出明确原因）
    $certOk = Test-Path "Cert:\LocalMachine\TrustedPeople\$hash"
    Log ('签名证书 TrustedPeople: ' + $(if ($certOk) { '已安装' } else { '未安装' }))
    if (-not $certOk) {
        throw ('签名证书未安装。请在管理员 PowerShell 运行: Import-Certificate -FilePath "{0}" -CertStoreLocation Cert:\LocalMachine\TrustedPeople' -f (Join-Path $dir 'SSS-NanaZip-Development.cer'))
    }

    # 幂等注册：先移除旧注册（升级/重装/目录移动场景），再注册
    $existing = Get-AppxPackage -Name $pkgName -ErrorAction SilentlyContinue
    if ($existing) {
        Log ('检测到已注册版本 {0}，先移除' -f $existing.Version)
        $existing | Remove-AppxPackage -ErrorAction Stop
    }

    Log '正在注册壳包 (Add-AppxPackage -ExternalLocation)...'
    Add-AppxPackage -Path $msix -ExternalLocation $dir -ErrorAction Stop

    $now = Get-AppxPackage -Name $pkgName -ErrorAction SilentlyContinue
    if (-not $now) { throw '注册后未查询到包，疑似失败' }
    Log ('注册成功: {0} @ {1}' -f $now.Version, $now.InstallLocation)
    $exitCode = 0
}
catch {
    Log ('注册失败: ' + $_.Exception.Message)
    Log 'Win11 一级菜单不可用；传统菜单（右键 - 显示更多选项）不受影响'
    $exitCode = 1
}
Log '==== NanaZip 壳包注册结束 ===='
exit $exitCode
