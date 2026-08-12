// CompressXamlAdapter.cpp
// 压缩对话框 XAML 适配器：Core（规则层）与 K7_COMPRESS_DIALOG_CONTEXT（快照）之间的桥。
// 只做数据映射与命令转发，规则仍在 CCompressDialogCore。

// IFileSaveDialog needs a Vista+ SDK level. 7-Zip's Common headers do not
// set _WIN32_WINNT/NTDDI_VERSION, so the interface would be hidden; define
// them before any Windows header in this translation unit.
#ifndef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WIN10
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN10
#endif

#include "StdAfx.h"

#include <memory>
#include <vector>

#include "../../../Common/MyCom.h"
#include "../../../Common/StringConvert.h"

#include <K7User.h>
#include <shlobj.h>

#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileIO.h"
#include "../../../Windows/FileName.h"

#include "../FileManager/BrowseDialog.h"
#include "../FileManager/LangUtils.h"
#include "../FileManager/SplitUtils.h"

#include "NanaZip.Modern.h"

#include "CompressDialogCore.h"
#include "CompressDialog.h"

#include "CompressXamlAdapter.h"

#include "CompressDialogRes.h"
#include "ExtractRes.h"
#include "resource2.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;

static const char * const k_DontSave_Exts =
  "xpi odt ods docx xlsx ";

static void AddFilter(CObjectVector<CBrowseFilterInfo> &filters,
    const UString &description, const UString &ext)
{
  CBrowseFilterInfo &f = filters.AddNew();
  UString mask ("*.");
  mask += ext;
  f.Masks.Add(mask);
  f.Description = description;
  f.Description += " (";
  f.Description += mask;
  f.Description += ")";
}

struct CCompressXamlHost
{
  CCompressDialogCore *Core;
  PK7_COMPRESS_DIALOG_CONTEXT Context;
  HWND Parent;
};

static void CopyText(WCHAR *dst, size_t dstChars, const UString &s)
{
  const size_t n = s.Len();
  const size_t copy = (n < dstChars - 1) ? n : dstChars - 1;
  memcpy(dst, s.Ptr(), copy * sizeof(wchar_t));
  dst[copy] = 0;
}

static void FillOptionList(PK7_COMPRESS_OPTION_LIST list,
    const CObjectVector<CCompressDialogCore::COptionItem> &items,
    UInt64 selValue, bool visible, bool enabled)
{
  const UINT32 count = (UINT32)items.Size();
  list->Count = count;
  list->SelectedIndex = -1;
  for (UINT32 i = 0; i < count; i++)
  {
    PK7_COMPRESS_OPTION_ITEM item = &list->Items[i];
    CopyText(item->DisplayText, K7_COMPRESS_MAX_DISPLAY_TEXT, items[i].Display);
    item->SemanticText[0] = 0;
    item->Value = (INT64)items[i].Value;
    item->Flags = (items[i].Value == (UInt64)(Int64)-1) ? K7_COMPRESS_OPTION_AUTO : 0;
    if (items[i].Value == selValue)
      list->SelectedIndex = (INT32)i;
  }
  list->Visible = visible ? TRUE : FALSE;
  list->Enabled = enabled ? TRUE : FALSE;
}

static void FillFixedList(PK7_COMPRESS_OPTION_LIST list,
    const UInt32 *langIDs, const int *values, unsigned numItems, int selValue)
{
  list->Count = (UINT32)numItems;
  list->SelectedIndex = -1;
  for (unsigned i = 0; i < numItems; i++)
  {
    PK7_COMPRESS_OPTION_ITEM item = &list->Items[i];
    UString s;
    LangString(langIDs[i], s);
    CopyText(item->DisplayText, K7_COMPRESS_MAX_DISPLAY_TEXT, s);
    item->SemanticText[0] = 0;
    item->Value = values[i];
    item->Flags = 0;
    if (values[i] == selValue)
      list->SelectedIndex = (INT32)i;
  }
  list->Visible = TRUE;
  list->Enabled = TRUE;
}

