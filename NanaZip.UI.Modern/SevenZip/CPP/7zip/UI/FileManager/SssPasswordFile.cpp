// SssPasswordFile.cpp

#include "StdAfx.h"

#include "SssPasswordFile.h"

#include <appmodel.h>
#include <shlobj.h>
#include <wincrypt.h>
#include <vector>

#include "../../../Common/StringConvert.h"
#include "../../../Common/MyBuffer.h"
#include "../../../Windows/FileIO.h"
#include "../../../Windows/FileDir.h"

using namespace NWindows;
using namespace NWindows::NFile;

// **************** SSS Modification Start ****************

static const wchar_t * const kPasswordFileName = L"passwords.txt";
static const wchar_t * const kApiConfigFileName = L"api_config.txt";
// Unpackaged (exe/green) processes have no package identity: keep
// config/history in an independent dir %LOCALAPPDATA%\NanaZip\ instead
// of the MSIX package data dir (removed together with the package).
static const wchar_t * const kUnpackagedDataDirName = L"NanaZip";

static const wchar_t * const kApiKeys[8] =
{
  L"CloudApiUrl",
  L"CloudAppId",
  L"CloudAesKey",
  L"CloudSigningKey",
  L"CloudPackageName",
  L"CloudFingerprint",
  L"CloudProtocolVersion",
  L"CloudTimeoutSeconds"
};

static const wchar_t * const kProtectedValuePrefix = L"dpapi:";

static bool SssProtectConfigValue(const UString &plain, UString &protectedValue)
{
  protectedValue.Empty();
  AString utf8 = UnicodeStringToMultiByte(plain, CP_UTF8);
  DATA_BLOB input = {};
  input.pbData = (BYTE *)(void *)utf8.Ptr();
  input.cbData = utf8.Len();
  DATA_BLOB output = {};
  if (!::CryptProtectData(&input, L"NanaZip cloud password configuration",
      NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &output))
    return false;
  DWORD length = 0;
  const bool sizeOk = ::CryptBinaryToStringW(output.pbData, output.cbData,
      CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &length) != FALSE;
  std::vector<wchar_t> encoded(sizeOk ? length : 0, 0);
  const bool encodeOk = sizeOk && ::CryptBinaryToStringW(output.pbData,
      output.cbData, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
      encoded.data(), &length) != FALSE;
  ::LocalFree(output.pbData);
  if (!encodeOk)
    return false;
  protectedValue = kProtectedValuePrefix;
  protectedValue += encoded.data();
  return true;
}

static bool SssUnprotectConfigValue(const UString &stored, UString &plain)
{
  plain.Empty();
  if (!stored.IsPrefixedBy(kProtectedValuePrefix))
  {
    plain = stored; // Backward-compatible read; next save migrates to DPAPI.
    return true;
  }
  const UString encoded = stored.Ptr(MyStringLen(kProtectedValuePrefix));
  DWORD length = 0;
  if (!::CryptStringToBinaryW(encoded.Ptr(), encoded.Len(),
      CRYPT_STRING_BASE64, NULL, &length, NULL, NULL))
    return false;
  std::vector<BYTE> bytes(length);
  if (!::CryptStringToBinaryW(encoded.Ptr(), encoded.Len(),
      CRYPT_STRING_BASE64, bytes.data(), &length, NULL, NULL))
    return false;
  DATA_BLOB input = {};
  input.pbData = bytes.data();
  input.cbData = length;
  DATA_BLOB output = {};
  if (!::CryptUnprotectData(&input, NULL, NULL, NULL, NULL,
      CRYPTPROTECT_UI_FORBIDDEN, &output))
    return false;
  AString utf8;
  utf8.SetFrom_CalcLen((const char *)output.pbData, output.cbData);
  plain = MultiByteToUnicodeString(utf8, CP_UTF8);
  ::SecureZeroMemory(output.pbData, output.cbData);
  ::LocalFree(output.pbData);
  ::SecureZeroMemory(bytes.data(), bytes.size());
  return true;
}

