# ============================================================
# 03-组装文件集.ps1 —— 从 msixbundle 组装绿色版文件集 + 生成壳包
# 位置：build\步骤\ 目录
# 参数：
#   -BundlePath  msixbundle 路径（必填）
#   -Version     版本号（默认从 bundle 文件名解析）
# 输出：绿色版目录路径（写 $Output 变量 / 打印）
# ============================================================
param(
    [string]$BundlePath = '',
    [string]$Version = ''
)
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $BundlePath) { throw '-BundlePath 必填' }
if (-not (Test-Path -LiteralPath $BundlePath)) { throw "bundle 不存在: $BundlePath" }
if (-not $Version) {
    $Version = [regex]::Match([System.IO.Path]::GetFileName($BundlePath), '_(\d+\.\d+\.\d+\.\d+)_').Groups[1].Value
    if (-not $Version) { throw '无法从文件名解析版本，请传 -Version' }
}
"版本: $Version"

$appPackages = "$root\Output\Binaries\AppPackages"
$work  = "$appPackages\_组装_临时_$Version"
$green = "$appPackages\NanaZip绿色版_${Version}_x64"

# ---------- 1. 解 bundle → x64 成员 msix → payload ----------
if (Test-Path $work) { Remove-Item $work -Recurse -Force }
New-Item -ItemType Directory -Path $work | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::ExtractToDirectory($BundlePath, "$work\bundle")
$msix = Get-ChildItem "$work\bundle" -Recurse -Filter '*.msix' | Where-Object { $_.Name -notmatch 'arm64' } | Select-Object -First 1
if (-not $msix) { throw "bundle 中未找到 x64 成员 msix" }
[System.IO.Compression.ZipFile]::ExtractToDirectory($msix.FullName, "$work\payload")
"已解包: $($msix.Name)"

# ---------- 2. 组装绿色版文件集 ----------
if (Test-Path $green) { Remove-Item $green -Recurse -Force }
New-Item -ItemType Directory -Path $green | Out-Null
$payload = "$work\payload"
@(
    'K7Base.dll', 'K7User.dll', 'Mile.Xaml.Styles.SunValley.xbf',
    'NanaZip.Codecs.dll', 'NanaZip.Core.Console.sfx', 'NanaZip.Core.dll',
    'NanaZip.Core.Windows.sfx', 'NanaZip.Modern.dll', 'NanaZip.Modern.FileManager.exe',
    'NanaZip.Modern.pri', 'NanaZip.Modern.winmd', 'NanaZip.ShellExtension.dll',
    'NanaZip.Universal.Console.exe', 'NanaZip.Universal.Windows.exe', 'resources.pri'
) | ForEach-Object {
    if (-not (Test-Path "$payload\$_")) { throw "payload 缺文件: $_" }
    Copy-Item "$payload\$_" $green
}
"文件集: $((Get-ChildItem $green -File).Count) 个文件"

# ---------- 3. 生成壳包（manifest 模板替换版本 → makeappx → 签名）----------
$shellSrc = Join-Path (Split-Path -Parent $PSScriptRoot) '安装器\ShellExt'
$shellWork = "$work\shellmsix"
New-Item -ItemType Directory -Path "$shellWork\Assets" | Out-Null
(Get-Content -LiteralPath "$shellSrc\AppxManifest.xml" -Raw).Replace('{VERSION}', $Version) | Set-Content -LiteralPath "$shellWork\AppxManifest.xml" -NoNewline -Encoding UTF8
Copy-Item "$payload\Assets\StoreLogo.scale-100.png" "$shellWork\StoreLogo.png"
Copy-Item "$payload\Assets\StoreLogo.scale-400.png" "$shellWork\Assets\StoreLogo.png"
Copy-Item "$payload\Assets\Square44x44Logo.scale-100.png" "$shellWork\Assets\Square44x44Logo.png"
Copy-Item "$payload\Assets\Square150x150Logo.scale-100.png" "$shellWork\Assets\Square150x150Logo.png"
@'
<?xml version="1.0" encoding="utf-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="xml" ContentType="application/xml" />
  <Default Extension="png" ContentType="image/png" />
</Types>
'@ | Set-Content -LiteralPath "$shellWork\[Content_Types].xml" -NoNewline -Encoding UTF8

$makeappx = Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin' -Recurse -Filter 'makeappx.exe' -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
$signtool = Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin' -Recurse -Filter 'signtool.exe' -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
if (-not $makeappx -or -not $signtool) { throw '未找到 makeappx / signtool' }

$shellMsix = "$green\NanaZipShellExt.x64.msix"
if (Test-Path $shellMsix) { Remove-Item $shellMsix -Force }
& $makeappx.FullName pack /d $shellWork /p $shellMsix /o /nv 2>&1 | Out-Null
if (-not (Test-Path $shellMsix)) { throw '壳包打包失败' }
& $signtool.FullName sign /fd SHA256 /sha1 0E7475D74EFAEDDB26D4BDA02FBC6551DC8D72A5 $shellMsix 2>&1 | Out-Null
"壳包: NanaZipShellExt.x64.msix ($([Math]::Round((Get-Item $shellMsix).Length/1KB))KB, 已签名)"

# ---------- 4. 注册脚本与绿色版右键菜单 ----------
Copy-Item "$shellSrc\RegisterShellExt.cmd" $green
Copy-Item "$shellSrc\RegisterShellExt.ps1" $green
Copy-Item "$shellSrc\UnregisterShellExt.cmd" $green
Copy-Item "$shellSrc\绿色版右键菜单\SetFileAssoc.ps1" $green
# 绿色版一键右键菜单（含自提权装证书；exe 安装器文件集同源递归，会一并带入 {app} 备用）
Copy-Item "$shellSrc\绿色版右键菜单\安装右键菜单.cmd" $green
Copy-Item "$shellSrc\绿色版右键菜单\移除右键菜单.cmd" $green
$cer = "$root\.local\SSSDevSigning\SSS-NanaZip-Development.cer"
if (Test-Path -LiteralPath $cer) {
    Copy-Item -LiteralPath $cer $green
} else {
    Write-Warning '未找到开发证书 cer，绿色版右键菜单首次安装将缺证书文件'
}

Write-Output "GREEN=$green"
