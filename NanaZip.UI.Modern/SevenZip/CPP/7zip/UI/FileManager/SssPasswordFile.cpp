// SssPasswordFile.cpp

#include "StdAfx.h"

#include "SssPasswordFile.h"

#include <appmodel.h>
#include <shlobj.h>

#include "../../../Common/StringConvert.h"
#include "../../../Common/MyBuffer.h"
#include "../../../Windows/FileIO.h"

using namespace NWindows;
using namespace NWindows::NFile;

// **************** SSS Modification Start ****************

static const wchar_t * const kPasswordFileName = L"passwords.txt";
static const wchar_t * const kApiConfigFileName = L"api_config.txt";

static const wchar_t * const kApiKeys[6] =
{
  L"CloudApiUrl",
  L"CloudAppId",
  L"CloudAesKey",
  L"CloudSigningKey",
  L"CloudPackageName",
  L"CloudFingerprint"
};

FString SssGetLocalStateDir()
{
  FString dir;
  PWSTR localAppData = NULL;
  if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &localAppData) != S_OK)
    return dir;
  UString base = localAppData;
  CoTaskMemFree(localAppData);

  const UINT32 filter = PACKAGE_FILTER_HEAD | PACKAGE_FILTER_DIRECT;
  UINT32 bufferLength = 0;
  UINT32 count = 0;
  // 第一次调用：buffer=NULL 查询所需大小，返回 ERROR_INSUFFICIENT_BUFFER(0x7A) 是正常行为
  LONG rc = GetCurrentPackageInfo(filter, &bufferLength, NULL, &count);
  if (rc != ERROR_INSUFFICIENT_BUFFER || bufferLength == 0)
    return dir;

  CByteBuffer buf;
  buf.Alloc(bufferLength);
  if (GetCurrentPackageInfo(filter, &bufferLength, buf, &count) != ERROR_SUCCESS)
    return dir;

  const PACKAGE_INFO *info = (const PACKAGE_INFO *)(const void *)buf;
  if (info->packageFamilyName == NULL || info->packageFamilyName[0] == 0)
    return dir;

  UString full = base;
  full += L"\\Packages\\";
  full += (const wchar_t *)info->packageFamilyName;
  full += L"\\LocalState";
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
    for (int k = 0; k < 6; k++)
    {
      if (key == (const wchar_t *)kApiKeys[k])
      {
        UString *dest = (k == 0) ? &cfg.Url : (k == 1) ? &cfg.AppId : (k == 2) ? &cfg.AesKey
            : (k == 3) ? &cfg.SigningKey : (k == 4) ? &cfg.PackageName : &cfg.Fingerprint;
        *dest = value;
        break;
      }
    }
  }
  return true;
}

bool SssSaveApiConfig(const SssApiConfig &cfg)
{
  UString text;
  for (int k = 0; k < 6; k++)
  {
    const UString *value = (k == 0) ? &cfg.Url : (k == 1) ? &cfg.AppId : (k == 2) ? &cfg.AesKey
        : (k == 3) ? &cfg.SigningKey : (k == 4) ? &cfg.PackageName : &cfg.Fingerprint;
    if (k != 0)
      text += L"\r\n";
    text += kApiKeys[k];
    text += L"=";
    text += *value;
  }
  return SssWriteWholeFileUtf8(SssLocalStateFilePath(kApiConfigFileName), text);
}

// **************** SSS Modification End ****************