static void FillVolumes(PK7_COMPRESS_OPTION_LIST list)
{
  static const wchar_t * const kVolumes[] =
  {
      L"10M"
    , L"100M"
    , L"1000M"
    , L"650M - CD"
    , L"700M - CD"
    , L"4092M - FAT"
    , L"4480M - DVD"
    , L"8128M - DVD DL"
    , L"23040M - BD"
  };
  list->Count = (UINT32)Z7_ARRAY_SIZE(kVolumes);
  list->SelectedIndex = -1;
  for (UINT32 i = 0; i < list->Count; i++)
  {
    PK7_COMPRESS_OPTION_ITEM item = &list->Items[i];
    wcscpy_s(item->DisplayText, K7_COMPRESS_MAX_DISPLAY_TEXT, kVolumes[i]);
    item->SemanticText[0] = 0;
    item->Value = (INT64)i;
    item->Flags = 0;
  }
  list->Visible = TRUE;
  list->Enabled = TRUE;
}

static void UpdateSnapshot(CCompressDialogCore &core, PK7_COMPRESS_DIALOG_CONTEXT ctx)
{
  core.UpdateMemoryTexts();
  core.UpdateOptionsSummary();

  FillOptionList(&ctx->Formats, core.FormatItems, (UInt64)core.FormatIndex, true, true);
  FillOptionList(&ctx->Levels, core.LevelItems, core.Level, true, core.LevelItems.Size() > 1);
  FillOptionList(&ctx->Methods, core.MethodItems, (UInt64)(Int64)core.MethodID, true, core.MethodItems.Size() > 1);
  FillOptionList(&ctx->Dictionaries, core.DictionaryItems, core.Dict64, true, core.DictionaryItems.Size() > 1);
  FillOptionList(&ctx->Orders, core.OrderItems, core.Order, true, core.OrderItems.Size() > 1);
  FillOptionList(&ctx->SolidBlocks, core.SolidItems, core.BlockLogSize, core.SolidSupported, core.SolidItems.Size() > 1);
  FillOptionList(&ctx->Threads, core.ThreadItems, core.NumThreads, true, core.ThreadItems.Size() > 1);
  FillOptionList(&ctx->MemoryLimits, core.MemUseItems, (UInt64)(Int64)core.MemUseIndex, core.MemUseSupported, core.MemUseItems.Size() > 1);
  FillFixedList(&ctx->UpdateModes, k_UpdateMode_IDs, k_UpdateMode_Vals, Z7_ARRAY_SIZE(k_UpdateMode_Vals), core.Info.UpdateMode);
  FillFixedList(&ctx->PathModes, k_PathMode_IDs, k_PathMode_Vals, Z7_ARRAY_SIZE(k_PathMode_Vals), core.Info.PathMode);
  FillOptionList(&ctx->EncryptionMethods, core.EncryptionMethodItems, (UInt64)(Int64)core.EncryptionMethodIndex, core.EncryptSupported, core.EncryptionMethodItems.Size() > 1);
  FillVolumes(&ctx->Volumes);

  ctx->FormatIndex = core.FormatIndex;
  ctx->Level = core.Level;
  ctx->Dictionary = core.Dict64;
  ctx->Order = core.Order;
  ctx->SolidBlock = core.BlockLogSize;
  ctx->ThreadsValue = core.NumThreads;
  ctx->MemoryLimit = core.MemUseIndex;
  ctx->UpdateMode = core.Info.UpdateMode;
  ctx->PathMode = core.Info.PathMode;
  ctx->SfxMode = core.SfxChecked;
  ctx->OpenShareForWrite = core.OpenShareForWrite;
  ctx->DeleteAfterCompressing = core.DeleteAfterCompressing;
  ctx->EncryptHeaders = core.EncryptHeadersChecked;
  ctx->ShowPassword = core.ShowPassword;
  ctx->EncryptHeadersAllowed = core.EncryptFileNamesSupported;

  ctx->SfxVisible = TRUE;
  ctx->SfxEnabled = core.SfxSupported;
  ctx->EncryptionVisible = TRUE;
  ctx->EncryptionEnabled = core.EncryptSupported;
  ctx->VolumeVisible = TRUE;
  ctx->ParametersVisible = TRUE;
  ctx->OptionsEnabled = TRUE;
  ctx->MemoryVisible = core.MemUseSupported;
  ctx->DecompressMemoryVisible = core.MemUseSupported;

  CopyText(ctx->ArchiveFolderText, 256, core.DirPrefix);
  CopyText(ctx->HardwareThreadsText, 256, core.HardwareThreadsText);
  CopyText(ctx->MemoryValueText, 256, core.MemoryValueText);
  CopyText(ctx->DecompressMemoryText, 256, core.DecompressMemoryText);
  CopyText(ctx->OptionsSummaryText, 1024, core.OptionsSummaryText);
  CopyText(ctx->ArchivePath, K7_COMPRESS_MAX_ARCHIVE_PATH, core.ArchiveName);
  CopyText(ctx->Parameters, K7_COMPRESS_MAX_PARAMETERS, core.Info.Options);
  CopyText(ctx->VolumeText, K7_COMPRESS_MAX_VOLUME_TEXT, core.VolumeText);
  CopyText(ctx->Password, K7_COMPRESS_MAX_PASSWORD, core.Password);
  CopyText(ctx->PasswordConfirmation, K7_COMPRESS_MAX_PASSWORD, core.PasswordConfirmation);
}

