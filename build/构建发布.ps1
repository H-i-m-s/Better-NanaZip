# ============================================================
# 构建发布.ps1 —— 本地构建总入口（总文件 + build\步骤\ 子文件）
# 位置：仓库 build\ 目录（随仓库走，克隆后直接用）
#
# 用法：
#   ./build/构建发布.ps1 -Mode 替换                          # 编译+替换 pkg91（日常开发）
#   ./build/构建发布.ps1 -Mode 构建 -Format msix -Version 6.5.1826.0   # 增量编译+msix+签名
#   ./build/构建发布.ps1 -Mode 构建 -Format exe -Version 6.5.1826.0    # 增量编译+exe 安装器
#   ./build/构建发布.ps1 -Mode 打包 -Format exe               # 不编译，用现有产物直接打包
#   ./build/构建发布.ps1 -Mode 全量 -Version 6.5.1826.0 -Arch both     # 清缓存全量+全格式
#
# 参数：
#   -Mode    替换 | 构建 | 打包 | 全量
#   -Format  msix | exe | green | all    （替换模式忽略）
#   -Arch    x64 | both                  （both = x64+arm64 bundle）
#   -Version 版本号，如 6.5.1826.0        （自动写入 manifest）
#   -Sign    签名开关（默认开，-Sign:$false 关）
#   -SkipPri 跳过 resources.pri（纯 .cpp 改动时用）
#   -Force   全量模式跳过确认
#
# 产物统一在 Output\发布\<版本>\（全量模式只清缓存，不会动发布目录）
# ============================================================
param(
    [ValidateSet('替换', '构建', '打包', '全量')][string]$Mode = '构建',
    [ValidateSet('msix', 'exe', 'green', 'all')][string]$Format = 'all',
    [ValidateSet('x64', 'both')][string]$Arch = 'x64',
    [string]$Version = '',
    [switch]$Sign = $true,
    [switch]$SkipPri,
    [switch]$Force
)
$ErrorActionPreference = 'Stop'

$root  = Split-Path -Parent $PSScriptRoot
$steps = Join-Path $PSScriptRoot '步骤'

function Invoke-Step([string]$StepName, [string[]]$StepParams) {
    "`n########## [$StepName] ##########"
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $steps $StepName) @StepParams
    if ($LASTEXITCODE -ne 0) { throw "步骤 $StepName 失败 (退出码 $LASTEXITCODE)" }
}

function Resolve-LatestBundle {
    Get-ChildItem "$root\Output\Binaries\AppPackages" -Directory -Filter 'NanaZipPackage_*_Test' |
        Where-Object { $_.Name -notmatch 'Debug' } |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
}

function VersionFrom-Bundle([string]$BundlePath) {
    [regex]::Match([System.IO.Path]::GetFileName($BundlePath), '_(\d+\.\d+\.\d+\.\d+)_').Groups[1].Value
}

"`n=== 构建发布 ($Mode / $Format / $Arch / v$Version) ==="

# ---------------- 替换模式 ----------------
if ($Mode -eq '替换') {
    $stepArgs = @('-Mode', '替换')
    if ($SkipPri) { $stepArgs += '-SkipPri' }
    Invoke-Step '01-编译.ps1' $stepArgs
    return
}

# ---------------- 构建 / 全量：先编译（打包模式不编译）----------------
if ($Mode -in @('构建', '全量')) {
    if ($Mode -eq '构建') {
        $compileMode = '增量'
    } else {  # 全量
        if (-not $Force) {
            $ans = Read-Host "全量模式将清理 Output\Objects + Output\Binaries（发布目录保留），确认? [y/N]"
            if ($ans -notmatch '^[yY]') { '已取消'; return }
        }
        $compileMode = '全量'
    }
    Invoke-Step '01-编译.ps1' @('-Mode', $compileMode)
}

# ---------------- 构建/全量：打 msix 包 ----------------
$bundlePath = $null
if ($Format -in @('msix', 'all')) {
    $stepArgs = @('-Arch', $Arch)
    if ($Version) { $stepArgs += @('-Version', $Version) }
    if (-not $Sign) { $stepArgs += @('-Sign:$false') }
    $out = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $steps '02-打包msix.ps1') @stepArgs
    $bundlePath = ($out | Select-String '^BUNDLE=' | Select-Object -Last 1).ToString().Replace('BUNDLE=', '').Trim()
    if (-not $bundlePath) { throw '02 未输出 BUNDLE 路径' }
    if (-not $Version) { $Version = VersionFrom-Bundle $bundlePath }
} else {
    # 不打 msix 也要版本号（exe/green 需要）
    if (-not $Version) { $Version = VersionFrom-Bundle (Resolve-LatestBundle).FullName }
}

# ---------------- 打包 / 构建 / 全量 的 exe/green ----------------
if ($Mode -eq '打包') {
    $dir = Resolve-LatestBundle
    if (-not $dir) { throw '未找到现有 bundle，无法打包' }
    $bundlePath = Get-ChildItem $dir.FullName -File -Filter '*.msixbundle' | Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
    if (-not $Version) { $Version = VersionFrom-Bundle $bundlePath }
    "打包模式：使用现有产物 $bundlePath (v$Version)"
}

$needExe  = $Format -in @('exe', 'all')
$needGreen = $Format -in @('green', 'all')

if ($needExe -or $needGreen) {
    # 03 组装文件集（exe 需要文件集；green 直接打 zip）
    $out = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $steps '03-组装文件集.ps1') -BundlePath $bundlePath -Version $Version
    $greenDir = ($out | Select-String '^GREEN=' | Select-Object -Last 1).ToString().Replace('GREEN=', '').Trim()
    if (-not $greenDir) { throw '03 未输出 GREEN 路径' }

    if ($needExe) {
        $out = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $steps '04-编译exe.ps1') -Version $Version -FilesDir $greenDir
        $setupPath = ($out | Select-String '^SETUP=' | Select-Object -Last 1).ToString().Replace('SETUP=', '').Trim()
    }
}

# ---------------- 汇总发布 ----------------
$sumArgs = @('-Version', $Version)
if ($bundlePath) { $sumArgs += @('-BundlePath', $bundlePath) }
if ($setupPath)  { $sumArgs += @('-SetupExe', $setupPath) }
if ($greenDir)   { $sumArgs += @('-GreenDir', $greenDir) }
Invoke-Step '05-汇总发布.ps1' $sumArgs
