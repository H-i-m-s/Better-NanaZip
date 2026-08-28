# ============================================================
# 04-编译exe.ps1 —— ISCC 编译 Inno 安装器
# 位置：build\步骤\ 目录
# 参数：
#   -Version     版本号（必填，覆盖 iss 的 MyAppVersion）
#   -FilesDir    文件集目录（绿色版目录，iss 从其中取源文件）
# 输出：setup.exe 路径（写 $Output 变量 / 打印）
# ============================================================
param(
    [string]$Version = '',
    [string]$FilesDir = ''
)
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $Version) { throw '-Version 必填' }
$iss = Join-Path (Split-Path -Parent $PSScriptRoot) '安装器\NanaZipSetup.iss'
if (-not (Test-Path -LiteralPath $iss)) { throw "iss 不存在: $iss" }

$iscc = Get-ChildItem 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe' -ErrorAction SilentlyContinue
if (-not $iscc) { $iscc = Get-ChildItem 'C:\Program Files\Inno Setup 6\ISCC.exe' -ErrorAction SilentlyContinue }
if (-not $iscc) { throw '未找到 Inno Setup 6 (ISCC.exe)，请先安装' }

# FilesDir 校验（iss 的 [Files] 从它取源）
if ($FilesDir -and -not (Test-Path -LiteralPath $FilesDir)) { throw "文件集目录不存在: $FilesDir" }
"文件集目录: $FilesDir"

"`n=== ISCC 编译 (版本 $Version) ==="
$t = Get-Date
& $iscc.FullName "/DMyAppVersion=$Version" $iss 2>&1 | Select-String -Pattern 'Successful|Error|Compiling' | ForEach-Object { $_.Line }
if ($LASTEXITCODE -ne 0) { throw "ISCC 编译失败 (退出码 $LASTEXITCODE)" }
"编译完成 (耗时 $([Math]::Round(((Get-Date)-$t).TotalSeconds))s)"

$setup = "$root\Output\Binaries\AppPackages\NanaZipSetup_${Version}_x64.exe"
if (-not (Test-Path -LiteralPath $setup)) { throw "setup.exe 未生成: $setup" }
Write-Output "SETUP=$setup"
