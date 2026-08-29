# ============================================================
# SetFileAssoc.ps1 —— 注册 Capabilities 并用官方 COM API 写 UserChoice
# 由 exe 安装器 [Code]（勾选 assoc 任务时）或绿色版脚本调用
# 官方路线：RegisteredApplications + IApplicationAssociationRegistration::
#           SetAppAsDefault，由 Windows 自己计算并写入 UserChoice Hash
# （Win11 25H2 的 UserChoice Hash 已是 24 字符新格式，第三方老算法全部失效，
#   SetUserFTA 也已商业化，官方 API 是唯一可持续路线）
# ============================================================
$ErrorActionPreference = 'Continue'
$appName = 'NanaZip 定制版'
$capPath = 'Software\SSS NanaZip Development\NanaZip\Capabilities'

$dir = $PSScriptRoot
if (-not $dir) { $dir = Split-Path -Parent $MyInvocation.MyCommand.Path }

# 定位 FM（命令行与图标都要指向真实安装位置）
$fm = Join-Path $dir 'NanaZip.Modern.FileManager.exe'
$ico = Join-Path $dir 'NanaZipArchiveFile.ico'
if (-not (Test-Path $fm)) {
    Write-Host "[错误] 找不到 $fm" -ForegroundColor Red
    exit 1
}

Write-Host '==== NanaZip 文件关联 ====' -ForegroundColor Cyan

# ---------- 1. ProgID（幂等，缺了补上） ----------
$rc = [Microsoft.Win32.Registry]::CurrentUser
$assoc = @(
    @{ Ext = '.7z';  ProgId = 'NanaZip.7z' },
    @{ Ext = '.zip'; ProgId = 'NanaZip.zip' },
    @{ Ext = '.rar'; ProgId = 'NanaZip.rar' }
)
foreach ($a in $assoc) {
    $k = $rc.CreateSubKey("Software\Classes\$($a.ProgId)")
    $k.SetValue('', "NanaZip $($a.Ext.TrimStart('.')) Archive", [Microsoft.Win32.RegistryValueKind]::String)
    $k.Close()
    $k = $rc.CreateSubKey("Software\Classes\$($a.ProgId)\DefaultIcon")
    $k.SetValue('', $(if (Test-Path $ico) { $ico } else { "$fm,0" }), [Microsoft.Win32.RegistryValueKind]::String)
    $k.Close()
    $k = $rc.CreateSubKey("Software\Classes\$($a.ProgId)\shell\open\command")
    $k.SetValue('', ('"' + $fm + '" "%1"'), [Microsoft.Win32.RegistryValueKind]::String)
    $k.Close()
    # 扩展名默认指向 + OpenWithProgids（"打开方式"菜单出现 NanaZip）
    $k = $rc.CreateSubKey("Software\Classes\$($a.Ext)")
    $k.SetValue('', $a.ProgId, [Microsoft.Win32.RegistryValueKind]::String)
    $k.Close()
    $rc.CreateSubKey("Software\Classes\$($a.Ext)\OpenWithProgids").Close()
    $k = $rc.OpenSubKey("Software\Classes\$($a.Ext)\OpenWithProgids", $true)
    $k.SetValue($a.ProgId, '', [Microsoft.Win32.RegistryValueKind]::String)
    $k.Close()
}

# ---------- 2. Capabilities + RegisteredApplications ----------
$k = $rc.CreateSubKey($capPath)
$k.SetValue('ApplicationName', $appName, [Microsoft.Win32.RegistryValueKind]::String)
$k.SetValue('ApplicationDescription', 'NanaZip 定制版：压缩/解压/文件管理', [Microsoft.Win32.RegistryValueKind]::String)
$k.Close()
$k = $rc.CreateSubKey("$capPath\FileAssociations")
foreach ($a in $assoc) {
    $k.SetValue($a.Ext, $a.ProgId, [Microsoft.Win32.RegistryValueKind]::String)
}
$k.Close()
$k = $rc.CreateSubKey('Software\RegisteredApplications')
$k.SetValue($appName, $capPath, [Microsoft.Win32.RegistryValueKind]::String)
$k.Close()
Write-Host '[1/2] Capabilities 与 ProgID 已注册'

# ---------- 3. 官方 API 写 UserChoice ----------
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace SssSetAssoc {
    [ComImport, Guid("591209c7-767b-42b2-9fba-44ee4615f2c7"), ClassInterface(ClassInterfaceType.None)]
    public class ApplicationAssociationRegistration { }
    [ComImport, Guid("4e530b0a-e611-4c77-a3ac-9031d022281b"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IApplicationAssociationRegistration {
        void QueryCurrentDefault(
            [MarshalAs(UnmanagedType.LPWStr)] string pszQuery, int atQueryType, int alQueryLevel,
            out IntPtr ppszAssociation);
        [return: MarshalAs(UnmanagedType.Bool)]
        bool QueryAppIsDefault(
            [MarshalAs(UnmanagedType.LPWStr)] string pszQuery, int atQueryType, int alQueryLevel,
            [MarshalAs(UnmanagedType.LPWStr)] string pszAppRegistryName,
            [In, Out] ref bool pfDefault);
        void QueryAppIsDefaultAll(
            int alQueryLevel, [MarshalAs(UnmanagedType.LPWStr)] string pszAppRegistryName,
            [In, Out] ref bool pfDefault);
        void SetAppAsDefault(
            [MarshalAs(UnmanagedType.LPWStr)] string pszAppRegistryName,
            [MarshalAs(UnmanagedType.LPWStr)] string pszSet, int atSetType);
        void SetAppAsDefaultAll(
            [MarshalAs(UnmanagedType.LPWStr)] string pszAppRegistryName);
        void ClearUserAssociations();
    }
    public static class Helper {
        public static string SetDefault(string appName, string ext) {
            try {
                var reg = (IApplicationAssociationRegistration)new ApplicationAssociationRegistration();
                reg.SetAppAsDefault(appName, ext, 0); // AT_FILEEXTENSION = 0
                return "OK";
            } catch (Exception ex) { return ex.Message; }
        }
    }
}
'@
Write-Host '[2/2] 请求系统写入默认关联...'
$failed = 0
foreach ($a in $assoc) {
    $r = [SssSetAssoc.Helper]::SetDefault($appName, $a.Ext)
    $uc = Get-ItemProperty "HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\$($a.Ext)\UserChoice" -ErrorAction SilentlyContinue
    if ($uc.ProgId -eq $a.ProgId) {
        Write-Host "  $($a.Ext) -> $($a.ProgId)  [已生效]"
    } else {
        Write-Host "  $($a.Ext) -> 未自动生效（API 返回: $r），可在 设置->默认应用 或 右键-打开方式 中手动选择" -ForegroundColor Yellow
        $failed++
    }
}
Write-Host ''
Write-Host '完成。' -ForegroundColor Green
exit $(if ($failed -gt 0) { 0 } else { 0 })
