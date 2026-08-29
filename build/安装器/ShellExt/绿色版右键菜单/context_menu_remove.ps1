# ============================================================
# NanaZip 绿色版一键移除右键菜单（由 移除右键菜单.cmd 调起）
# 做两件事：1) 注销 MSIX 壳包（用户级，免管理员）
#           2) 删除传统 verb 注册表项（HKCU，免管理员）
# 签名证书保留在受信任区（可能被同机其他 NanaZip 形态使用），
# 如需彻底删除见文末提示。
# ============================================================
$ErrorActionPreference = 'Continue'

Write-Host ''
Write-Host '==== NanaZip 绿色版 - 移除右键菜单 ====' -ForegroundColor Cyan
Write-Host ''

# ---------- 1. 注销壳包 ----------
Write-Host '[1/2] 注销 Win11 右键一级菜单...'
try {
    Get-AppxPackage -Name 'SSS.NanaZip.ShellExtension' -ErrorAction SilentlyContinue |
        Remove-AppxPackage -ErrorAction Stop
} catch {
    Write-Host '[警告] 壳包注销返回异常（可能本来就未注册），继续。' -ForegroundColor Yellow
}

# ---------- 2. 删除传统 verb ----------
Write-Host '[2/2] 删除"显示更多选项"备用菜单...'
# .NET API：字面 * 键名，PowerShell provider 会把它当通配符
$rc = [Microsoft.Win32.Registry]::CurrentUser
foreach ($sub in @('Software\Classes\*\shell\NanaZipOpen', 'Software\Classes\*\shell\NanaZipExtract', 'Software\Classes\Directory\shell\NanaZipOpenDir')) {
    try { $rc.DeleteSubKeyTree($sub, $false) } catch { }
}

Write-Host ''
Write-Host '============================================' -ForegroundColor Green
Write-Host '已移除右键菜单，绿色版恢复纯绿状态。'
Write-Host '签名证书仍保留在受信任区（不影响安全，仅是一条信任记录）。'
Write-Host '如需彻底删除，请以管理员运行：'
Write-Host '  certutil -delstore TrustedPeople 0E7475D74EFAEDDB26D4BDA02FBC6551DC8D72A5'
Write-Host '============================================' -ForegroundColor Green
Read-Host '按回车退出'
