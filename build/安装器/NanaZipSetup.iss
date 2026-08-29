; ============================================================
; NanaZip 定制版 exe 安装器（Inno Setup 6）
; 用途：给不喜欢 MSIX（证书/旁加载）的人用，双击即装，无需管理员
; 安装位置：用户级 {localappdata}\Programs\NanaZip（免 UAC）
; 内容：FM 文件管理器（主入口，双击打开压缩包）+ 7zG 提取 + 7z 命令行 + 全编解码器
;       注意：NanaZip.Universal.Windows.exe 是 7zG（命令驱动，无参数显示 "Specify command"），
;       快捷方式与文件关联必须指向 NanaZip.Modern.FileManager.exe，否则用户会看到该提示
; 构建：C:\Program Files (x86)\Inno Setup 6\ISCC.exe NanaZipSetup.iss
; 产物：<仓库根>\Output\Binaries\AppPackages\NanaZipSetup_<ver>_x64.exe
; 注意：文件集来自 打包x64.ps1 产物解包出的绿色版目录，先有它再编安装器
; ============================================================
#define MyAppName "NanaZip 定制版"
#ifndef MyAppVersion
#define MyAppVersion "6.5.1825.0"
#endif
#define MyAppPublisher "SSS NanaZip Development"
#define MyAppExeName "NanaZip.Universal.Windows.exe"
; iss 位于 <仓库根>\build\安装器\，源文件集在 <仓库根>\Output\Binaries\AppPackages\NanaZip绿色版_<ver>_x64\