FString SssGetLocalStateDir()
{
  FString dir;
  PWSTR localAppData = NULL;
  if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &localAppData) != S_OK)
    return dir;
  UString base = localAppData;
  CoTaskMemFree(localAppData);

  // 打包进程（MSIX 安装）：用当前包身份的 LocalState，与其他包数据隔离
  const UINT32 filter = PACKAGE_FILTER_HEAD | PACKAGE_FILTER_DIRECT;
  UINT32 bufferLength = 0;
  UINT32 count = 0;
  // 第一次调用：buffer=NULL 查询所需大小，返回 ERROR_INSUFFICIENT_BUFFER(0x7A) 是正常行为
  if (GetCurrentPackageInfo(filter, &bufferLength, NULL, &count) == ERROR_INSUFFICIENT_BUFFER &&
      bufferLength != 0)
  {
    CByteBuffer buf;
    buf.Alloc(bufferLength);
    if (GetCurrentPackageInfo(filter, &bufferLength, buf, &count) == ERROR_SUCCESS)
    {
      const PACKAGE_INFO *info = (const PACKAGE_INFO *)(const void *)buf;
      const wchar_t *packageFamilyName =
        (info->packageFamilyName && info->packageFamilyName[0])
          ? (const wchar_t *)info->packageFamilyName
          : NULL;
      if (packageFamilyName)
      {
        UString full = base;
        full += L"\\Packages\\";
        full += packageFamilyName;
        full += L"\\LocalState";
        return us2fs(full);
      }
    }
  }

  // Unpackaged (exe/green): independent data dir, created on first write
  UString full = base;
  full += L"\\";
  full += kUnpackagedDataDirName;
  return us2fs(full);
}

bool SssReadFileUtf8(const FString &path, UString &text)
{
  text.Empty();
  NIO::CInFile file;
  if (!file.OpenShared(path, true))
    return false;
  UInt64 size64 = 0;
  if (!file.GetLength(size64))
  {
    file.Close();
    return false;
  }
  if (size64 == 0)
    return true;
  if (size64 > (UInt64)(1 << 30))
    return false;
  size_t size = (size_t)size64;
  AString bytes;
  char *p = bytes.GetBuf((unsigned)size + 1);
  size_t processed = 0;
  if (!file.ReadFull(p, size, processed))
  {
    file.Close();
    return false;
  }
  bytes.ReleaseBuf_SetEnd((unsigned)processed);
  file.Close();

  const char *s = bytes.Ptr();
  size_t len = bytes.Len();
  if (len >= 3 && (Byte)s[0] == 0xEF && (Byte)s[1] == 0xBB && (Byte)s[2] == 0xBF)
  {
    s += 3;
    len -= 3;
  }
  AString body;
  body.SetFrom_CalcLen(s, (unsigned)len);
  text = MultiByteToUnicodeString(body, CP_UTF8);
  return true;
}

static bool SssWriteWholeFileUtf8(const FString &path, const UString &text)
{
  if (path.IsEmpty())
    return false;
  // Ensure the parent directory exists (first write in exe builds)
  const int sep = path.ReverseFind(FCHAR_PATH_SEPARATOR);
  if (sep >= 0)
    NWindows::NFile::NDir::CreateComplexDir(path.Left((unsigned)sep));
  NIO::COutFile file;
  // createAlways=true → CREATE_ALWAYS（覆盖已有文件）；密码本/配置会多次全量重写
  if (!file.Create(path, true))
    return false;
  static const char * const kBom = "\xEF\xBB\xBF";
  if (!file.WriteFull(kBom, 3))
  {
    file.Close();
    return false;
  }
  AString body = UnicodeStringToMultiByte(text, CP_UTF8);
  if (!body.IsEmpty())
    if (!file.WriteFull(body.Ptr(), body.Len()))
    {
      file.Close();
      return false;
    }
  file.Close();
  return true;
}

void SssSplitTextToLines(const UString &text, UStringVector &lines)
{
  UString cur;
  for (unsigned i = 0; i < text.Len(); i++)
  {
    wchar_t c = text[i];
    if (c == L'\n')
    {
      if (!cur.IsEmpty() && cur.Back() == L'\r')
        cur.DeleteBack();
      lines.Add(cur);
      cur.Empty();
    }
    else
      cur += c;
  }
  if (!cur.IsEmpty())
  {
    if (cur.Back() == L'\r')
      cur.DeleteBack();
    lines.Add(cur);
  }
}

static FString SssLocalStateFilePath(const wchar_t *name)
{
  FString dir = SssGetLocalStateDir();
  if (dir.IsEmpty())
    return FString();
  return dir + FCHAR_PATH_SEPARATOR + (const wchar_t *)name;
}

