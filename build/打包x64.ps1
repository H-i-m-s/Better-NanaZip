# ============================================================
# 打包x64.ps1 —— 增量编译 + x64 单平台 msix 打包 + 签名
# 位置：仓库 build\ 目录（随仓库走，可上 GitHub；克隆后直接可用）
# 用途：日常验证"最新功能"时，不想等全量构建（BuildAllTargets 清空 Output ~21min）
#       用它：增量编译（只编改动的）→ wapproj 打包（跳过未变项目，~2min）→ 签名
# 产物：<仓库根>\Output\Binaries\AppPackages\NanaZipPackage_<ver>_x64_Test\*.msix
# 安装：build\SSSDevSigning\Install-SSSNanaZipPackage.ps1 -PackagePath <msix> -ForceUpdate
# 依赖：开发证书（store-only，Cert:\CurrentUser\My，CN=SSS NanaZip Development），
#       以及 build\SSSDevSigning\Sign-SSSNanaZipPackage.ps1 同目录存在。
# ============================================================
$ErrorActionPreference = 'Stop'

$root      = Split-Path -Parent $PSScriptRoot   # build 的父目录 = 仓库根
$vcvars64  = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
$fmProj    = 'NanaZip.UI.Modern\NanaZip.Modern.FileManager.vcxproj'
$wapproj   = 'NanaZipPackage\NanaZipPackage.wapproj'
$signScript = Join-Path $PSScriptRoot 'SSSDevSigning\Sign-SSSNanaZipPackage.ps1'

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

# ---------- 3. wapproj 打包：x64 单平台 msix（不编 bundle、不做 arm64）----------
"`n=== wapproj 打包 (x64 单平台 msix) ==="
$t = Get-Date
& cmd.exe /c "call `"$vcvars64`" >nul 2>&1 && cd /d $root && MSBuild `"$wapproj`" /t:Build /p:Configuration=Release /p:Platform=x64 /p:PreferredToolArchitecture=x64 /p:AppxBundlePlatforms=x64 /p:AppxBundle=Never /m:8 /v:m /nologo" | Out-Host
if ($LASTEXITCODE -ne 0) { throw "wapproj 打包失败 (退出码 $LASTEXITCODE)" }
"打包完成 (耗时 $([Math]::Round(((Get-Date)-$t).TotalSeconds))s)"

# ---------- 4. 定位产物并签名 ----------
"`n=== 签名 ==="
$pkgDir = Get-ChildItem "$root\Output\Binaries\AppPackages" -Directory -Filter 'NanaZipPackage_*_x64_Test' |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $pkgDir) { throw '未找到打包产物目录' }
$msix = Get-ChildItem $pkgDir.FullName -File -Filter '*.msix' |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $msix) { throw '未找到 .msix 产物' }

& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $signScript -PackagePath $msix.FullName
if ($LASTEXITCODE -ne 0) { throw "签名失败: $($msix.FullName)" }

"`n=== 产物 ===`n$($msix.FullName)"
'`n完成。安装命令：'
"build\SSSDevSigning\Install-SSSNanaZipPackage.ps1 -PackagePath `"$($msix.FullName)`" -ForceUpdate"