[Setup]
AppId={{7C3E9D4A-2B8F-4E61-9A05-1F6C4D8B3A20}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\NanaZip
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#SourcePath}..\..\Output\Binaries\AppPackages
OutputBaseFilename=NanaZipSetup_{#MyAppVersion}_x64
Compression=lzma2/max
SolidCompression=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
CloseApplications=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}";
Name: "assoc"; Description: "关联 .7z / .zip / .rar 文件到 NanaZip"; GroupDescription: "附加任务:";

[Files]
Source: "{#SourcePath}..\..\Output\Binaries\AppPackages\NanaZip绿色版_{#MyAppVersion}_x64\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs
; 文件类型图标（与 MSIX 版 fileTypeAssociations 的 ArchiveFile 图标一致，多尺寸 ico）
Source: "{#SourcePath}NanaZipArchiveFile.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\NanaZip"; Filename: "{app}\NanaZip.Modern.FileManager.exe"
Name: "{autoprograms}\NanaZip 命令行"; Filename: "{app}\NanaZip.Universal.Console.exe"
Name: "{autodesktop}\NanaZip"; Filename: "{app}\NanaZip.Modern.FileManager.exe"; Tasks: desktopicon

[Registry]
; --- 右键一级菜单（HKCU，免管理员；卸载自动清理）---
; 文件：用 NanaZip 打开 / 解压到当前文件夹
Root: HKCU; Subkey: "Software\Classes\*\shell\NanaZipOpen"; ValueType: string; ValueName: ""; ValueData: "用 NanaZip 打开"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\*\shell\NanaZipOpen"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\NanaZip.Modern.FileManager.exe,0"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\*\shell\NanaZipOpen\command"; ValueType: string; ValueName: ""; ValueData: """{app}\NanaZip.Modern.FileManager.exe"" ""%1"""; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\*\shell\NanaZipExtract"; ValueType: string; ValueName: ""; ValueData: "用 NanaZip 解压到当前文件夹"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\*\shell\NanaZipExtract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\NanaZip.Universal.Console.exe"" x -y ""%1"""; Flags: uninsdeletekey
; 文件夹：用 NanaZip 打开
Root: HKCU; Subkey: "Software\Classes\Directory\shell\NanaZipOpenDir"; ValueType: string; ValueName: ""; ValueData: "用 NanaZip 打开"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\shell\NanaZipOpenDir\command"; ValueType: string; ValueName: ""; ValueData: """{app}\NanaZip.Modern.FileManager.exe"" ""%1"""; Flags: uninsdeletekey
; --- 文件关联（HKCU，免管理员；卸载自动清理）---
Root: HKCU; Subkey: "Software\Classes\.7z";  ValueType: string; ValueName: ""; ValueData: "NanaZip.7z";  Flags: uninsdeletevalue; Tasks: assoc
Root: HKCU; Subkey: "Software\Classes\.zip"; ValueType: string; ValueName: ""; ValueData: "NanaZip.zip"; Flags: uninsdeletevalue; Tasks: assoc
Root: HKCU; Subkey: "Software\Classes\.rar"; ValueType: string; ValueName: ""; ValueData: "NanaZip.rar"; Flags: uninsdeletevalue; Tasks: assoc
Root: HKCU; Subkey: "Software\Classes\NanaZip.7z\DefaultIcon";  ValueType: string; ValueName: ""; ValueData: "{app}\NanaZipArchiveFile.ico"; Flags: uninsdeletekey; Tasks: assoc
Root: HKCU; Subkey: "Software\Classes\NanaZip.7z\shell\open\command";  ValueType: string; ValueName: ""; ValueData: """{app}\NanaZip.Modern.FileManager.exe"" ""%1"""; Flags: uninsdeletekey; Tasks: assoc
Root: HKCU; Subkey: "Software\Classes\NanaZip.zip\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\NanaZipArchiveFile.ico"; Flags: uninsdeletekey; Tasks: assoc
Root: HKCU; Subkey: "Software\Classes\NanaZip.zip\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\NanaZip.Modern.FileManager.exe"" ""%1"""; Flags: uninsdeletekey; Tasks: assoc
Root: HKCU; Subkey: "Software\Classes\NanaZip.rar\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\NanaZipArchiveFile.ico"; Flags: uninsdeletekey; Tasks: assoc
Root: HKCU; Subkey: "Software\Classes\NanaZip.rar\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\NanaZip.Modern.FileManager.exe"" ""%1"""; Flags: uninsdeletekey; Tasks: assoc

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "运行 NanaZip"; Flags: nowait postinstall skipifsilent

[Code]
// 注册/卸载右键菜单扩展（MSIX 壳包 + ExternalLocation，Win11 默认菜单直接显示）
// 用事件钩子而非 [Run]，因为静默/升级模式不执行 [Run] 段
procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssPostInstall then
  begin
    Exec(ExpandConstant('{app}\RegisterShellExt.cmd'), '', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    // 勾选 assoc 任务时：注册 Capabilities 并用官方 API 写 UserChoice（.zip/.7z/.rar 双击立即生效）
    if WizardIsTaskSelected('assoc') then
      Exec('powershell.exe', '-NoProfile -ExecutionPolicy Bypass -File "' + ExpandConstant('{app}') + '\SetFileAssoc.ps1"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  end;
end;

procedure CurUninstallStepChanged(CurStep: TUninstallStep);
var
  ResultCode: Integer;
begin
  if CurStep = usUninstall then
  begin
    Exec('powershell.exe', '-NoProfile -ExecutionPolicy Bypass -Command "Get-AppxPackage -Name ''SSS.NanaZip.ShellExtension'' -ErrorAction SilentlyContinue | Remove-AppxPackage"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    // 清 UserChoice（悬空指向已卸载的 ProgID 会弹“打开方式”）+ Capabilities 注册
    Exec('powershell.exe', '-NoProfile -Command "''.zip'',''.7z'',''.rar'' | ForEach-Object { Remove-Item -LiteralPath (''HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\'' + $_ + ''\\UserChoice'') -Recurse -Force -ErrorAction SilentlyContinue }"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Exec('powershell.exe', '-NoProfile -Command "Remove-Item -LiteralPath ''HKCU:\\Software\\SSS NanaZip Development'' -Recurse -Force -ErrorAction SilentlyContinue"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  end;
end;
