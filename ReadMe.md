![NanaZip hero](assets/readme/hero.svg)

NanaZip 是一款面向现代 Windows 体验的开源压缩工具，从 [7-Zip] 源码 fork，
围绕 Windows 现代设计语言重建：原生深色模式、Mica 材质、深度的资源管理器
集成与 MSIX 打包。完整继承 7-Zip 26.02 的全部能力，并扩展了额外格式、
哈希算法与安全加固。

[![License](https://img.shields.io/badge/许可证-MIT-58A6FF)](License.md)
[![Platform](https://img.shields.io/badge/平台-Windows%2010%202004%2B-58A6FF)](docs/BUILDING.md)
[![Core](https://img.shields.io/badge/内核-7--Zip%2026.02-3FB950)](https://www.7-zip.org/)

## 核心特性

- **7-Zip 26.02 内核**，集成 [7-Zip ZS] 与 [7-Zip NSIS] 的增强（Brotli、LZ4、
  LZ5、Lizard、Zstandard、Fast-LZMA2、NSIS 脚本支持）。
- **所有 GUI 组件原生深色模式**，主窗口沉浸式 Mica 效果。
- **资源管理器深度集成**：Windows 10/11 右键菜单与文件关联。
- **MSIX 打包**，依赖内置，部署干净，卸载不留残留。
- **7-Zip 执行别名**（`7z.exe` 指向 NanaZip），从 7-Zip 迁移无痛。
- **每显示器 DPI 感知**，现代化消息框与文件夹选择器，智能解压，
  默认传递 Mark-of-the-Web（Zone.Identifier）。
- **额外只读格式**：.NET 单文件应用、Electron asar、ROMFS、UFS/UFS2、
  ZealFS、WebAssembly、littlefs。
- **30 余种哈希算法**：MD2-MD5、SHA 系列、SHA-3、BLAKE2b/3、ED2K、GOST、
  Snefru、Tiger、TTH、Whirlpool、XXH32/64/3、SM3。
- **安全加固**：Control Flow Guard、CET 影子栈、包完整性校验、
  严格句柄校验、Release 构建禁用动态代码生成。

## 截图

![NanaZip 主窗口，深色模式](Documents/MainWindowDarkMode.png)
![NanaZip 主窗口，浅色模式](Documents/MainWindowLightMode.png)
![NanaZip 右键菜单](Documents/ContextMenu.png)

## 安装

从 [Releases](https://github.com/M2Team/NanaZip/releases) 页面下载最新的
**MSIX 安装包**，双击即可安装；或在 PowerShell 中执行：

```powershell
Add-AppxPackage -Path "NanaZip_x64.msixbundle 的路径"
```

安装后如右键菜单未出现，请在任务管理器中重启资源管理器（explorer.exe）。

## 从源码构建

完整指南见 [docs/BUILDING.md](docs/BUILDING.md)。快速开始：

```bat
git clone --recursive https://github.com/你的账号/NanaZip.git
cd NanaZip
build\BuildAllTargets.cmd
```

产物输出到 `Output\Binaries`。[构建二进制](.github/workflows/BuildBinaries.yml)
工作流在 GitHub Actions 上构建同样的目标，无需签名、无需 secrets，
fork 后即可开箱即用（Actions 页面手动触发）。

## 文档

- [从源码构建](docs/BUILDING.md)
- [发布说明](Documents/ReleaseNotes.md)
- [贡献指南](docs/CONTRIBUTING.md)
- [许可证](License.md)
- [安全政策](docs/Security.md)
- [隐私政策](Documents/Privacy.md)
- [组策略管理模板 (ADMX/ADML)](Documents/PolicyDefinitions)
- [Section 508 无障碍符合性报告](Documents/Section508)

## 许可证

NanaZip 以 [MIT 许可证](License.md) 发布。

NanaZip 是 [7-Zip]（作者 Igor Pavlov）的 fork，并基于 [7-Zip ZS] 与
[7-Zip NSIS] 项目的成果。相关上游代码适用各自的许可证。

[7-Zip]: https://www.7-zip.org/
[7-Zip ZS]: https://github.com/mcmilk/7-Zip-zstd
[7-Zip NSIS]: https://github.com/myfreeer/7z-build-nsis
