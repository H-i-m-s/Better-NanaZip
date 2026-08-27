# SSS NanaZip 本机开发签名

这些脚本用于在当前电脑上创建和使用 SSS 的开发证书，让自建 NanaZip
MSIX/MSIXBundle 可以本机安装测试。它们只做本机构建辅助，不是标准构建流程
的一部分；标准流程见 [BUILDING.md](../../docs/BUILDING.md)。

## 当前开发身份

```text
Package Name:       SSS.NanaZip.RemotePassword
Publisher:          CN=SSS NanaZip Development
Publisher Display:  SSS
```

当前身份已写入：

```text
<仓库根>\NanaZipPackage\Package.appxmanifest
```

## 文件

- `New-SSSDevCertificate.ps1`：创建当前用户证书库中的自签名证书；store-only 模式只导出公钥 `.cer`。
- `Install-SSSDevCertificate.ps1`：把公钥 `.cer` 导入当前用户 `Trusted People`。
- `Build-SSSNanaZipPackage.ps1`：调用 `build\BuildAllTargets.cmd` 做完整构建，并可对生成包批量签名。
- `Sign-SSSNanaZipPackage.ps1`：优先使用当前用户证书库中的私钥签名，也支持 `.pfx`。
- `Verify-SSSNanaZipPackage.ps1`：只读验证包签名。
- `Install-SSSNanaZipPackage.ps1`：导入公钥并调用 `Add-AppxPackage` 安装。
- `Remove-SSSDevCertificate.ps1`：清理本机 SSS 开发证书和可选的本地证书文件。

## 私钥位置

推荐的本机测试模式是 store-only：

```text
私钥：Cert:\CurrentUser\My
公钥：<仓库根>\.local\SSSDevSigning\SSS-NanaZip-Development.cer
```

`.local/` 已被仓库 `.gitignore` 排除，不会进入 git；`.pfx` 也被忽略。
证书文件（`.cer`/`.pfx`）只存在于本机，不要 `git add -f` 这些文件。

## 使用顺序

### 1. 创建证书

证书通常已经在开发机创建完成。以后如果需要重新创建，推荐使用 store-only 模式：

```powershell
cd <仓库根>
.\build\SSSDevSigning\New-SSSDevCertificate.ps1 -StoreOnly
```

脚本默认创建：

```text
CN=SSS NanaZip Development
```

manifest 的 `Publisher` 必须与证书 Subject 完全一致。

如果以后要给 GitHub Actions 或其他构建机使用，再运行不带 `-StoreOnly` 的
模式，脚本会提示设置 PFX 密码。PFX 包含私钥，不能提交或分享。

### 2. 信任证书

当前电脑通常已完成这一步。以后重新创建证书后执行：

```powershell
.\build\SSSDevSigning\Install-SSSDevCertificate.ps1
```

这只导入公钥 `.cer` 到当前用户 `Trusted People`，不会把私钥放入该存储。

### 3. 构建并签名

完整构建脚本会清空 `Output`，恢复依赖，构建 x64/ARM64 和 MSIX 产物：

```powershell
cd <仓库根>
.\build\SSSDevSigning\Build-SSSNanaZipPackage.ps1 -SignAllMsix
```

如果已经构建过，只想对现有产物签名：

```powershell
.\build\SSSDevSigning\Build-SSSNanaZipPackage.ps1 -SkipBuild -SignAllMsix
```

store-only 模式会从 `Cert:\CurrentUser\My` 读取私钥，不需要输入 PFX 密码。

也可以手动签名：

```powershell
.\build\SSSDevSigning\Sign-SSSNanaZipPackage.ps1 `
  -PackagePath "<仓库根>\Output\Binaries\AppPackages\你的包.msixbundle"
```

如果使用 PFX 文件，再传 `-PfxPath`，脚本会提示 PFX 密码。

### 4. 验证签名

```powershell
.\build\SSSDevSigning\Verify-SSSNanaZipPackage.ps1 `
  -PackagePath "<仓库根>\Output\Binaries\AppPackages\你的包.msixbundle"
```

### 5. 安装

确认签名和包身份都正确后执行：

```powershell
.\build\SSSDevSigning\Install-SSSNanaZipPackage.ps1 `
  -PackagePath "<仓库根>\Output\Binaries\AppPackages\你的包.msixbundle"
```

如果包身份相同但版本号回退，可以在确认是自己的开发包后使用：

```powershell
.\build\SSSDevSigning\Install-SSSNanaZipPackage.ps1 `
  -PackagePath "<仓库根>\Output\Binaries\你的包.msixbundle" `
  -ForceUpdate
```

### 6. 验证安装和右键菜单

```powershell
Get-AppxPackage -Name 'SSS.NanaZip.RemotePassword'
```

然后重启 Explorer：

```powershell
Stop-Process -Name explorer -Force
Start-Process explorer.exe
```

在文件或文件夹上右键，确认 SSS NanaZip 是否出现在 Windows 11 第一层菜单。

## 重要限制

- 自签名证书只适合你的开发电脑或明确导入证书的测试机。
- `.pfx` 包含私钥，不得上传 GitHub，不得分享；本机测试优先使用 `-StoreOnly`。
- 这套证书不会让安装包变成 Microsoft Store 或公开可信发布包。
- 修改 `Name` / `Publisher` 后，系统会把它当作独立应用，不会覆盖官方 NanaZip。
- 如果打包过程中仍使用旧 manifest，签名时会出现 Publisher 与证书 Subject 不匹配。
- MSIX Bundle 的签名应在 Bundle 生成完成后进行；不要在签名后再修改包内容。
- Windows PowerShell 5.1 执行脚本时，脚本文件保持 ASCII，避免系统编码导致解析错误。
