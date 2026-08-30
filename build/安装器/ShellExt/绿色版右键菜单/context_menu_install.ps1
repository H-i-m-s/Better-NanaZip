# ============================================================
# NanaZip 绿色版一键安装右键菜单（由 安装右键菜单.cmd 调起）
# 做三件事（相互独立，一项失败不影响其余）：
#   1) 首次运行安装签名证书（需管理员，一次 UAC；已装则零 UAC）
#   2) 注册 MSIX 壳包（Win11 一级菜单，用户级免管理员）
#   3) 注册传统 verb（"显示更多选项"兜底，用户级，任何情况都尝试）
# 全部可逆：运行"移除右键菜单.cmd"即可还原纯绿状态
# 日志：本目录 右键菜单安装日志.txt（远程排查就发这个文件）
# ============================================================
param(
    [switch]$SkipCertCheck
)
$ErrorActionPreference = 'Continue'
$dir = $PSScriptRoot
$log = Join-Path $dir '右键菜单安装日志.txt'
$hash = '0E7475D74EFAEDDB26D4BDA02FBC6551DC8D72A5'
$pkgName = 'SSS.NanaZip.ShellExtension'

function Log([string]$msg) {
    Write-Host $msg
    try { Add-Content -LiteralPath $log -Value ('[{0:HH:mm:ss}] {1}' -f (Get-Date), $msg) -Encoding UTF8 } catch { }
}

function Test-CertInstalled {
    return (Test-Path "Cert:\LocalMachine\TrustedPeople\$hash")
}

Log ''
Log '==== NanaZip 绿色版 - 安装右键菜单 ===='
$os = [System.Environment]::OSVersion.Version
Log ('系统版本: {0}.{1}.{2}  是否管理员: {3}' -f $os.Major, $os.Minor, $os.Build,
    ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator))

# ---------- 检查文件 ----------
if (-not (Test-Path (Join-Path $dir 'NanaZipShellExt.x64.msix'))) {
    Log '[错误] 当前目录找不到 NanaZipShellExt.x64.msix'
    Log '请把本脚本放在 NanaZip 绿色版目录内运行。'
    Read-Host '按回车退出'
    exit 1
}

$modernOk = $false

# ---------- 1. 证书（首次需要，已装跳过） ----------
if (-not ($SkipCertCheck -or (Test-CertInstalled))) {
    $cer = Join-Path $dir 'SSS-NanaZip-Development.cer'
    if (-not (Test-Path $cer)) {
        Log '[错误] 首次使用需要证书文件 SSS-NanaZip-Development.cer'
        Log '请把它放到本目录后重试。'
        Read-Host '按回车退出'
        exit 1
    }
    # 非管理员 → 自提权重跑（提权窗口内完成全部步骤）
    $isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $isAdmin) {
        Log '首次运行需要安装签名证书，正在请求管理员权限（请在 UAC 弹窗选"是"）...'
        try {
            Start-Process powershell.exe -Verb RunAs -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`"") | Out-Null
            Log '已拉起管理员窗口，请在新窗口中继续操作。'
        }
        catch {
            Log '[提示] 未获得管理员权限（UAC 被取消或被安全软件拦截）。'
            Log '影响：Win11 一级菜单装不上；仍会尝试注册传统兜底菜单。'
        }
        Read-Host '按回车退出'
        exit 0
    }
    try {
        Import-Certificate -FilePath $cer -CertStoreLocation 'Cert:\LocalMachine\TrustedPeople' -ErrorAction Stop | Out-Null
        Log '[1/3] 签名证书已安装（一次 UAC，以后不再需要）'
    }
    catch {
        Log ('[1/3] [错误] 证书导入失败: ' + $_.Exception.Message)
        Log '可能被安全软件拦截。Win11 一级菜单将不可用，继续注册传统兜底菜单。'
    }
}
else {
    Log '[1/3] 签名证书已存在，跳过'
}

# ---------- 2. 注册壳包（Win11 一级菜单） ----------
if ($os.Build -lt 17763) {
    Log ('[2/3] [跳过] Windows 版本过旧（build {0} < 17763），不支持壳包，Win11 一级菜单不可用' -f $os.Build)
}
elseif (-not (Test-CertInstalled)) {
    Log '[2/3] [跳过] 签名证书未安装（第 1 步未完成），Win11 一级菜单不可用'
}
else {
    Log '[2/3] 注册 Win11 右键一级菜单...'
    try {
        $existing = Get-AppxPackage -Name $pkgName -ErrorAction SilentlyContinue
        if ($existing) {
            Log ('      已有注册（版本 ' + $existing.Version + '），先移除（支持目录移动/重装）')
            $existing | Remove-AppxPackage -ErrorAction Stop
        }
        Add-AppxPackage -Path (Join-Path $dir 'NanaZipShellExt.x64.msix') -ExternalLocation $dir -ErrorAction Stop
        $now = Get-AppxPackage -Name $pkgName -ErrorAction SilentlyContinue
        if (-not $now) { throw '注册后未查询到包' }
        Log ('      注册成功，版本 ' + $now.Version)
        $modernOk = $true
    }
    catch {
        Log '[2/3] [失败] 壳包注册未成功:'
        Log ('      ' + $_.Exception.Message)
        Log '      常见原因：安全软件拦截；目录移动过（本脚本已自动先移除旧注册，可重试）。'
    }
}

# ---------- 3. 传统 verb（Win10 / "显示更多选项"兜底）—— 无论如何都尝试 ----------
Log '[3/3] 注册"显示更多选项"备用菜单...'
try {
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

    Log '      传统菜单注册成功'
}
catch {
    Log '[3/3] [失败] 传统菜单注册异常: ' + $_.Exception.Message
}

# ---------- 结果汇总 ----------
Log ''
Log '================ 结果 ================'
if ($modernOk) {
    Log 'Win11 新菜单：已注册（右键压缩包/文件夹直接可见）'
}
else {
    Log 'Win11 新菜单：未注册（原因见上方 [2/3] 日志）'
}
Log '传统菜单：右键 - 显示更多选项 里应有"用 NanaZip 打开 / 解压到当前文件夹"'
Log '本日志文件: ' + $log
Log '======================================'
Read-Host '按回车退出'
