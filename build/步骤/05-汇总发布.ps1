# ============================================================
# 05-汇总发布.ps1 —— 收集产物到 Output\发布\<版本>\，生成安装说明
# 位置：build\步骤\ 目录
# 参数：
#   -Version    版本号
#   -BundlePath msixbundle（可空）
#   -SetupExe   setup.exe（可空）
#   -GreenDir   绿色版目录（可空，有则打 zip）
# ============================================================
param(
    [string]$Version = '',
    [string]$BundlePath = '',
    [string]$SetupExe = '',
    [string]$GreenDir = ''
)
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $Version) { throw '-Version 必填' }
$pubDir = "$root\Output\发布\${Version}"
if (Test-Path $pubDir) { Remove-Item $pubDir -Recurse -Force }
New-Item -ItemType Directory -Path $pubDir | Out-Null
"发布目录: $pubDir"

# ---------- 收集产物 ----------
if ($BundlePath -and (Test-Path -LiteralPath $BundlePath)) {
    Copy-Item -LiteralPath $BundlePath $pubDir
    "  [msix]  $([System.IO.Path]::GetFileName($BundlePath))"
}
if ($SetupExe -and (Test-Path -LiteralPath $SetupExe)) {
    Copy-Item -LiteralPath $SetupExe $pubDir
    "  [exe]   $([System.IO.Path]::GetFileName($SetupExe))"
}
if ($GreenDir -and (Test-Path -LiteralPath $GreenDir)) {
    $zip = "$pubDir\NanaZip绿色版_${Version}_x64.zip"
    Compress-Archive -Path "$GreenDir\*" -DestinationPath $zip
    "  [zip]   NanaZip绿色版_${Version}_x64.zip"
}
$cer = "$root\.local\SSSDevSigning\SSS-NanaZip-Development.cer"
if (Test-Path -LiteralPath $cer) { Copy-Item -LiteralPath $cer $pubDir; "  [cer]   SSS-NanaZip-Development.cer" }

# ---------- 安装说明 ----------
$lines = @()
$lines += "【NanaZip 定制版 $Version · 安装说明】"
$lines += ""
$lines += "三种形态，任选其一："
if ($SetupExe -and (Test-Path -LiteralPath $SetupExe)) {
    $lines += ""
    $lines += "1. exe 安装版（推荐给大多数人）"
    $lines += "   双击 NanaZipSetup_${Version}_x64.exe 即装，无需证书/管理员/旁加载。"
    $lines += "   装完自带：FM 文件管理器、.7z/.zip/.rar 关联、Win11 右键菜单（自动注册）。"
    $lines += "   卸载：控制面板 → 卸载程序。"
}
if ($GreenDir -and (Test-Path -LiteralPath $GreenDir)) {
    $lines += ""
    $lines += "2. 绿色版（解压即用）"
    $lines += "   解压 NanaZip绿色版_${Version}_x64.zip 到任意目录，运行 NanaZip.Modern.FileManager.exe。"
    $lines += "   无右键菜单/文件关联（绿色版不注册）。"
}
if ($BundlePath -and (Test-Path -LiteralPath $BundlePath)) {
    $lines += ""
    $lines += "3. MSIX 版（系统集成最完整）"
    $lines += "   步骤：a) 双击 SSS-NanaZip-Development.cer → 安装证书 → 当前用户 → 受信任的人"
    $lines += "         b) 设置 → 开发者选项 → 启用开发人员模式（或旁加载）"
    $lines += "         c) 双击 $( [System.IO.Path]::GetFileName($BundlePath) ) 安装"
}
$lines += ""
$lines += "【说明】"
$lines += "· 自签名开发版（CN=SSS NanaZip Development），非 Microsoft Store 版"
$lines += "· 同一机器同版本内容不同无法覆盖，重装前先卸载旧版"
$lines += "· 证书指纹：0E7475D74EFAEDDB26D4BDA02FBC6551DC8D72A5"
$lines += ""
[System.IO.File]::WriteAllLines("$pubDir\安装说明.txt", $lines, [System.Text.Encoding]::UTF8)
"  [txt]   安装说明.txt"

"`n=== 发布产物清单 ==="
Get-ChildItem $pubDir -File | Select-Object Name, @{n='大小';e={ if ($_.Length -gt 1MB) { '{0:N1} MB' -f ($_.Length/1MB) } else { '{0:N0} KB' -f ($_.Length/1KB) } }} | Format-Table -AutoSize
"`n完成。发布目录: $pubDir"
