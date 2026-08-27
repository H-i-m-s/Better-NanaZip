# 参与 NanaZip 贡献

## 如何成为贡献者

- 直接贡献代码
  - 本项目采用与 Windows NT 内核驱动相似（但不完全相同）的代码风格。
    提交 Pull Request 之前，请逐字阅读代码风格指南。为维护 NanaZip 的源码
    质量并尊重审查者的时间，不符合规范的 PR 将不予合并。
  - 我们要求所有贡献与现有风格完全一致，没有任何例外。对代码规范有疑问，
    请先开 issue 讨论，再提交 Pull Request。
  - 提交 Pull Request 即表示你同意以 MIT 许可证授权你的贡献。我们保留
    按需重用和改写贡献者 PR 的权利。
  - 禁止修改任何以 "Mile." 前缀开头的文件与文件夹中的内容，这些实现被
    多个项目共享。未经许可修改此类文件的 PR 将直接关闭、不予合并。
  - 允许使用 AI/LLM 工具辅助编写代码，但最终提交必须由你本人撰写并审查，
    且符合本项目的代码风格与约定。若发现 PR 中没有你的贡献或未经你审查，
    该 PR 将直接关闭、不予合并。
- 反馈建议与 bug
  - 我们使用 GitHub issues 跟踪 bug 与功能需求。
  - 提交 bug 或一般问题请 [新建 issue](../../issues/new)。

## 代码贡献指南

### 环境要求

- Visual Studio 2022 或更高版本。
  - 如需编译 ARM64 版本，还需要安装 ARM64 组件（MSVC 工具链与 ATL/MFC）。
- Windows 11 SDK 或更高版本。
  - 同样，如需编译 ARM64 版本，请安装对应的 ARM64 组件。

### 如何构建 NanaZip 全部目标

完整指南（环境要求、输出位置、常见问题）见 [BUILDING.md](BUILDING.md)。

简版：在仓库根目录运行 `build\BuildAllTargets.cmd`。

### 如何修改或调试 NanaZip

如果尚未运行过 `build\RestoreNuGetPackages.cmd` 或 `build\BuildAllTargets.cmd`，建议先
运行前者还原 NuGet 包。（Visual Studio 2026 的较新版本可能不会自动还原
NuGet 包。）

在仓库根目录打开 `build\NanaZip.slnx`。

### 代码风格与约定

更多细节请阅读 Kenji Mouri 的
[MD24: 我所有开源项目的代码风格](https://github.com/MouriNaruto/MouriDocs/tree/main/docs/24)。

所有语言请遵循源码树中的 [.editorconfig](https://editorconfig.org/) 文件，
多数 IDE 原生支持或可通过插件支持。

#### 对继承的 7-Zip 主源码的修改

> [!NOTE]
> 请先阅读 https://github.com/M2Team/NanaZip/blob/main/License.md 了解
> 哪些文件属于继承的 7-Zip 主源码。

> [!NOTE]
> 向继承的 7-Zip 代码中添加内容时，如果原有的 7-Zip 方法与函数之间没有
> 空行，请不要额外添加空行。

为简化与 7-Zip 主线的同步，修改标记是必需的：原始 7-Zip 主线代码应以注释
形式保留。格式如下：

```
// **************** NanaZip Modification Start **************** 
// xzProps.numTotalThreads = (int)(prop.ulVal); 
xzProps.numTotalThreads = ((int)prop.ulVal) > 1 ? (int)prop.ulVal : 1; 
// **************** NanaZip Modification End **************** 
```

多行修改也可以使用以下格式：

```
// **************** 7-Zip ZS Modification Start ****************
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
kBZip2 = 12,

kLZMA = 14,

kTerse = 18,
kLz77 = 19,
kZstdPk = 20,

kZstdWz = 93,
kMP3 = 94,
kXz = 95,
kJpeg = 96,
kWavPack = 97,
kPPMd = 98,
kWzAES = 99
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
kBZip2 = 12,   // File is compressed using BZIP2 algorithm

kLZMA = 14,    // LZMA

kTerse = 18,   // File is compressed using IBM TERSE (new)
kLz77 = 19,    // IBM LZ77 z Architecture
kZstdPk = 20,  // deprecated (use method 93 for zstd)

kZstd = 93,    // Zstandard (zstd) Compression
kMP3 = 94,     // MP3 Compression
kXz = 95,      // XZ Compression
kJpeg = 96,    // JPEG variant
kWavPack = 97, // WavPack compressed data
kPPMd = 98,    // PPMd version I, Rev 1
kWzAES = 99    // AE-x encryption marker (see APPENDIX E)
// **************** 7-Zip ZS Modification End ****************
```

#### 翻译贡献须知

`resw` 文件中的所有 `comment` 应保持英文，便于日后维护。
