# Code Signing Policy

本政策说明本项目的 Windows 二进制签名方式、来源验证与团队角色。
英文关键表述按 SignPath Foundation 要求保留原文标注。

## 签名对象（What is signed）

- Windows 安装包与便携包：`.msixbundle` / `.msix` / `.exe`（Inno Setup 安装器）
- 右键菜单扩展壳包：`NanaZipShellExt.x64.msix`（sparse MSIX）
- 以上产物全部通过 GitHub Releases 免费公开发布

## 构建与签名流程（Build and signing process）

- Artifacts are built from this repository. 构建入口为
  `build/构建发布.ps1`（步骤见 `docs/BUILDING.md`），构建脚本与全部源码
  均在本仓库内，任何人可复现。
- Only artifacts built from this repository are submitted for signing.
- 私钥由签名服务方 HSM 托管（SignPath Foundation / SignPath.io），
  本项目不持有、不存储签名私钥。
  The private key is held by the signing service (HSM-backed); this
  project does not store the private key.
- 签名后的产物仅发布到 GitHub Releases：
  https://github.com/H-i-m-s/Better-NanaZip/releases

## 团队角色（Team roles，单人维护项目）

- **Authors**（拥有提交权限，可直接修改仓库）：
  - https://github.com/H-i-m-s
- **Reviewers**（外部 PR 合并前须由维护者评审）：
  - https://github.com/H-i-m-s
  - 政策：所有外部 Pull Request 在合并前经过维护者评审。
- **Approvers**（批准每次签名请求）：
  - https://github.com/H-i-m-s
  - 政策：每次签名请求均由维护者明确批准。

## 上游与第三方组件（Upstream）

- 本项目是 [M2Team/NanaZip](https://github.com/M2Team/NanaZip) 的可见 fork，
  许可关系见 [License.md](../License.md)（MIT 为主，第三方组件按其原许可分发）。
- 上游相关许可与署名在 License.md 中逐项列明。

## 隐私（Privacy）

This project does not transfer any information to other networked systems
unless specifically requested by the user. 密码本等用户数据仅存放在本机
用户目录；任何网络功能均需用户明确配置与触发。详见
[隐私政策](../Documents/Privacy.md)。

## 用户验证（Verification）

- 用户应仅从官方 GitHub Releases 页面获取产物：
  https://github.com/H-i-m-s/Better-NanaZip/releases
- Windows 对签名产物会自动校验发布者身份与文件完整性。
