# ============================================================
# NanaZip 绿色版一键安装右键菜单（由 安装右键菜单.cmd 调起）
# 做三件事：
#   1) 首次运行安装签名证书（需管理员，一次 UAC；已装则零 UAC）
#   2) 注册 MSIX 壳包（Win11 一级菜单，用户级免管理员）
#   3) 注册传统 verb（"显示更多选项"兜底，用户级）
# 全部可逆：运行"移除右键菜单.cmd"即可还原纯绿状态
# ============================================================
param(
    [switch]$SkipCertCheck
)
$ErrorActionPreference = 'Continue'
$dir = $PSScriptRoot
$hash = '0E7475D74EFAEDDB26D4BDA02FBC6551DC8D72A5'

Write-Host ''
Write-Host '==== NanaZip 绿色版 - 安装右键菜单 ====' -ForegroundColor Cyan
Write-Host ''

# ---------- 检查文件 ----------
if (-not (Test-Path (Join-Path $dir 'NanaZipShellExt.x64.msix'))) {
    Write-Host '[错误] 当前目录找不到 NanaZipShellExt.x64.msix' -ForegroundColor Red
    Write-Host '请把本脚本放在 NanaZip 绿色版目录内运行。'
    Read-Host '按回车退出'
    exit 1
}

function Test-CertInstalled {
    return (Test-Path "Cert:\LocalMachine\TrustedPeople\$hash")
}

# ---------- 1. 证书（首次需要，已装跳过） ----------
if ($SkipCertCheck -or (Test-CertInstalled)) {
    Write-Host '[1/3] 签名证书已存在，跳过'
} else {
    $cer = Join-Path $dir 'SSS-NanaZip-Development.cer'
    if (-not (Test-Path $cer)) {
        Write-Host '[错误] 首次使用需要证书文件 SSS-NanaZip-Development.cer' -ForegroundColor Red
        Write-Host '请把它放到本目录后重试。'
        Read-Host '按回车退出'
        exit 1
    }
    # 非管理员 → 自提权重跑（提权窗口内完成全部步骤）
    $isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $isAdmin) {
        Write-Host '首次运行需要安装签名证书，正在请求管理员权限...'
        Start-Process powershell.exe -Verb RunAs -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`"")
        exit 0
    }
    Import-Certificate -FilePath $cer -CertStoreLocation 'Cert:\LocalMachine\TrustedPeople' -ErrorAction Stop | Out-Null
    Write-Host '[1/3] 签名证书已安装（一次 UAC，以后不再需要）'
}

# ---------- 2. 注册壳包（Win11 一级菜单） ----------
Write-Host '[2/3] 注册 Win11 右键一级菜单...'
try {
    Add-AppxPackage -Path (Join-Path $dir 'NanaZipShellExt.x64.msix') -ExternalLocation $dir -ErrorAction Stop
} catch {
    Write-Host '[错误] 壳包注册失败：' -ForegroundColor Red
    Write-Host $_.Exception.Message
    Write-Host '可能原因：已注册过（先运行"移除右键菜单.cmd"再试）；绿色版目录移动过（同样先移除再装）。'
    Read-Host '按回车退出'
    exit 1
}

# ---------- 3. 传统 verb（Win10 / "显示更多选项"兜底） ----------
Write-Host '[3/3] 注册"显示更多选项"备用菜单...'
$fm = Join-Path $dir 'NanaZip.Modern.FileManager.exe'
$console = Join-Path $dir 'NanaZip.Universal.Console.exe'

# 用 .NET Registry API：路径里的字面 * 键在 PowerShell provider 里会被当通配符，
# 外部 reg.exe 会被 PS5.1 的参数引号转义 bug 搞坏，只有 .NET API 既字面又可靠。
# 注意 RegistryKey.Parent 只有 .NET Core 才有，PS5.1(.NET Framework) 必须分别打开父键。
$rc = [Microsoft.Win32.Registry]::CurrentUser

$k = $rc.CreateSubKey('Software\Classes\*\shell\NanaZipOpen')
$k.SetValue('', '用 NanaZip 打开', [Microsoft.Win32.RegistryValueKind]::String)
$k.SetValue('Icon', "$fm,0", [Microsoft.Win32.RegistryValueKind]::String)
$k.Close()
$k = $rc.CreateSubKey('Software\Classes\*\shell\NanaZipOpen\command')
$k.SetValue('', ('"' + $fm + '" "%1"'), [Microsoft.Win32.RegistryValueKind]::String)
$k.Close()

$k = $rc.CreateSubKey('Software\Classes\*\shell\NanaZipExtract')
$k.SetValue('', '用 NanaZip 解压到当前文件夹', [Microsoft.Win32.RegistryValueKind]::String)
$k.Close()
$k = $rc.CreateSubKey('Software\Classes\*\shell\NanaZipExtract\command')
$k.SetValue('', ('"' + $console + '" x -y "%1"'), [Microsoft.Win32.RegistryValueKind]::String)
$k.Close()

$k = $rc.CreateSubKey('Software\Classes\Directory\shell\NanaZipOpenDir')
$k.SetValue('', '用 NanaZip 打开', [Microsoft.Win32.RegistryValueKind]::String)
$k.Close()
$k = $rc.CreateSubKey('Software\Classes\Directory\shell\NanaZipOpenDir\command')
$k.SetValue('', ('"' + $fm + '" "%1"'), [Microsoft.Win32.RegistryValueKind]::String)
$k.Close()

Write-Host ''
Write-Host '============================================' -ForegroundColor Green
Write-Host '完成！右键压缩包或文件夹试试：' -ForegroundColor Green
Write-Host '  Win11 新菜单：直接显示 NanaZip 项'
Write-Host '  兜底入口：右键 - 显示更多选项'
Write-Host '注意：绿色版目录移动后请先"移除"再重新"安装"。'
Write-Host '============================================' -ForegroundColor Green
Read-Host '按回车退出'
