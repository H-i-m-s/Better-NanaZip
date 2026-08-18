// SssPasswordFile.h

#ifndef __SSS_PASSWORD_FILE_H
#define __SSS_PASSWORD_FILE_H

#include "../../../Common/MyString.h"

// **************** SSS Modification Start ****************
// LocalState 密码本 (passwords.txt) 与 API 配置 (api_config.txt) 读写。
// 懒创建原则：文件不存在时读返回 false 且不创建；只有用户真正输入（或导入）才落盘。
// 路径：%LOCALAPPDATA%\Packages\<PackageFamilyName>\LocalState
// **************** SSS Modification End ****************

// 返回包内 LocalState 目录；非打包环境或获取失败返回空串
FString SssGetLocalStateDir();

// 读取任意 UTF-8 文件（兼容带/不带 BOM）；文件不存在返回 false
bool SssReadFileUtf8(const FString &path, UString &text);
// 按行拆分（兼容 CRLF/LF，去行尾 \r）；不丢空行（由调用方决定是否过滤）
void SssSplitTextToLines(const UString &text, UStringVector &lines);

// ---- 密码本 passwords.txt ----
// 格式：每行一个密码；整行以 '#' 开头为注释行；空行忽略
// 读时保留注释行（lines 含注释与密码，passwords 仅密码），写盘时保留注释
struct SssPasswordBook
{
  UStringVector lines;      // 原始有效行（含 # 注释行，不含空行）
  UStringVector passwords;  // 仅密码行（用于自动匹配）
};

// 读：文件不存在 → 返回 false（不创建文件），book 置空
bool SssLoadPasswordBook(SssPasswordBook &book);
// 写：全量重写（UTF-8 带 BOM，CRLF），懒创建：任何一次写都会创建文件
bool SssSavePasswordBook(const UStringVector &lines);

// ---- API 配置 api_config.txt ----
// 六项 key=value，UTF-8；默认全空白不硬编码；懒创建
struct SssApiConfig
{
  UString Url;         // CloudApiUrl
  UString AppId;       // CloudAppId
  UString AesKey;      // CloudAesKey
  UString SigningKey;  // CloudSigningKey
  UString PackageName; // CloudPackageName
  UString Fingerprint; // CloudFingerprint
  UString ProtocolVersion; // CloudProtocolVersion, default 2.2.3
  UInt32 TimeoutSeconds; // CloudTimeoutSeconds, default 5

  SssApiConfig() { Clear(); }
  void Clear();
  // 六项全部非空才算完整（云端查询启用时的前置条件）
  bool IsComplete() const;
};

// 读：文件不存在 → 返回 false（不创建文件），cfg 全空白
bool SssLoadApiConfig(SssApiConfig &cfg);
// 写：六行固定顺序 key=value（UTF-8）；懒创建
bool SssSaveApiConfig(const SssApiConfig &cfg);

#endif
