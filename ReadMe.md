![NanaZip hero](assets/readme/hero.svg)

NanaZip 是一款面向现代 Windows 体验的开源压缩工具，从 [7-Zip] 源码 fork，
围绕 Windows 现代设计语言重建：原生深色模式、Mica 材质、深度的资源管理器
集成与 MSIX 打包。完整继承 7-Zip 26.02 的全部能力，并扩展了额外格式、
哈希算法与安全加固。

本仓库是 **NanaZip 6.5 的深度定制版**，在官方版本之上进行了大量界面现代化
改造与功能增强，包身份为 `SSS.NanaZip.RemotePassword`（自签名发布）。

[![License](https://img.shields.io/badge/许可证-MIT-58A6FF)](License.md)
[![Platform](https://img.shields.io/badge/平台-Windows%2010%202004%2B-58A6FF)](docs/BUILDING.md)
[![Core](https://img.shields.io/badge/内核-7--Zip%2026.02-3FB950)](https://www.7-zip.org/)
[![Base](https://img.shields.io/badge/基线-官方%206.5.1800.0-58A6FF)]()

## 定制版特色

### 密码解决方案

这是本定制版的核心增强，围绕压缩包密码处理提供一整套工具：

- **本地密码本**：维护本地密码库，解压时自动匹配密码本中的密码，无需逐个
  手工尝试。支持向密码本添加密码、处理空行与换行等细节。
- **云端密码查询**：解压遇到本地密码本没有的密码时，可向云端查询解压码。
  > 侵权风险提示：云端查询功能基于对第三方解压软件通信协议的研究，存在
  > 侵权风险。如何获取云端 API 由用户自行研究，本项目不提供任何指导，也不
  > 对使用该功能产生的法律后果负责。建议仅用于合法用途。
- **混合匹配模式**：本地密码本与云端查询可组合使用，按优先级自动匹配。
- **自动匹配密码**：解压流程全自动匹配，匹配成功后直接解压，减少人工干预。
- **分享密码**：支持将匹配到的密码生成分享内容，方便共享给协作者。
- **修复解压黑框问题**：密码匹配阶段不再弹出多余的控制台窗口。

### 现代化界面（Win32 → XAML 迁移）

将官方版本中遗留的 Win32 对话框逐步迁移到 XAML，风格统一、支持深色模式：

- **压缩 / 解压界面**：全新 XAML 界面，支持自适应布局、右栏对齐、横向展开、
  拖动丝滑、Esc 退出、下拉框显示等细节优化。
- **设置界面**：整体重构为 XAML，含参数选项页（可选参数不崩溃）、确定 /
  取消 / 应用按钮行为修正、设置卡片界面。
- **文件选择器**：重写为现代化选择器，完美适配深色主题。
- **文件替换 / 覆盖对话框**：XAML 迁移（最终采用 Win32 方案并打磨至满意）。
- **分割文件对话框**、**密码对话框**、**新建文件夹对话框**、**基准测试页**：
  全部现代化。
- **右键菜单栏**：菜单默认向下展开、向上可变位置弹出、显示快捷键、顶栏
  右键菜单支持、批量操作入口整合。
- **信息 / 属性对话框**：信息高度自适应、文件类型等字段对齐修正、界面
  右侧空白修复、每显示器 DPI 修正（属性对话框按目标显示器实际缩放值计算
  尺寸，消除高 DPI 下的错位与空白）。

### 交互与体验优化

- **批量解压**：批量 / 单个循环解压，修复批量提取界面重复弹出、参数传递、
  汇总框缺失、本地密码批量匹配等问题。
- **批量删除**：修复批量删除与删除压缩包后的清理逻辑。
- **自适应列宽**：文件列表列宽自适应内容，文件大小简化显示（B/KB/MB/GB）。
- **文件输入框下拉菜单**：输入框支持下拉历史，文本框默认选中，输入更顺手。
- **蓝色选中态**：文件选中样式修正，点击应用后选中态保持，选项卡不再回缩。

## 截图

![NanaZip 主窗口，深色模式](Documents/MainWindowDarkMode.png)
![NanaZip 主窗口，浅色模式](Documents/MainWindowLightMode.png)
![NanaZip 右键菜单](Documents/ContextMenu.png)

## 安装

本仓库的发布包使用自签名证书（`CN=SSS NanaZip Development`）签名。从
Releases 下载 MSIX 安装包后：

```powershell
Add-AppxPackage -Path "NanaZipPackage_6.5.x.x_x64_arm64.msixbundle"
```

如提示证书不受信任，先导入公钥再安装：

```powershell
.\build\SSSDevSigning\Install-SSSDevCertificate.ps1
.\build\SSSDevSigning\Install-SSSNanaZipPackage.ps1 -PackagePath "NanaZipPackage_6.5.x.x_x64_arm64.msixbundle"
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
工作流在 GitHub Actions 上构建同样的目标，无需签名、无需 secrets，fork 后
即可开箱即用（Actions 页面手动触发）。

本机签名与安装辅助脚本见 [build/SSSDevSigning/](build/SSSDevSigning/README.md)。

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