static void ApplyUserText(CCompressDialogCore &core, PK7_COMPRESS_DIALOG_CONTEXT ctx)
{
  core.ArchiveName = ctx->ArchivePath;
  core.Info.Options = ctx->Parameters;
  core.VolumeText = ctx->VolumeText;
  core.Password = ctx->Password;
  core.PasswordConfirmation = ctx->PasswordConfirmation;
  core.ShowPassword = (ctx->ShowPassword != FALSE);
}

static void OnBrowseArchive(CCompressXamlHost *host)
{
  CCompressDialogCore &core = *host->Core;
  PK7_COMPRESS_DIALOG_CONTEXT ctx = host->Context;

  UString path;
  if (!core.GetFinalPath_Smart(path))
  {
    CopyText(ctx->ErrorText, 1024, k_IncorrectPathMessage);
    return;
  }

  int filterIndex;
  CObjectVector<CBrowseFilterInfo> filters;
  unsigned numFormats = 0;

  const bool isSFX = core.IsSfx();
  if (isSFX)
  {
    filterIndex = 0;
    const UString ext ("exe");
    AddFilter(filters, ext, ext);
  }
  else
  {
    numFormats = (unsigned)core.FormatItems.Size();
    filterIndex = core.FindExactIndex(core.FormatItems, (UInt64)core.FormatIndex);

    // filters [0, ... numFormats - 1] corresponds to FormatItems
    UString desc;
    UStringVector masks;
    CStringFinder finder;

    for (unsigned i = 0; i < numFormats; i++)
    {
      const CArcInfoEx &ai = (*core.ArcFormats)[(unsigned)core.FormatItems[i].Value];
      CBrowseFilterInfo &f = filters.AddNew();
      f.Description = ai.Name;
      f.Description += " (";
      bool needSpace_desc = false;

      FOR_VECTOR (k, ai.Exts)
      {
        const UString &ext = ai.Exts[k].Ext;
        UString mask ("*.");
        mask += ext;

        if (finder.FindWord_In_LowCaseAsciiList_NoCase(k_DontSave_Exts, ext))
          continue;

        f.Masks.Add(mask);
        masks.Add(mask);
        if (needSpace_desc)
          f.Description.Add_Space();
        needSpace_desc = true;
        f.Description += ext;
      }
      f.Description += ")";
      if (i != 0)
        desc.Add_Space();
      desc += ai.GetMainExt();
    }

    CBrowseFilterInfo &f = filters.AddNew();
    f.Description = LangString(IDT_COMPRESS_ARCHIVE);
    f.Description.RemoveChar(L'&');
    f.Description += " (";
    f.Description += desc;
    f.Description += ")";
    f.Masks = masks;
  }

  AddFilter(filters, LangString(IDS_OPEN_TYPE_ALL_FILES), UString("*"));
  if (filterIndex < 0)
    filterIndex = (int)filters.Size() - 1;

  const UString title = LangString(IDS_COMPRESS_SET_ARCHIVE_BROWSE);

  // Use the modern save dialog (IFileSaveDialog) instead of the legacy
  // SHBrowseForFolder-based browse: the legacy one renders a white list
  // even in dark mode, and the packaged 7zG process needs the dark-mode
  // bypass so the system dialog follows the app theme.
  K7UserDarkModeWorkaroundBypassScope DarkModeWorkaroundBypass;
  CMyComPtr<IFileSaveDialog> Dialog;
  HRESULT hr = ::CoCreateInstance(
      CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
      IID_IFileSaveDialog, (void **)&Dialog);
  if (FAILED(hr))
    return;

  {
    // Convert the format filters into COMDLG_FILTERSPEC (name + a
    // "*.ext;*.ext" spec), keeping the strings alive in parallel vectors.
    CObjectVector<UString> specNames;
    CObjectVector<UString> specMasks;
    FOR_VECTOR (i, filters)
    {
      UString masks;
      FOR_VECTOR (k, filters[i].Masks)
      {
        if (k != 0)
          masks += L";";
        masks += filters[i].Masks[k];
      }
      specNames.Add(filters[i].Description);
      specMasks.Add(masks);
    }
    if (specNames.Size() > 0)
    {
      std::vector<COMDLG_FILTERSPEC> specs;
      specs.reserve(specNames.Size());
      FOR_VECTOR (i, specNames)
      {
        COMDLG_FILTERSPEC spec;
        spec.pszName = specNames[i].Ptr();
        spec.pszSpec = specMasks[i].Ptr();
        specs.push_back(spec);
      }
      Dialog->SetFileTypes((UINT)specs.size(), specs.data());
      if (filterIndex >= 0 && filterIndex < (int)specs.size())
        Dialog->SetFileTypeIndex((UINT)filterIndex + 1);
    }
  }

  Dialog->SetTitle(title.Ptr());
  if (!path.IsEmpty())
    Dialog->SetFileName(path.Ptr());

  {
    DWORD opts = 0;
    Dialog->GetOptions(&opts);
    opts |= FOS_OVERWRITEPROMPT | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
    Dialog->SetOptions(opts);
  }

  hr = Dialog->Show(host->Parent);
  if (FAILED(hr))
    return; // user cancelled the browse; not an error

  {
    CMyComPtr<IShellItem> item;
    hr = Dialog->GetResult(&item);
    if (FAILED(hr) || !item)
      return;
    LPWSTR pszPath = nullptr;
    hr = item->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
    if (FAILED(hr) || !pszPath)
      return;
    path = pszPath;
    ::CoTaskMemFree(pszPath);
  }

  UINT fileTypeIndex = 0;
  Dialog->GetFileTypeIndex(&fileTypeIndex);
  const int chosenFilterIndex = (fileTypeIndex == 0)
      ? -1 : (int)fileTypeIndex - 1;

  if (isSFX)
  {
    const int dotPos = GetExtDotPos(path);
    if (dotPos >= 0)
      path.DeleteFrom(dotPos);
    path += kExeExt;
  }
  else
  if (chosenFilterIndex >= 0 && (unsigned)chosenFilterIndex < numFormats)
  {
    // archive format was confirmed. So we try to set format extension
    bool needAddExt = true;
    const CArcInfoEx &ai = (*core.ArcFormats)[(unsigned)core.FormatItems[(unsigned)chosenFilterIndex].Value];
    const int dotPos = GetExtDotPos(path);
    if (dotPos >= 0)
    {
      const UString ext = path.Ptr(dotPos + 1);
      if (ai.FindExtension(ext) >= 0)
        needAddExt = false;
    }
    if (needAddExt)
    {
      if (path.IsEmpty() || path.Back() != '.')
        path.Add_Dot();
      path += ai.GetMainExt();
    }
  }

  UString name;
  core.SetArcPathFields(path, name, true);

  if (!isSFX)
  if (chosenFilterIndex >= 0 && (unsigned)chosenFilterIndex < numFormats)
  if (chosenFilterIndex != filterIndex)
  {
    core.OnFormatSelected((int)core.FormatItems[(unsigned)chosenFilterIndex].Value);
    UpdateSnapshot(core, ctx);
    return;
  }

  if (core.ArcPathChanged(path))
    UpdateSnapshot(core, ctx);
  else
    UpdateSnapshot(core, ctx);
}

