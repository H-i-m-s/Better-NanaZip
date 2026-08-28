# ============================================================
# 更新pkg91.ps1 —— 增量编译 + 白名单替换到 pkg91\x64
# 位置：仓库 build\ 目录（随仓库走，可上 GitHub；克隆后直接可用）
# 用途：改完代码后一键完成「关进程 → 编译 → 替换 → 核对」
# 默认含 wapproj 打包刷新 resources.pri；加 -SkipPri 跳过（只编二进制）
#
# 依赖：.local\pkg91、.local\pkg90 是 MSIX 解包运行目录（.gitignore 排除，
#       不进 git）。克隆仓库后需要先用「build\SSSDevSigning\Build-SSSNanaZipPackage.ps1」
#       全量构建并安装/解包，或手动解包官方包，生成这两个目录。
#       若 pkg90 缺失，脚本自动跳过布局核对只做替换。
# ============================================================
param(
    [switch]$SkipPri,
    [string]$Pkg91 = '',   # 目标解包目录（默认 <仓库根>\.local\pkg91）
    [string]$Pkg90 = ''    # 对照基准解包目录（默认 <仓库根>\.local\pkg90）
)
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot   # build 的父目录 = 仓库根
if ($Pkg91 -eq '') { $Pkg91 = "$root\.local\pkg91" }
if ($Pkg90 -eq '') { $Pkg90 = "$root\.local\pkg90" }
$src      = "$root\Output\Binaries\Release\x64"            # 编译主输出
$srcPkg   = "$root\Output\Binaries\Release\NanaZipPackage\x64"  # wapproj 布局源（resources.pri 等）
$dst      = "$Pkg91\x64"
$vcvars64 = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
$fmProj   = 'NanaZip.UI.Modern\NanaZip.Modern.FileManager.vcxproj'

# 目标目录缺失时自动创建；对照基准缺失时跳过核对
if (-not (Test-Path -LiteralPath $dst)) {
    New-Item -ItemType Directory -Path $dst -Force | Out-Null
    "已创建目标目录: $dst"
}
$skipLayoutCheck = -not (Test-Path -LiteralPath "$Pkg90\x64")
if ($skipLayoutCheck) { Write-Warning "对照基准目录不存在: $Pkg90\x64，本轮跳过布局核对" }

# pkg90\x64 的完整文件布局（MSIX 包内布局，白名单）
$whitelist = @(
    'AppxBlockMap.xml',
    'AppxManifest.xml',
    '[Content_Types].xml',
    'K7Base.dll',
    'K7User.dll',
    'Mile.Xaml.Styles.SunValley.xbf',
    'NanaZip.Codecs.dll',
    'NanaZip.Core.Console.sfx',
    'NanaZip.Core.dll',
    'NanaZip.Core.Windows.sfx',
    'NanaZip.Modern.dll',
    'NanaZip.Modern.FileManager.exe',
    'NanaZip.Modern.pri',
    'NanaZip.Modern.winmd',
    'NanaZip.ShellExtension.dll',
    'NanaZip.Universal.Console.exe',
    'NanaZip.Universal.Windows.exe',
    'resources.pri'
)

# ---------- 1. 停掉残留进程 ----------
$p = Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^NanaZip' }
if ($p) { $p | Stop-Process -Force; "已关闭 $($p.Count) 个 NanaZip 进程" } else { '无 NanaZip 进程在运行' }
Start-Sleep -Seconds 2

# ---------- 2. 增量编译（单入口 FM, Release x64, 原生 vcvars64, 全核并行）----------
"`n=== 编译 (单入口 FM, Release x64) ==="
$t = Get-Date
& cmd.exe /c "call `"$vcvars64`" >nul 2>&1 && cd /d $root && MSBuild `"$fmProj`" /p:Configuration=Release /p:Platform=x64 /m /v:minimal /nologo" | Out-Host
if ($LASTEXITCODE -ne 0) { throw "编译失败 (退出码 $LASTEXITCODE)" }
"编译成功 (耗时 $([Math]::Round(((Get-Date)-$t).TotalSeconds))s)"

