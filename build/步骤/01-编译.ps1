# ============================================================
# 01-编译.ps1 —— 编译步骤（替换 / 增量 / 全量）
# 位置：build\步骤\ 目录
# 用法：
#   替换：编译 FM + 白名单替换到 .local\pkg91\x64（原更新pkg91.ps1）
#   增量：编译 FM 单入口（依赖链自动带出），供后续 wapproj 打包
#   全量：清构建缓存（Objects+Binaries，保留 Output\发布）后全链重编
# 参数：
#   -Mode      替换|增量|全量 （默认 增量）
#   -SkipPri   跳过 wapproj 刷新 resources.pri（纯 .cpp 改动时用）
#   -Pkg91/-Pkg90  替换模式的目标/对照解包目录（默认 .local\pkg91 / pkg90）
# ============================================================
param(
    [ValidateSet('替换', '增量', '全量')][string]$Mode = '增量',
    [switch]$SkipPri,
    [string]$Pkg91 = '',
    [string]$Pkg90 = ''
)
$ErrorActionPreference = 'Stop'

$root     = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)  # 步骤\ → build → 仓库根
$vcvars64 = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
$fmProj   = 'NanaZip.UI.Modern\NanaZip.Modern.FileManager.vcxproj'
$wapproj  = 'NanaZipPackage\NanaZipPackage.wapproj'

# ---------- 0. 停掉残留进程（解锁 DLL / 保证文件不被占用）----------
$p = Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^NanaZip' }
if ($p) { $p | Stop-Process -Force; "已关闭 $($p.Count) 个 NanaZip 进程" } else { '无 NanaZip 进程在运行' }
Start-Sleep -Seconds 2

# ---------- 1. 编译 ----------
if ($Mode -eq '全量') {
    "`n=== 清理构建缓存 (Objects + Binaries，保留 Output\发布) ==="
    foreach ($d in @("$root\Output\Objects", "$root\Output\Binaries")) {
        if (Test-Path $d) { Remove-Item $d -Recurse -Force; "已清: $d" }
    }
}
"`n=== 编译 ($Mode, FM 单入口, Release x64) ==="
$t = Get-Date
& cmd.exe /c "call `"$vcvars64`" >nul 2>&1 && cd /d $root && MSBuild `"$fmProj`" /p:Configuration=Release /p:Platform=x64 /m /v:minimal /nologo" | Out-Host
if ($LASTEXITCODE -ne 0) { throw "编译失败 (退出码 $LASTEXITCODE)" }
"编译完成 (耗时 $([Math]::Round(((Get-Date)-$t).TotalSeconds))s)"

# ---------- 2. resources.pri（替换模式需要刷新；增量模式留给 wapproj 打包）----------
if ($Mode -eq '替换' -and -not $SkipPri) {
    "`n=== wapproj 打包 (刷新 resources.pri) ==="
    $t = Get-Date
    & cmd.exe /c "call `"$vcvars64`" >nul 2>&1 && cd /d $root && MSBuild `"$wapproj`" /t:Build /p:Configuration=Release /p:Platform=x64 /p:PreferredToolArchitecture=x64 /p:AppxBundlePlatforms=x64 /p:AppxBundle=Never /m:8 /v:m" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "wapproj 打包失败 (退出码 $LASTEXITCODE)" }
    "resources.pri 已刷新 (耗时 $([Math]::Round(((Get-Date)-$t).TotalSeconds))s)"
} elseif ($Mode -eq '替换') {
    '跳过 wapproj 打包（-SkipPri）'
}

# ---------- 3. 替换模式：白名单替换 + 布局核对（原更新pkg91.ps1 逻辑）----------
if ($Mode -ne '替换') { '编译完成（增量/全量模式不替换 pkg91）'; return }

if ($Pkg91 -eq '') { $Pkg91 = "$root\.local\pkg91" }
if ($Pkg90 -eq '') { $Pkg90 = "$root\.local\pkg90" }
$src    = "$root\Output\Binaries\Release\x64"
$srcPkg = "$root\Output\Binaries\Release\NanaZipPackage\x64"
$dst    = "$Pkg91\x64"

if (-not (Test-Path -LiteralPath $dst)) { New-Item -ItemType Directory -Path $dst -Force | Out-Null }
$skipLayoutCheck = -not (Test-Path -LiteralPath "$Pkg90\x64")
if ($skipLayoutCheck) { Write-Warning "对照基准目录不存在: $Pkg90\x64，本轮跳过布局核对" }

$whitelist = @(
    'AppxBlockMap.xml', 'AppxManifest.xml', '[Content_Types].xml',
    'K7Base.dll', 'K7User.dll', 'Mile.Xaml.Styles.SunValley.xbf',
    'NanaZip.Codecs.dll', 'NanaZip.Core.Console.sfx', 'NanaZip.Core.dll',
    'NanaZip.Core.Windows.sfx', 'NanaZip.Modern.dll', 'NanaZip.Modern.FileManager.exe',
    'NanaZip.Modern.pri', 'NanaZip.Modern.winmd', 'NanaZip.ShellExtension.dll',
    'NanaZip.Universal.Console.exe', 'NanaZip.Universal.Windows.exe', 'resources.pri'
)

"`n=== 白名单替换 -> $dst ==="
$copied = @()
foreach ($name in $whitelist) {
    $cand = @()
    foreach ($s in @($src, $srcPkg)) {
        $f = Join-Path $s $name
        if (Test-Path -LiteralPath $f) { $cand += Get-Item -LiteralPath $f }
    }
    if ($cand.Count -eq 0) { continue }
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
            catch { Start-Sleep -Seconds 2 }
        }
        if ($ok) { $copied += "$name  ($($sf.LastWriteTime.ToString('HH:mm:ss')))" }
        else     { "失败(被占用): $name" }
    }
}
if ($copied.Count -eq 0) { '无更新文件' } else { $copied }

if ($skipLayoutCheck) {
    "`n=== 布局核对（pkg90 缺失，跳过）==="
} else {
    "`n=== 布局核对（对照 $Pkg90\x64）==="
    $p90   = Get-ChildItem "$Pkg90\x64" -File | Select-Object -ExpandProperty Name
    $p91   = Get-ChildItem $dst -File | Select-Object -ExpandProperty Name
    $extra   = Compare-Object $p90 $p91 | Where-Object { $_.SideIndicator -eq '=>' } | Select-Object -ExpandProperty InputObject
    $missing = Compare-Object $p90 $p91 | Where-Object { $_.SideIndicator -eq '<=' } | Select-Object -ExpandProperty InputObject
    if ($extra)   { "多余: $($extra -join ', ')" } else { '无多余文件' }
    if ($missing) {
        foreach ($m in $missing) {
            $s90 = "$Pkg90\x64\$m"
            if (Test-Path -LiteralPath $s90) { Copy-Item -LiteralPath $s90 (Join-Path $dst $m) -Force; "已从 pkg90 补齐: $m" }
            else { "缺失且无源: $m" }
        }
    } else { '无缺失文件' }
}
'替换完成。可以跑了。'
