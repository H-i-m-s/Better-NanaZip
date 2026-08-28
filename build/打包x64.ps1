# ============================================================
# 打包x64.ps1 —— 增量编译 + x64 单成员 bundle 打包 + 签名
# 位置：仓库 build\ 目录（随仓库走，可上 GitHub；克隆后直接可用）
# 用途：日常验证"最新功能"时，不想等全量构建（BuildAllTargets 清空 Output ~21min）
#       用它：增量编译（只编改动的）→ wapproj 打包（跳过未变项目，~4min）→ 签名
#
# 版本号：
#   Windows 不允许同版本不同内容覆盖安装（0x80073CFB），所以每次装包前必须升版本。
#   运行脚本时用 -Version 传入新版本号，脚本自动改写 Package.appxmanifest，无需手动改文件：
#     & 'E:\NanaZip\build\打包x64.ps1' -Version 6.5.1826.0
#   不传 -Version 则沿用 manifest 现有版本号。
#
# 产物：<仓库根>\Output\Binaries\AppPackages\NanaZipPackage_<ver>_Test\*.msixbundle
# 说明：产出 x64 单成员 bundle（而非单 msix）。实测单 msix 无法覆盖安装已装的
#       bundle 形态包（0x80073CFB 同版本内容不同禁止重装；跨形态覆盖也会被拒）。
# 安装：build\SSSDevSigning\Install-SSSNanaZipPackage.ps1 -PackagePath <bundle> -ForceUpdate
# 依赖：开发证书（store-only，Cert:\CurrentUser\My，CN=SSS NanaZip Development），
#       以及 build\SSSDevSigning\Sign-SSSNanaZipPackage.ps1 同目录存在。
# ============================================================
param(
    [string]$Version = ''   # 新版本号，如 -Version 6.5.1826.0；留空则沿用 manifest 现有值
)
$ErrorActionPreference = 'Stop'

$root      = Split-Path -Parent $PSScriptRoot   # build 的父目录 = 仓库根
$vcvars64  = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
$fmProj    = 'NanaZip.UI.Modern\NanaZip.Modern.FileManager.vcxproj'
$wapproj   = 'NanaZipPackage\NanaZipPackage.wapproj'
$signScript = Join-Path $PSScriptRoot 'SSSDevSigning\Sign-SSSNanaZipPackage.ps1'
$manifest  = Join-Path $root 'NanaZipPackage\Package.appxmanifest'

# ---------- 0. 版本号处理（自动改写 manifest，无需手动改文件）----------
$manifestText = Get-Content -LiteralPath $manifest -Raw
if ($Version -ne '') {
    if ($Version -notmatch '^\d+\.\d+\.\d+\.\d+$') { throw "版本号格式错误: '$Version'（应为 4 段数字，如 6.5.1826.0）" }
    $newText = [regex]::Replace($manifestText, 'Version="\d+\.\d+\.\d+\.\d+"', "Version=`"$Version`"")
    if ($newText -eq $manifestText) { throw "manifest 中未找到 Version 属性: $manifest" }
    Set-Content -LiteralPath $manifest -Value $newText -NoNewline -Encoding UTF8
    "已设置版本号: $Version（$manifest）"
} else {
    $cur = [regex]::Match($manifestText, 'Version="(\d+\.\d+\.\d+\.\d+)"').Groups[1].Value
    if (-not $cur) { throw "manifest 中未找到 Version 属性: $manifest" }
    "使用 manifest 现有版本号: $cur（如需升版请加 -Version 参数）"
}

# ---------- 1. 停掉残留进程（解锁 DLL / 保证包内文件不被占用）----------
$p = Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^NanaZip' }
if ($p) { $p | Stop-Process -Force; "已关闭 $($p.Count) 个 NanaZip 进程" } else { '无 NanaZip 进程在运行' }
Start-Sleep -Seconds 2

# ---------- 2. 增量编译（单入口 FM, Release x64；未改动文件 MSBuild 自动跳过）----------
"`n=== 增量编译 (FM, Release x64) ==="
$t = Get-Date
& cmd.exe /c "call `"$vcvars64`" >nul 2>&1 && cd /d $root && MSBuild `"$fmProj`" /p:Configuration=Release /p:Platform=x64 /m /v:minimal /nologo" | Out-Host
if ($LASTEXITCODE -ne 0) { throw "编译失败 (退出码 $LASTEXITCODE)" }
"编译完成 (耗时 $([Math]::Round(((Get-Date)-$t).TotalSeconds))s)"

# ---------- 3. wapproj 打包：x64 单成员 bundle（不编 arm64）----------
"`n=== wapproj 打包 (x64 单成员 bundle) ==="
$t = Get-Date
& cmd.exe /c "call `"$vcvars64`" >nul 2>&1 && cd /d $root && MSBuild `"$wapproj`" /t:Build /p:Configuration=Release /p:Platform=x64 /p:PreferredToolArchitecture=x64 /p:AppxBundlePlatforms=x64 /m:8 /v:m /nologo" | Out-Host
if ($LASTEXITCODE -ne 0) { throw "wapproj 打包失败 (退出码 $LASTEXITCODE)" }
"打包完成 (耗时 $([Math]::Round(((Get-Date)-$t).TotalSeconds))s)"

# ---------- 4. 定位产物并签名 ----------
"`n=== 签名 ==="
$pkgDir = Get-ChildItem "$root\Output\Binaries\AppPackages" -Directory -Filter 'NanaZipPackage_*_Test' |
    Where-Object { $_.Name -notmatch 'Debug' } |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $pkgDir) { throw '未找到打包产物目录' }
$pkg = Get-ChildItem $pkgDir.FullName -File -Filter '*.msixbundle' |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $pkg) { throw '未找到 .msixbundle 产物' }

& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $signScript -PackagePath $pkg.FullName
if ($LASTEXITCODE -ne 0) { throw "签名失败: $($pkg.FullName)" }

"`n=== 产物 ===`n$($pkg.FullName)"
'`n完成。安装命令：'
"build\SSSDevSigning\Install-SSSNanaZipPackage.ps1 -PackagePath `"$($pkg.FullName)`" -ForceUpdate"