static BOOLEAN WINAPI CCompressCommandThunk(
    LPVOID callbackContext, UINT32 command, INT64 value, LPCWSTR semanticText)
{
  CCompressXamlHost *host = (CCompressXamlHost *)callbackContext;
  if (!host || !host->Core || !host->Context)
    return FALSE;
  CCompressDialogCore &core = *host->Core;
  PK7_COMPRESS_DIALOG_CONTEXT ctx = host->Context;

  switch (command)
  {
    case K7_COMPRESS_COMMAND_FORMAT:
      core.OnFormatSelected((int)value);
      UpdateSnapshot(core, ctx);
      return TRUE;

    case K7_COMPRESS_COMMAND_LEVEL:
      core.OnLevelSelected((UInt32)value);
      UpdateSnapshot(core, ctx);
      return TRUE;

    case K7_COMPRESS_COMMAND_METHOD:
      core.OnMethodSelected((int)value);
      UpdateSnapshot(core, ctx);
      return TRUE;

    case K7_COMPRESS_COMMAND_DICTIONARY:
      core.OnDictionarySelected((UInt64)value);
      UpdateSnapshot(core, ctx);
      return TRUE;

    case K7_COMPRESS_COMMAND_ORDER:
      core.OnOrderSelected((UInt32)value);
      UpdateSnapshot(core, ctx);
      return TRUE;

    case K7_COMPRESS_COMMAND_SOLID:
      core.OnSolidSelected((UInt32)value);
      UpdateSnapshot(core, ctx);
      return TRUE;

    case K7_COMPRESS_COMMAND_THREADS:
      core.OnThreadsSelected((UInt32)value);
      UpdateSnapshot(core, ctx);
      return TRUE;

    case K7_COMPRESS_COMMAND_MEMORY:
      core.OnMemUseSelected((int)value);
      UpdateSnapshot(core, ctx);
      return TRUE;

    case K7_COMPRESS_COMMAND_UPDATE_MODE:
      core.OnUpdateModeSelected((int)value);
      UpdateSnapshot(core, ctx);
      return TRUE;

    case K7_COMPRESS_COMMAND_PATH_MODE:
      core.OnPathModeSelected((int)value);
      UpdateSnapshot(core, ctx);
      return TRUE;

    case K7_COMPRESS_COMMAND_ENCRYPTION_METHOD:
      core.OnEncryptionMethodSelected((int)value);
      UpdateSnapshot(core, ctx);
      return TRUE;

    case K7_COMPRESS_COMMAND_ARCHIVE_PATH:
      if (semanticText)
        core.ArchiveName = semanticText;
      return TRUE;

    case K7_COMPRESS_COMMAND_PARAMETERS:
      if (semanticText)
        core.Info.Options = semanticText;
      return TRUE;

    case K7_COMPRESS_COMMAND_VOLUME:
      if (semanticText)
        core.VolumeText = semanticText;
      return TRUE;

    case K7_COMPRESS_COMMAND_SFX:
      core.OnSfxChecked(value != 0);
      UpdateSnapshot(core, ctx);
      return TRUE;

    case K7_COMPRESS_COMMAND_SHARED:
      core.OpenShareForWrite = (value != 0);
      return TRUE;

    case K7_COMPRESS_COMMAND_DELETE:
      core.DeleteAfterCompressing = (value != 0);
      return TRUE;

    case K7_COMPRESS_COMMAND_ENCRYPT_HEADERS:
      core.EncryptHeadersChecked = (value != 0);
      return TRUE;

    case K7_COMPRESS_COMMAND_SUBMIT:
    {
      ApplyUserText(core, ctx);
      UString errorMessage;
      CCompressDialogCore::ECommitResult res = core.ValidateAndCommit(errorMessage);
      if (res == CCompressDialogCore::kCommitOk)
        return TRUE;
      if (res == CCompressDialogCore::kCommitNeedVolumeConfirm)
      {
        if (::MessageBoxW(host->Parent, core.VolumeConfirmText,
            // **************** NanaZip Modification Start ****************
            //L"7-Zip", MB_YESNOCANCEL | MB_ICONQUESTION) == IDYES)
            L"NanaZip", MB_YESNOCANCEL | MB_ICONQUESTION) == IDYES)
            // **************** NanaZip Modification End ****************
        {
          core.VolumeConfirmed = true;
          res = core.ValidateAndCommit(errorMessage);
          if (res == CCompressDialogCore::kCommitOk)
            return TRUE;
          if (res == CCompressDialogCore::kCommitBlocked)
          {
            CopyText(ctx->ErrorText, 1024, errorMessage);
            return FALSE;
          }
          // kCommitNeedVolumeConfirm can't happen after VolumeConfirmed
          return TRUE;
        }
        return FALSE;
      }
      // kCommitBlocked
      CopyText(ctx->ErrorText, 1024, errorMessage);
      return FALSE;
    }

    case K7_COMPRESS_COMMAND_BROWSE_ARCHIVE:
      OnBrowseArchive(host);
      return TRUE;
  }

  return TRUE;
}

