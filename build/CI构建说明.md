# GitHub Actions 构建说明（x64 msix）

主文件：`.github/workflows/BuildMsix.yml`
触发：Actions → `Build MSIX (x64)` → Run workflow（手动，可填版本号）

产物：`NanaZipPackage_<版本>_x64_Test.msixbundle` 与开发证书 `.cer`，以 artifact 形式提供。

---

## 一、它实际做了什么

直接调用仓库既有的构建入口，不另写一套：

```powershell
build\构建发布.ps1 -Mode 构建 -Format msix -Arch x64 -Version <v>
```

内部顺序：

1. `01-编译.ps1` 增量 → FM 单入口 Release x64（props 自动展开依赖图）
2. `02-打包msix.ps1` wapproj 打包 + bundle 签名
3. `05-汇总发布.ps1` 汇总到 `Output\发布\<版本>\`

`-Format msix` 不会触发 `03` / `04`，所以不组装文件集、不编 Inno 安装器。

---

## 二、前置：两个 secrets

| 名称 | 内容 |
|---|---|
| `SSS_NANAZIP_PFX_BASE64` | 开发证书 `.pfx` 的 base64 |
| `SSS_NANAZIP_PFX_PASSWORD` | 该 pfx 的密码 |

生成 base64：

```powershell
[Convert]::ToBase64String([IO.File]::ReadAllBytes('SSS-NanaZip-Development.pfx')) | Set-Clipboard
```

必须与本地同一张证书。`Sign-SSSNanaZipPackage.ps1` 按 Subject `CN=SSS NanaZip Development` 查找证书库里的条目，换一张证书会导致签名主体变化，装包时要重新信任。

---

## 三、环境

`windows-2022` runner 自带：VS2022 Enterprise（C++ 桌面 + UWP）、Windows SDK、MSBuild、NuGet。

workflow 额外做两件事：

- `microsoft/setup-msbuild@v3` 把 MSBuild 加进 PATH
- 预装 `Mile.Project.Configurations` MSBuild SDK 到全局包目录

msix 链路不需要 Inno Setup。

---

## 四、为什么不做 obj 级增量

结论：**不缓存 `Output\Objects`。** 四条理由，按重要性排。

**1. 省的和你付的不成比例。**
FM 单入口编译实测约 48 秒（`build\构建流程与注意事项.md`）。`Output\Objects` 现在 9 GB，Release 部分 6 GB，压缩后 2~3 GB。GitHub 的 cache 要上传和下载这一坨，通常比自己重编更慢。

**2. 配额是仓库共享的。**
GitHub 单仓库 cache 总上限 10 GB，超出按最近使用淘汰。塞进一份 2~3 GB 的 obj 缓存，会把 NuGet 包缓存挤出去。而 NuGet 缓存每次构建都受益，obj 缓存只在"源码几乎没改"时受益。

**3. 正确性靠伪造时间戳，失败模式很坏。**
MSBuild 的 FileTracker 用文件时间戳判断是否需要重编。干净 runner 上 checkout 出来的源码 mtime 全是新的，恢复回来的 obj 反而是旧的，直接导致全量重编。要让它生效，必须：

1. 缓存里额外存上次构建的 commit sha
2. 恢复后把所有源码 mtime 统一压到过去
3. 再用 `git diff --name-only <上次sha> HEAD` 把改动文件 mtime 刷到现在

这套流程一旦缓存不完整、或并发构建互相覆盖，就可能出现"构建成功但少编了一个文件"的二进制。这比构建慢更糟。

**4. 瓶颈不在编译。**
即使 obj 全命中，省下的也只是编译那 1~2 分钟。wapproj 打包、XAML 编译、resources.pri、bundle 签名、NuGet restore 一样都省不掉，而它们才是时间大头。

如果哪天编译真的变成瓶颈（比如项目膨胀到几分钟、或者构建频率高到需要排队），正确做法不是 obj 缓存，而是 sccache 这类按内容哈希的编译缓存。但它要接管 `cl.exe`，对开了 LTO 的配置会退化成直通，属于另一个工程。

---

## 五、耗时预期

| 阶段 | 首次（无缓存） | 后续（缓存命中） |
|---|---|---|
| 环境准备 | 3~5 分钟 | 1~2 分钟 |
| 编译 | 1~2 分钟 | 1~2 分钟 |
| wapproj 打包 + 签名 | 2~4 分钟 | 2~4 分钟 |
| 汇总 + 上传 | 1 分钟 | 1 分钟 |
| 合计 | 约 8~12 分钟 | 约 6~9 分钟 |

首次数字是估算，跑一次后按实际调整。

---

## 六、以后要出 exe 怎么改

在 `BuildMsix.yml` 基础上：

1. 把 `-Format msix` 改成 `-Format exe`
2. 加一步 `choco install innosetup --no-progress -y`（`04-编译exe.ps1` 依赖 `ISCC.exe`）
3. Upload 的 path 不变（`Output/发布/**`），会多出 `NanaZipSetup_<版本>_x64.exe`

exe 这条链会额外跑 `03`（从 bundle 解包出文件集）和 `04`（ISCC），所以 msix 打包不会被跳过。

---

## 七、为 CI 做的仓库改动

| 文件 | 改动 |
|---|---|
| `build\步骤\01-编译.ps1` | `vcvars64.bat` 改为 vswhere 探测 + 常见路径回退。原来硬编码 `2022\Community`，CI 上是 `Enterprise`，会直接失败 |
| `build\步骤\02-打包msix.ps1` | 同上 |
| `build\步骤\03-组装文件集.ps1` | 壳包签名 thumbprint 改为可覆盖（`-Thumbprint` 或 `NANAZIP_SIGN_THUMBPRINT`），默认值不变；签名失败现在会抛错，不再静默跳过。msix 链路不经过它，但 exe/绿色版会回到来 |

三个文件都保持 UTF-8 BOM + CRLF（PowerShell 5.1 中文解析依赖 BOM）。

---

## 八、排查

- 构建失败先看 `vcvars64` 有没有探测到、证书有没有导入成功
- `02` 报签名失败 = secrets 里的 pfx 与证书 Subject 不匹配，或私钥不可用
- restore 失败可以先看是不是 windows-2022 的 NuGet 缓存问题，必要时临时加 `dotnet nuget locals all --clear`
- 需要更细的 MSBuild 日志时，在 Build 步骤的命令后加 `/bl`，或本地 `-Mode 打包` 跳过编译复现
