# ============================================================
# 02-打包msix.ps1 —— wapproj 打包 + 签名，出 msixbundle
# 位置：build\步骤\ 目录
# 参数：
#   -Version  版本号（默认读 manifest 现值）
#   -Arch     x64 | both（both = x64+arm64 bundle）
#   -Sign     是否签名（默认开，-Sign:$false 跳过）
# 输出：最新 msixbundle 绝对路径（写 $Output 变量或打印）
# ============================================================
param(
    [string]$Version = '',
    [ValidateSet('x64', 'both')][string]$Arch = 'x64',
    [switch]$Sign = $true
)
$ErrorActionPreference = 'Stop'

$root     = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$vcvars64 = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
$wapproj  = 'NanaZipPackage\NanaZipPackage.wapproj'
$signScript = Join-Path (Split-Path -Parent $PSScriptRoot) 'SSSDevSigning\Sign-SSSNanaZipPackage.ps1'
$manifest = Join-Path $root 'NanaZipPackage\Package.appxmanifest'

# ---------- 0. 版本（-Version 传入则自动写入 manifest）----------
$manifestText = Get-Content -LiteralPath $manifest -Raw
if ($Version -ne '') {
    if ($Version -notmatch '^\d+\.\d+\.\d+\.\d+$') { throw "版本号格式错误: '$Version'" }
    if ($manifestText -notmatch 'Version="\d+\.\d+\.\d+\.\d+"') { throw "manifest 中未找到 Version 属性" }
    # 只替换 Identity 的 Version 属性：TargetDeviceFamily 的 MinVersion/MaxVersionTested
    # 同样是四段式 Version 属性，宽松正则会把它们一起改坏（曾把 MinVersion 改成 6.5.x）
    # 替换串用 ${1}/${2} 语法避免 PowerShell 把 $1 当变量展开
    $newText = [regex]::Replace($manifestText,
        '(<Identity[^>]*?Version=")\d+\.\d+\.\d+\.\d+("[^>]*?/?>)',
        ('${1}' + $Version + '${2}'),
        [System.Text.RegularExpressions.RegexOptions]::Singleline)
    if ($newText -eq $manifestText) { throw "Identity Version 未被替换，正则需检查" }
    if ($newText -match [regex]::Escape('$' + $Version)) { throw "替换串语法错误：版本号前出现美元符" }
    Set-Content -LiteralPath $manifest -Value $newText -NoNewline -Encoding UTF8
    "已设置版本号: $Version"
} else {
    $Version = [regex]::Match($manifestText, 'Version="(\d+\.\d+\.\d+\.\d+)"').Groups[1].Value
    "使用 manifest 现有版本号: $Version"
}

# ---------- 1. 停进程 ----------
$p = Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^NanaZip' }
if ($p) { $p | Stop-Process -Force; "已关闭 $($p.Count) 个 NanaZip 进程" }
Start-Sleep -Seconds 2

# ---------- 2. wapproj 打包 ----------
$platforms = if ($Arch -eq 'both') { 'x64|arm64' } else { 'x64' }
"`n=== wapproj 打包 (bundle: $platforms) ==="
$t = Get-Date
& cmd.exe /c "call `"$vcvars64`" >nul 2>&1 && cd /d $root && MSBuild `"$wapproj`" /t:Build /p:Configuration=Release /p:Platform=x64 /p:PreferredToolArchitecture=x64 /p:AppxBundlePlatforms=`"$platforms`" /m:8 /v:m /nologo" | Out-Host
if ($LASTEXITCODE -ne 0) { throw "wapproj 打包失败 (退出码 $LASTEXITCODE)" }
"打包完成 (耗时 $([Math]::Round(((Get-Date)-$t).TotalSeconds))s)"

# ---------- 3. 定位产物 ----------
$pkgDir = Get-ChildItem "$root\Output\Binaries\AppPackages" -Directory -Filter 'NanaZipPackage_*_Test' |
    Where-Object { $_.Name -notmatch 'Debug' } |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $pkgDir) { throw '未找到打包产物目录' }
$bundle = Get-ChildItem $pkgDir.FullName -File -Filter '*.msixbundle' |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $bundle) { throw "未找到 .msixbundle 产物（$($pkgDir.FullName)）" }
"产物: $($bundle.FullName)"

# ---------- 4. 签名 ----------
if ($Sign) {
    "`n=== 签名 ==="
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $signScript -PackagePath $bundle.FullName
    if ($LASTEXITCODE -ne 0) { throw "签名失败: $($bundle.FullName)" }
} else {
    '跳过签名（-Sign:$false）'
}
Write-Output "BUNDLE=$($bundle.FullName)"
