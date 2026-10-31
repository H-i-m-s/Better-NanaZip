# GitHub Actions 构建说明（只出 exe）

主文件：`.github/workflows/BuildExe.yml`
触发：Actions → `Build EXE (x64)` → Run workflow（手动，支持填版本号）

产物：`NanaZipSetup_<版本>_x64.exe` 及同目录的 `安装说明.txt`、开发证书 `.cer`，以 artifact 形式提供。

---

## 一、它实际做了什么

直接调用仓库既有的 `build\构建发布.ps1 -Mode 构建 -Format exe -Arch x64 -Version <v>`，不另写一套构建。

这条链的顺序是：

1. `01-编译.ps1` 增量 → FM 单入口 Release x64（props 自动展开依赖图）
2. `02-打包msix.ps1` wapproj 打包 + bundle 签名
3. `03-组装文件集.ps1` 从 bundle 解包出文件集 + 生成并签名壳包
4. `04-编译exe.ps1` ISCC 编译 Inno 安装器
5. `05-汇总发布.ps1` 汇总到 `Output\发布\<版本>\`

注意第 2 步：exe 的文件集来自 msixbundle，所以**出 exe 也必须跑一遍 msix 打包**。这是仓库既有结构，不是 CI 引入的。

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

为什么不能用临时自签证书：安装器里的壳包（`NanaZipShellExt.x64.msix`）必须能通过目标机器上已信任的开发证书校验，否则 Win11 右键菜单注册会失败。所以 CI 必须用与本地同一张证书。

导入后：

- msixbundle 由 `Sign-SSSNanaZipPackage.ps1` 按 Subject `CN=SSS NanaZip Development` 查找证书签名
- 壳包由 `03-组装文件集.ps1` 按 `NANAZIP_SIGN_THUMBPRINT` 环境变量指定的 thumbprint 签名

---

## 三、环境

`windows-2022` runner 自带：VS2022 Enterprise（C++ 桌面 + UWP）、Windows SDK、MSBuild、choco、NuGet。

workflow 额外做三件事：

- `microsoft/setup-msbuild@v3` 把 MSBuild 加进 PATH
- `choco install innosetup` 装 Inno Setup 6（`04-编译exe.ps1` 依赖 `ISCC.exe`）
- 预装 `Mile.Project.Configurations` MSBuild SDK 到全局包目录

---

## 四、增量策略

先说结论：**没有缓存 `Output\Objects`，也不建议缓存。**

依据是仓库自己的实测数据（`build\构建流程与注意事项.md`）：FM 单入口编译约 48 秒。CI 上真正的时间花在 checkout、NuGet restore、SDK 预装、wapproj 打包和 ISCC，不在编译本身。

而 `Output\Objects` 现在是 6 GB 左右，压缩后仍有 2~3 GB。GitHub 单仓库 cache 总上限 10 GB，上传下载这些体积的耗时通常超过重新编译的时间，收益为负。

workflow 只缓存两样小东西：

| 缓存 | 大小量级 | key |
|---|---|---|
| `~\.nuget\packages` | 数百 MB | `nuget-win-<工程文件 hash>`，`restore-keys: nuget-win-` 兜底 |
| Mile SDK 由上面这个缓存覆盖，命中就跳过下载 | — | — |

每次构建的 `run_id` 不同不会影响命中，因为 restore-keys 会回退到最近一份同前缀缓存。

如果以后确实需要 obj 级增量（例如 runner 变慢、或全量超过 10 分钟），可行方案是：

1. 缓存 `Output\Objects\Release`
2. 在缓存里额外存一份 `上次构建的 commit sha`
3. 恢复后：把所有源文件 mtime 统一设为 2020-01-01，再用 `git diff --name-only <上次sha> HEAD` 把改动过的文件 mtime 设为当前时间
4. 这样 MSBuild 的 FileTracker 会跳过未改动文件，只重编改动项

这套做法没写进 workflow，因为按当前 48 秒的编译成本算不过来。

---

## 五、耗时预期

| 阶段 | 首次（无缓存） | 后续（缓存命中） |
|---|---|---|
| 环境准备 | 3~5 分钟 | 1~2 分钟 |
| 编译 | 1~2 分钟 | 1~2 分钟 |
| msix 打包 | 2~4 分钟 | 2~4 分钟 |
| 组装 + ISCC + 汇总 | 1 分钟 | 1 分钟 |
| 合计 | 约 8~12 分钟 | 约 6~9 分钟 |

首次数字是估算，跑一次后按实际调整。

---

## 六、为 CI 做的仓库改动

`.github/workflows/BuildExe.yml` 之外，改了三个构建脚本，本地行为不变：

| 文件 | 改动 |
|---|---|
| `build\步骤\01-编译.ps1` | `vcvars64.bat` 改为 vswhere 探测 + 常见路径回退。原来硬编码 `2022\Community`，CI 上是 `Enterprise`，会直接失败 |
| `build\步骤\02-打包msix.ps1` | 同上 |
| `build\步骤\03-组装文件集.ps1` | 壳包签名的 thumbprint 改为可覆盖（`-Thumbprint` 或 `NANAZIP_SIGN_THUMBPRINT`），默认仍是 `0E7475D74EFAEDDB26D4BDA02FBC6551DC8D72A5`；签名失败现在会抛错，不再静默跳过 |

三个文件都保持 UTF-8 BOM（PowerShell 5.1 中文解析依赖它）。

---

## 七、排查

- 构建失败最先看 `vcvars64` 有没有探测到、`ISCC.exe` 有没有装、证书有没有导入成功
- `03` 报「壳包签名失败」= thumbprint 对不上，检查 secrets 里的 pfx 是不是同一张
- `04` 报「未找到 Inno Setup 6」= choco 步骤失败，看上一步日志
- 产物不完整看 `05` 的发布清单输出
- 需要更细的 MSBuild 日志时，可以临时在 workflow 的 Build 步骤里加 `/bl`，或直接在本地用 `-Mode 打包` 跳过编译复现