bool SssLoadPasswordBook(SssPasswordBook &book)
{
  book.lines.Clear();
  book.passwords.Clear();
  UString text;
  if (!SssReadFileUtf8(SssLocalStateFilePath(kPasswordFileName), text))
    return false;
  UStringVector lines;
  SssSplitTextToLines(text, lines);
  for (unsigned i = 0; i < lines.Size(); i++)
  {
    const UString &line = lines[i];
    if (line.IsEmpty())
      continue;
    book.lines.Add(line);
    if (line[0] != L'#')
      book.passwords.Add(line);
  }
  return true;
}

bool SssSavePasswordBook(const UStringVector &lines)
{
  UString text;
  for (unsigned i = 0; i < lines.Size(); i++)
  {
    if (i != 0)
      text += L"\r\n";
    text += lines[i];
  }
  return SssWriteWholeFileUtf8(SssLocalStateFilePath(kPasswordFileName), text);
}

void SssApiConfig::Clear()
{
  Url.Empty();
  AppId.Empty();
  AesKey.Empty();
  SigningKey.Empty();
  PackageName.Empty();
  Fingerprint.Empty();
  ProtocolVersion.Empty();
  TimeoutSeconds = 0;
}

bool SssApiConfig::IsComplete() const
{
  return !Url.IsEmpty() && !AppId.IsEmpty() && !AesKey.IsEmpty()
      && !SigningKey.IsEmpty() && !PackageName.IsEmpty() && !Fingerprint.IsEmpty();
}

bool SssLoadApiConfig(SssApiConfig &cfg)
{
  cfg.Clear();
  UString text;
  if (!SssReadFileUtf8(SssLocalStateFilePath(kApiConfigFileName), text))
    return false;
  UStringVector lines;
  SssSplitTextToLines(text, lines);
  for (unsigned i = 0; i < lines.Size(); i++)
  {
    const UString &line = lines[i];
    if (line.IsEmpty() || line[0] == L'#')
      continue;
    int eq = line.Find(L'=');
    if (eq < 0)
      continue;
    UString key = line.Left((unsigned)eq);
    UString value = line.Ptr((unsigned)(eq + 1));
    for (int k = 0; k < 8; k++)
    {
      if (key != (const wchar_t *)kApiKeys[k])
        continue;
      if (k == 0) cfg.Url = value;
      else if (k >= 1 && k <= 5)
      {
        UString plain;
        if (!SssUnprotectConfigValue(value, plain))
        {
          cfg.Clear();
          return false;
        }
        if (k == 1) cfg.AppId = plain;
        else if (k == 2) cfg.AesKey = plain;
        else if (k == 3) cfg.SigningKey = plain;
        else if (k == 4) cfg.PackageName = plain;
        else cfg.Fingerprint = plain;
      }
      else if (k == 6 && !value.IsEmpty()) cfg.ProtocolVersion = value;
      else if (k == 7)
      {
        const UInt32 timeout = (UInt32)wcstoul(value.Ptr(), NULL, 10);
        if (timeout >= 1 && timeout <= 30)
          cfg.TimeoutSeconds = timeout;
      }
      break;
    }
  }
  return true;
}

bool SssSaveApiConfig(const SssApiConfig &cfg)
{
  UString text;
  for (int k = 0; k < 8; k++)
  {
    UString value;
    if (k == 0) value = cfg.Url;
    else if (k >= 1 && k <= 5)
    {
      const UString *plain = (k == 1) ? &cfg.AppId : (k == 2) ? &cfg.AesKey
          : (k == 3) ? &cfg.SigningKey : (k == 4) ? &cfg.PackageName
          : &cfg.Fingerprint;
      if (!SssProtectConfigValue(*plain, value))
        return false;
    }
    else if (k == 6) value = cfg.ProtocolVersion;
    else
    {
      if (cfg.TimeoutSeconds != 0)
        value.Add_UInt32(cfg.TimeoutSeconds);
    }
    if (k != 0)
      text += L"\r\n";
    text += kApiKeys[k];
    text += L"=";
    text += value;
  }
  return SssWriteWholeFileUtf8(SssLocalStateFilePath(kApiConfigFileName), text);
}

// **************** SSS Modification End ****************