static BOOLEAN WINAPI CCompressOptionsThunk(LPVOID callbackContext)
{
  CCompressXamlHost *host = (CCompressXamlHost *)callbackContext;
  if (!host || !host->Core || !host->Context)
    return FALSE;
  COptionsDialog dialog(host->Core);
  if (dialog.Create(host->Parent) == IDOK)
    UpdateSnapshot(*host->Core, host->Context);
  return TRUE;
}

static UINT32 ReadFontSizeDialog()
{
  UINT32 pt = 0;
  HKEY key;
  if (::RegOpenKeyExW(HKEY_CURRENT_USER,
      L"Software\\NanaZip\\Options", 0, KEY_READ, &key) == ERROR_SUCCESS)
  {
    DWORD size = sizeof(pt);
    ::RegQueryValueExW(key, L"FontSizeDialog", nullptr, nullptr,
        reinterpret_cast<LPBYTE>(&pt), &size);
    ::RegCloseKey(key);
  }
  return pt;
}

// Archive-path history is stored in a plain file under the packaged app's
// LocalState directory (same location as ExtractHistory.txt): the packaged
// (MSIX) environment isolates registry writes made by the helper process, so
// registry-based history never survives. This mirrors the extract dialog.
static FString GetCompressHistoryFilePath()
{
  FString result;
  wchar_t envBuf[MAX_PATH];
  const DWORD len = ::GetEnvironmentVariableW(
      L"LOCALAPPDATA", envBuf, MAX_PATH);
  if (len == 0 || len >= MAX_PATH)
    return result;
  result = envBuf;
  result += L"\\Packages\\SSS.NanaZip.RemotePassword_t9byekn60qs4j"
      L"\\LocalState\\CompressHistory.txt";
  return result;
}