# ---------- 3.5 跑 wapproj 打包（x64 单平台）刷新 resources.pri ----------
# resources.pri 是包级资源索引，只在 wapproj 打包时生成；XAML 页面改动后必须刷新
if (-not $SkipPri) {
    "`n=== wapproj 打包 (刷新 resources.pri) ==="
    $t = Get-Date
    & cmd.exe /c "call `"$vcvars64`" >nul 2>&1 && cd /d $root && MSBuild NanaZipPackage\NanaZipPackage.wapproj -t:Build -p:Configuration=Release -p:Platform=x64 -p:PreferredToolArchitecture=x64 -p:AppxBundlePlatforms=x64 -p:AppxBundle=Never /m:8 -v:m" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "wapproj 打包失败 (退出码 $LASTEXITCODE)" }
    "resources.pri 已刷新 (耗时 $([Math]::Round(((Get-Date)-$t).TotalSeconds))s)"
} else {
    '跳过 wapproj 打包（-SkipPri）'
}

# ---------- 3. 白名单替换 ----------
"`n=== 白名单替换 -> $dst ==="
$copied = @()
foreach ($name in $whitelist) {
    # 从两个候选源目录取最新的一份
    $cand = @()
    foreach ($s in @($src, $srcPkg)) {
        $f = Join-Path $s $name
        if (Test-Path -LiteralPath $f) { $cand += Get-Item -LiteralPath $f }
    }
    if ($cand.Count -eq 0) { continue }   # 源里没有，跳过

    $sf  = $cand | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    $df  = Join-Path $dst $name
    $need = $false
    if (Test-Path -LiteralPath $df) {
        if ((Get-Item -LiteralPath $df).LastWriteTime -lt $sf.LastWriteTime) { $need = $true }
    } else { $need = $true }

    if ($need) {
        $ok = $false
        for ($i = 0; $i -lt 3 -and -not $ok; $i++) {
            try { Copy-Item -LiteralPath $sf.FullName $df -Force -ErrorAction Stop; $ok = $true }
            catch { Start-Sleep -Seconds 2 }   # DLL 被占用时等释放重试
        }
        if ($ok) { $copied += "$name  ($($sf.LastWriteTime.ToString('HH:mm:ss')))" }
        else     { "失败(被占用): $name" }
    }
}
if ($copied.Count -eq 0) { '无更新文件' } else { $copied }

# ---------- 4. 布局核对（对照 pkg90）----------
if ($skipLayoutCheck) {
    "`n=== 布局核对（pkg90 缺失，跳过）==="
} else {
    "`n=== 布局核对（对照 $Pkg90\x64）==="
    $p90   = Get-ChildItem "$Pkg90\x64" -File | Select-Object -ExpandProperty Name
    $p91   = Get-ChildItem $dst -File | Select-Object -ExpandProperty Name
    $extra   = Compare-Object $p90 $p91 | Where-Object { $_.SideIndicator -eq '=>' } | Select-Object -ExpandProperty InputObject
    $missing = Compare-Object $p90 $p91 | Where-Object { $_.SideIndicator -eq '<=' } | Select-Object -ExpandProperty InputObject
    if ($extra)   { "多余: $($extra -join ', ')" }   else { '无多余文件' }
    if ($missing) {
        # 白名单内的静态文件（如 [Content_Types].xml）源目录没有，从 pkg90 补齐
        foreach ($m in $missing) {
            $s90 = "$Pkg90\x64\$m"
            if (Test-Path -LiteralPath $s90) {
                Copy-Item -LiteralPath $s90 (Join-Path $dst $m) -Force
                "已从 pkg90 补齐: $m"
            } else {
                "缺失且无源: $m"
            }
        }
    } else { '无缺失文件' }
    if (-not (Test-Path "$dst\Assets")) { '警告: Assets 目录缺失' }
}

'`n完成。可以跑了。'
