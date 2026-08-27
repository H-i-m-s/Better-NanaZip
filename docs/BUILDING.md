# 从源码构建 NanaZip

NanaZip 使用标准的 Visual Studio 工具链构建，不需要任何自定义脚本或签名
配置。一个全新的 fork 即可开箱即用，本地和 GitHub Actions 上都可以构建。

## 环境要求

- **Windows 10 2004（Build 19041）或更高版本**（开发机；构建出的安装包
  可运行于 Windows 10 2004+ 与 Windows Server 2022+）。
- **Visual Studio 2022**，安装 **使用 C++ 的桌面开发** 工作负载
  （MSVC 工具集、Windows SDK、C++ CMake 工具）。
  - 建议安装较新的 **Windows 10/11 SDK**（10.0.26100.0 或更高）。
- **Git**。
- **.NET SDK 8 或更高**：仅当需要重新构建 `build\NanaZip.MaintainerTools.slnx`
  中的维护工具时才需要，常规构建不依赖。

MSBuild 项目 SDK `Mile.Project.Configurations` 的版本已在 `global.json`
中锁定（1.0.2075），构建时自动从 NuGet 还原。

## 本地构建

```bat
git clone --recursive https://github.com/<你的账号>/NanaZip.git
cd NanaZip

rem 可选：预先还原 NuGet 包（使用 Visual Studio 2026 时推荐，
rem 该版本可能不会自动还原包）。
build\RestoreNuGetPackages.cmd

rem 完整构建：x64 + ARM64，Debug + Release，MSIX 安装包、
rem 扩展包安装器、便携版二进制与调试符号。
build\BuildAllTargets.cmd
```

`build\BuildAllTargets.cmd` 内部执行 `MSBuild -m build\BuildAllTargets.proj`。也可以在
**开发人员命令提示符**中直接调用 MSBuild：

```bat
MSBuild -m build\BuildAllTargets.proj
```

默认目标链为 `RefreshVersion → Restore → Build → Packaging`：

- `RefreshVersion`：根据 git 提交日期推导包版本号。
- `Restore`：为两种配置还原 NuGet 包。
- `Build`：构建 MSIX 包项目（x64 + ARM64，Debug + Release）与扩展包安装器。
- `Packaging`：组装便携版二进制与调试符号压缩包。

## 输出位置

| 路径 | 内容 |
| --- | --- |
| `Output\Binaries\AppPackages` | 可安装的 MSIX 安装包 |
| `Output\Binaries\Root\Binaries` | 便携版二进制（x64 + arm64） |
| `Output\Binaries\Root\Symbols` | PDB 调试符号 |
| `Output\NanaZip.ExtensionPackage_*.exe` | 扩展包安装器（Inno Setup） |
| `Output\NanaZip_*_Binaries.zip` | 便携版发布压缩包 |
| `Output\NanaZip_*_DebugSymbols.zip` | 调试符号压缩包 |

## 在 GitHub Actions 上构建

[构建二进制](../../.github/workflows/BuildBinaries.yml) 工作流是标准的 CI 构建，
运行于 `windows-2022`，执行 `MSBuild build\BuildAllTargets.proj`，并将 MSIX 安装包、
扩展包安装器、便携版二进制、调试符号与 MSBuild 二进制日志上传为 artifacts。

构建你的 fork：

1. Fork 本仓库。
2. 打开 **Actions** 页面 → **Build Binaries**。
3. 点击 **Run workflow**。

无需任何仓库 secrets。该工作流为手动触发（`workflow_dispatch`），
不会因 push 自动运行。

## 常见问题

- **全新机器上 NuGet 还原失败**：先执行一次
  `dotnet nuget locals all --clear` 再重新构建（工作流已内置此步骤）。
- **Visual Studio 不自动还原包**：构建前先运行
  `build\RestoreNuGetPackages.cmd`。
- **`Mile.Project.Configurations` 解析错误**：确认 `global.json` 完好，
  首次构建需要联网（SDK 从 nuget.org 下载一次后本地缓存）。
- **缺少 MSIX 工具**：通过 Visual Studio Installer 安装 Windows SDK 的
  *应用打包工具* 组件。

## 修改与调试

在 Visual Studio 中打开 `build\NanaZip.slnx`，构建你需要的项目
（XAML 对话框改 `NanaZip.Modern`，文件管理器与 Shell 扩展改
`NanaZip.UI.Modern`，MSIX 包改 `NanaZipPackage`）。
代码风格与协作流程见 [CONTRIBUTING.md](CONTRIBUTING.md)。