static void SaveCompressHistoryFile(const UStringVector &paths)
{
  FString path = GetCompressHistoryFilePath();
  if (path.IsEmpty())
    return;
  // One path per line, CRLF, native UTF-16 (wchar_t) bytes.
  size_t total = 1;
  FOR_VECTOR (i, paths)
    total += paths[i].Len() + 2;
  std::vector<wchar_t> buf(total, 0);
  size_t off = 0;
  FOR_VECTOR (i, paths)
  {
    wcscpy_s(&buf[off], total - off, paths[i].Ptr());
    off += paths[i].Len();
    buf[off++] = L'\r';
    buf[off++] = L'\n';
  }
  NWindows::NFile::NIO::COutFile file;
  if (file.Create_ALWAYS(path))
  {
    UInt32 written = 0;
    file.Write(buf.data(), (UInt32)(off * sizeof(wchar_t)), written);
    file.Close();
  }
}

static void LoadCompressHistoryFile(UStringVector &paths)
{
  FString path = GetCompressHistoryFilePath();
  if (path.IsEmpty())
    return;
  NWindows::NFile::NIO::CInFile file;
  if (!file.Open(path))
    return;
  UInt64 size64 = 0;
  if (!file.GetLength(size64) || size64 == 0 || size64 > (1 << 20))
    return;
  std::vector<wchar_t> buf((size_t)(size64 / sizeof(wchar_t)) + 1, 0);
  UInt32 read = 0;
  file.Read(buf.data(), (UInt32)size64, read);
  file.Close();
  UString line;
  const size_t count = read / sizeof(wchar_t);
  for (size_t i = 0; i < count; i++)
  {
    const wchar_t ch = buf[i];
    if (ch == L'\n')
    {
      if (!line.IsEmpty() && line.Back() == L'\r')
        line.DeleteBack();
      if (!line.IsEmpty())
      {
        paths.Add(line);
        if (paths.Size() >= 16)
          break;
      }
      line.Empty();
    }
    else if (ch != 0)
      line += ch;
  }
  if (!line.IsEmpty() && paths.Size() < 16)
  {
    if (line.Back() == L'\r')
      line.DeleteBack();
    if (!line.IsEmpty())
      paths.Add(line);
  }
}

// Temporary diagnostics for the dialog-startup crash investigation. The
// steps are appended to %TEMP%\k7compress_diag.log so we can see where an
// exception aborts the compression dialog. Remove once the crash is fixed.
static void DiagLog(const wchar_t *msg)
{
  wchar_t path[MAX_PATH];
  const DWORD n = ::GetTempPathW(MAX_PATH, path);
  if (n == 0 || n >= MAX_PATH)
    return;
  wcscat_s(path, L"k7compress_diag.log");
  HANDLE h = ::CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ,
      nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h != INVALID_HANDLE_VALUE)
  {
    DWORD written = 0;
    ::WriteFile(h, msg, (DWORD)(wcslen(msg) * sizeof(wchar_t)),
        &written, nullptr);
    ::WriteFile(h, L"\r\n", 4, &written, nullptr);
    ::CloseHandle(h);
  }
}

// Show the XAML compression dialog for one dialog session.
// On success the caller reads core.Info for the committed result.
ECompressXamlResult K7ShowCompressDialogXaml(HWND hwndParent, CCompressDialogCore &core)
{
  DiagLog(L"[U1] K7ShowCompressDialogXaml enter");
  if (!K7ModernAvailable())
    return kXamlNotAvailable;

  core.Initialize();
  core.CalcFormats();
  core.FormatChanged(false);
  {
    UString fileName;
    core.SetArcPathFields(core.Info.ArcPath, fileName, true);
    core.StartDirPrefix = core.DirPrefix;
    core.SetArchiveName(fileName);
  }
  DiagLog(L"[U2] core init done");

  // The context is ~1MB (12 option lists of 128 items plus the big text
  // buffers). It must live on the heap: on the default 1MB thread stack the
  // __chkstk probe in this function's prologue overflows (0xc00000fd) as
  // soon as the dialog path is entered, which crashed 7zG instantly.
  std::unique_ptr<K7_COMPRESS_DIALOG_CONTEXT> ctx(new K7_COMPRESS_DIALOG_CONTEXT());
  CCompressXamlHost host;
  host.Core = &core;
  host.Context = ctx.get();
  host.Parent = hwndParent;

  ctx->CommandCallback = &CCompressCommandThunk;
  ctx->OptionsCallback = &CCompressOptionsThunk;
  ctx->CallbackContext = &host;
  ctx->FontSizeDialog = ReadFontSizeDialog();
  DiagLog(L"[U3] ctx allocated");

  // Load the archive-path history into the context. The XAML page renders
  // the current path first, followed by these entries.
  {
    UStringVector history;
    LoadCompressHistoryFile(history);
    ctx->NumPaths = 0;
    FOR_VECTOR (i, history)
    {
      if (i >= 16)
        break;
      wcscpy_s(ctx->Paths[i], history[i].Ptr());
      ctx->NumPaths = (UInt32)(i + 1);
    }
    ctx->NumRemovedPaths = 0;
  }

  UpdateSnapshot(core, ctx.get());
  DiagLog(L"[U4] UpdateSnapshot done");

  const int ModernResult = ::K7ModernShowCompressDialog(hwndParent, ctx.get());
  DiagLog(L"[U5] K7ModernShowCompressDialog returned");
  if (ModernResult == -1)
  {
    // The XAML dialog could not be shown (window creation, XAML content
    // loading or message loop failed). Treat it as a hard failure so the
    // caller falls back to the original Win32 dialog instead of silently
    // cancelling.
    return kXamlFailed;
  }

  // Apply history removals from the drop-down "x" buttons regardless of
  // whether the dialog was confirmed or cancelled.
  if (ctx->NumRemovedPaths > 0)
  {
    UStringVector history;
    LoadCompressHistoryFile(history);
    for (UINT32 i = 0; i < ctx->NumRemovedPaths && i < 16; i++)
    {
      UString rm = ctx->RemovedPaths[i];
      FOR_VECTOR (j, history)
      {
        if (history[j] == rm)
        {
          history.Delete(j);
          break;
        }
      }
    }
    SaveCompressHistoryFile(history);
  }

  if (!ctx->OK)
    return kXamlCancelled;

  // On OK the page merged the current archive path into ctx->Paths (most
  // recent first, deduplicated, capped at 16); persist it for next time.
  {
    UStringVector merged;
    for (UINT32 i = 0; i < ctx->NumPaths; i++)
      merged.Add(ctx->Paths[i]);
    SaveCompressHistoryFile(merged);
  }

  ApplyUserText(core, ctx.get());
  return kXamlOk;
}
