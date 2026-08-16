// ExtractGUI.cpp

#include "StdAfx.h"

#include <vector>

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/FileIO.h"
#include "../../../Windows/Thread.h"

#include "../FileManager/ExtractCallback.h"
#include "../FileManager/FormatUtils.h"
#include "../FileManager/LangUtils.h"
#include "../FileManager/resourceGui.h"
#include "../FileManager/OverwriteDialogRes.h"

#include "../Common/ArchiveExtractCallback.h"
#include "../Common/PropIDUtils.h"

#include "../Explorer/MyMessages.h"

#include "resource2.h"
#include "ExtractRes.h"

#include "ExtractDialog.h"
#include "ExtractGUI.h"
#include "HashGUI.h"

#include "NanaZip.Modern.h"

#include "../FileManager/PropertyNameRes.h"

// **************** SSS Modification Start ****************
#include "../../../Windows/Registry.h"
#include "../../../Windows/Shell.h"
// **************** SSS Modification End ****************

using namespace NWindows;
using namespace NFile;
using namespace NDir;

static const wchar_t * const kIncorrectOutDir = L"Incorrect output directory path";

#ifndef Z7_SFX

static void AddValuePair(UString &s, UINT resourceID, UInt64 value, bool addColon = true)
{
  AddLangString(s, resourceID);
  if (addColon)
    s.Add_Colon();
  s.Add_Space();
  s.Add_UInt64(value);
  s.Add_LF();
}

static void AddSizePair(UString &s, UINT resourceID, UInt64 value)
{
  AddLangString(s, resourceID);
  s += ": ";
  AddSizeValue(s, value);
  s.Add_LF();
}

#endif

class CThreadExtracting: public CProgressThreadVirt
{
  HRESULT ProcessVirt() Z7_override;
public:
  /*
  #ifdef Z7_EXTERNAL_CODECS
  const CExternalCodecs *externalCodecs;
  #endif
  */

  CCodecs *codecs;
  CExtractCallbackImp *ExtractCallbackSpec;
  const CObjectVector<COpenType> *FormatIndices;
  const CIntVector *ExcludedFormatIndices;

  UStringVector *ArchivePaths;
  UStringVector *ArchivePathsFull;
  const NWildcard::CCensorNode *WildcardCensor;
  const CExtractOptions *Options;

  #ifndef Z7_SFX
  CHashBundle *HashBundle;
  virtual void ProcessWasFinished_GuiVirt() Z7_override;
  #endif

  CMyComPtr<IFolderArchiveExtractCallback> FolderArchiveExtractCallback;
  UString Title;

  CPropNameValPairs Pairs;

  // **************** 7-Zip ZS Modification Start ****************
#ifndef Z7_SFX
  FString FirstExtractedPath;
#endif
  // **************** 7-Zip ZS Modification End ****************
};


#ifndef Z7_SFX
void CThreadExtracting::ProcessWasFinished_GuiVirt()
{
  if (HashBundle && !Pairs.IsEmpty())
    ShowHashResults(Pairs, *this);
}
#endif

HRESULT CThreadExtracting::ProcessVirt()
{
  // **************** NanaZip Modification Start ****************
  //CDecompressStat Stat;
  CDecompressStat &Stat = ExtractCallbackSpec->Stat;
  // **************** NanaZip Modification End ****************
  
  #ifndef Z7_SFX
  /*
  if (HashBundle)
    HashBundle->Init();
  */
  #endif

  HRESULT res = Extract(
      /*
      #ifdef Z7_EXTERNAL_CODECS
      externalCodecs,
      #endif
      */
      codecs,
      *FormatIndices, *ExcludedFormatIndices,
      *ArchivePaths, *ArchivePathsFull,
      *WildcardCensor, *Options,
      ExtractCallbackSpec, ExtractCallbackSpec, FolderArchiveExtractCallback,
      #ifndef Z7_SFX
        HashBundle,
      #endif
      FinalMessage.ErrorMessage.Message, Stat);
  
  #ifndef Z7_SFX
  if (res == S_OK && ExtractCallbackSpec->IsOK())
  {
    // **************** 7-Zip ZS Modification Start ****************
    FirstExtractedPath = Stat.FirstExtractedPath;
    // **************** 7-Zip ZS Modification End ****************
    if (HashBundle)
    {
      AddValuePair(Pairs, IDS_ARCHIVES_COLON, Stat.NumArchives);
      AddSizeValuePair(Pairs, IDS_PROP_PACKED_SIZE, Stat.PackSize);
      AddHashBundleRes(Pairs, *HashBundle);
    }
    else if (Options->TestMode)
    {
      UString s;
    
      AddValuePair(s, IDS_ARCHIVES_COLON, Stat.NumArchives, false);
      AddSizePair(s, IDS_PROP_PACKED_SIZE, Stat.PackSize);

      if (Stat.NumFolders != 0)
        AddValuePair(s, IDS_PROP_FOLDERS, Stat.NumFolders);
      AddValuePair(s, IDS_PROP_FILES, Stat.NumFiles);
      AddSizePair(s, IDS_PROP_SIZE, Stat.UnpackSize);
      if (Stat.NumAltStreams != 0)
      {
        s.Add_LF();
        AddValuePair(s, IDS_PROP_NUM_ALT_STREAMS, Stat.NumAltStreams);
        AddSizePair(s, IDS_PROP_ALT_STREAMS_SIZE, Stat.AltStreams_UnpackSize);
      }
      s.Add_LF();
      AddLangString(s, IDS_MESSAGE_NO_ERRORS);
      FinalMessage.OkMessage.Title = Title;
      FinalMessage.OkMessage.Message = s;
    }
  }
  #endif

  return res;
}

// **************** 7-Zip ZS Modification Start ****************
#ifndef Z7_SFX
#include <shlobj_core.h>
static void BrowseToPath(
    bool explore,
    UString &path)
{
  if (explore /* || (GetFileAttributes(path.Ptr()) & FILE_ATTRIBUTE_DIRECTORY)*/) {
    ShellExecute(NULL, L"explore", path.Ptr(), NULL, NULL, SW_SHOW);
  } else {
  #if (NTDDI_VERSION >= NTDDI_WINXP)
    LPITEMIDLIST pidl = ILCreateFromPath(path.Ptr());
    if (pidl) {
      SHOpenFolderAndSelectItems(pidl,0,0,0);
      ILFree(pidl);
    }
  #else
    UString args = L"/n,/select,\"" + path + L"\"";
    ShellExecute(NULL, L"open", L"explorer.exe", args.Ptr(), NULL, SW_SHOW);
  #endif
  }
}
#endif
// **************** 7-Zip ZS Modification End ****************

// **************** SSS Modification Start ****************
// Set by the file manager via -snd: don't delete archives here; the file
// manager deletes every archive of a batch together after all extraction
// has finished (see SssExtractAll in PanelOperations.cpp).
extern bool g_SssNoDelete;
// Set by the file manager via -ssdlg for a one-by-one extraction loop:
// initialize this dialog from the state file written by the previous
// archive's dialog, so the user's per-run choices stay consistent.
extern bool g_SssUseDlgState;

// Path of the per-run dialog state file shared across the archives of a
// one-by-one extraction loop.
static UString SssDlgStateFilePath()
{
  wchar_t temp[MAX_PATH];
  UString p;
  if (::GetTempPathW(MAX_PATH, temp) != 0)
  {
    p = temp;
    p += L"sss_batch_dlg.txt";
  }
  return p;
}

// %TEMP%\sss_batch_del.txt - '1' when the dialog asked to delete the
// archive after extraction. The file manager reads it after every archive
// of a one-by-one loop and deletes all marked archives together at the end
// (single Recycle Bin operation) instead of one-by-one.
static void SssWriteDeleteMark(bool deleteAfter)
{
  wchar_t temp[MAX_PATH];
  if (::GetTempPathW(MAX_PATH, temp) == 0)
    return;
  UString full(temp);
  full += L"sss_batch_del.txt";
  HANDLE h = ::CreateFileW(full, GENERIC_WRITE, FILE_SHARE_READ, NULL,
      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return;
  const wchar_t *mark = deleteAfter ? L"1" : L"0";
  DWORD written = 0;
  ::WriteFile(h, mark, 2 * sizeof(wchar_t), &written, NULL);
  ::CloseHandle(h);
}

// Write the dialog's full state so the next archive of the loop can
// initialize its dialog identically (path, modes, checkboxes, password).
static void SssWriteDlgStateFile(CExtractDialog &dialog)
{
  const UString path = SssDlgStateFilePath();
  if (path.IsEmpty())
    return;
  HANDLE h = ::CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return;
  UString s;
  s += L"Path="; s += dialog.DirPath; s += L"\r\n";
  s += L"PathMode="; s.Add_UInt32((unsigned)dialog.PathMode); s += L"\r\n";
  s += L"OverwriteMode="; s.Add_UInt32((unsigned)dialog.OverwriteMode); s += L"\r\n";
  s += L"ElimDup="; s += (dialog.ElimDup.Val ? L"1" : L"0"); s += L"\r\n";
  #ifndef Z7_SFX
  s += L"NtSecurity="; s += (dialog.NtSecurity.Val ? L"1" : L"0"); s += L"\r\n";
  s += L"OpnTrgFold="; s += (dialog.OpnTrgFold.Val ? L"1" : L"0"); s += L"\r\n";
  #endif
  s += L"OpenFolder="; s += (dialog.OpenFolder.Val ? L"1" : L"0"); s += L"\r\n";
  s += L"DeleteAfterExtract="; s += (dialog.DeleteAfterExtract ? L"1" : L"0"); s += L"\r\n";
  #ifndef Z7_SFX
  s += L"Password="; s += dialog.Password; s += L"\r\n";
  #endif
  DWORD written = 0;
  ::WriteFile(h, s.Ptr(), (DWORD)(s.Len() * sizeof(wchar_t)), &written, NULL);
  ::CloseHandle(h);
}

// Apply the saved dialog state (if any) before the dialog is created.
static void SssReadDlgStateFile(CExtractDialog &dialog)
{
  const UString path = SssDlgStateFilePath();
  if (path.IsEmpty())
    return;
  HANDLE h = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return;
  const DWORD sizeLow = ::GetFileSize(h, NULL);
  if (sizeLow == INVALID_FILE_SIZE || sizeLow == 0 || sizeLow > (1 << 16))
  {
    ::CloseHandle(h);
    return;
  }
  const size_t size = (size_t)sizeLow;
  wchar_t *buf = new wchar_t[size / sizeof(wchar_t) + 1];
  DWORD read = 0;
  ::ReadFile(h, buf, (DWORD)size, &read, NULL);
  ::CloseHandle(h);
  buf[read / sizeof(wchar_t)] = 0;
  UString text(buf);
  delete[] buf;
  unsigned pos = 0;
  while (pos < text.Len())
  {
    unsigned eol = text.Find(L'\r', pos);
    if (eol == (unsigned)-1)
      eol = text.Len();
    const unsigned eq = text.Find(L'=', pos);
    if (eq != (unsigned)-1 && eq < eol)
    {
      const UString key = text.Mid(pos, eq - pos);
      const UString val = text.Mid(eq + 1, eol - eq - 1);
      if (key == L"Path")
        dialog.DirPath = val;
      else if (key == L"PathMode")
      {
        // A forced mode from the command line (-sps / -spf) wins over the
        // saved state.
        if (!dialog.PathMode_Force)
          dialog.PathMode = (NExtract::NPathMode::EEnum)wcstol(val.Ptr(), NULL, 10);
      }
      else if (key == L"OverwriteMode")
      {
        // A forced overwrite mode from the command line (-aoa/-aos/-aou,
        // forwarded by the file manager when the user picked "Yes to All")
        // wins over the saved state.
        if (!dialog.OverwriteMode_Force)
          dialog.OverwriteMode = (NExtract::NOverwriteMode::EEnum)wcstol(val.Ptr(), NULL, 10);
      }
      else if (key == L"ElimDup")
        dialog.ElimDup.Val = (val == L"1");
      #ifndef Z7_SFX
      else if (key == L"NtSecurity")
        dialog.NtSecurity.Val = (val == L"1");
      else if (key == L"OpnTrgFold")
        dialog.OpnTrgFold.Val = (val == L"1");
      #endif
      else if (key == L"OpenFolder")
        dialog.OpenFolder.Val = (val == L"1");
      else if (key == L"DeleteAfterExtract")
        dialog.DeleteAfterExtract = (val == L"1");
      #ifndef Z7_SFX
      else if (key == L"Password")
        dialog.Password = val;
      #endif
    }
    if (eol >= text.Len())
      break;
    pos = eol;
    while (pos < text.Len() && (text[pos] == L'\r' || text[pos] == L'\n'))
      pos++;
  }
}

// Read the "delete archive after extraction" switches from the settings page.
// The settings live in HKCU\Software\NanaZip\FM (written by the file manager
// options dialog). 7zG runs as a separate process, so we read them directly.
static void SssReadDeleteSettings(bool &deleteAfter, bool &deletePermanently)
{
  deleteAfter = false;
  deletePermanently = false;
  NWindows::NRegistry::CKey key;
  if (key.Open(HKEY_CURRENT_USER, L"Software\\NanaZip\\FM", KEY_READ) == ERROR_SUCCESS)
  {
    key.QueryValue(L"DeleteAfterExtract", deleteAfter);
    key.QueryValue(L"DeletePermanently", deletePermanently);
  }
}

// Delete archives after a successful extraction. Only called when every
// archive finished OK (extractCallback->IsOK()).
static void SssDeleteArchivesAfterExtract(const UStringVector &paths, bool permanently)
{
  if (paths.IsEmpty())
    return;
  if (permanently)
  {
    FOR_VECTOR (i, paths)
      NDir::DeleteFileAlways(us2fs(paths[i]));
    return;
  }
  // Move to the Recycle Bin via SHFileOperationW (FOF_ALLOWUNDO).
  size_t total = 1; // final NUL
  FOR_VECTOR (i, paths)
    total += paths[i].Len() + 1;
  wchar_t *buf = new wchar_t[total];
  wchar_t *p = buf;
  FOR_VECTOR (i, paths)
  {
    MyStringCopy(p, paths[i]);
    p += paths[i].Len() + 1;
  }
  *p = 0;
  SHFILEOPSTRUCTW fo;
  memset(&fo, 0, sizeof(fo));
  fo.wFunc = FO_DELETE;
  fo.pFrom = buf;
  fo.fFlags = FOF_ALLOWUNDO;
  ::SHFileOperationW(&fo);
  delete[] buf;
}

// Write a "batch archive finished OK" marker to %TEMP%. The file manager
// deletes the marker before each archive, starts 7zG with -snd (no delete),
// and after 7zG exits checks the marker to know whether the archive really
// finished OK (a failed/cancelled extraction leaves no marker, so its
// archive is never deleted).
static void SssWriteBatchOk()
{
  wchar_t temp[MAX_PATH];
  if (::GetTempPathW(MAX_PATH, temp) == 0)
    return;
  UString full(temp);
  full += L"sss_batch_ok.txt";
  HANDLE h = ::CreateFileW(full, GENERIC_WRITE, FILE_SHARE_READ, NULL,
      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return;
  DWORD written = 0;
  wchar_t ok = L'1';
  ::WriteFile(h, &ok, sizeof(ok), &written, NULL);
  ::CloseHandle(h);
}
// **************** SSS Modification End ****************

// **************** NanaZip Modification Start ****************
// Extract history is stored in a plain file under the packaged app's
// LocalState directory (next to the user's passwords.txt) instead of the
// registry: the packaged (MSIX) environment isolates registry writes made
// by the helper extraction process, so registry-based history never
// survives. A file read/written with ordinary file APIs works everywhere.
static FString GetExtractHistoryFilePath()
{
  FString result;
  wchar_t envBuf[MAX_PATH];
  const DWORD len = ::GetEnvironmentVariableW(
      L"LOCALAPPDATA", envBuf, MAX_PATH);
  if (len == 0 || len >= MAX_PATH)
    return result;
  result = envBuf;
  result += L"\\Packages\\SSS.NanaZip.RemotePassword_t9byekn60qs4j"
      L"\\LocalState\\ExtractHistory.txt";
  return result;
}

static void SaveExtractHistoryFile(const UStringVector &paths)
{
  FString path = GetExtractHistoryFilePath();
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

static void LoadExtractHistoryFile(UStringVector &paths)
{
  FString path = GetExtractHistoryFilePath();
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
// **************** NanaZip Modification End ****************

HRESULT ExtractGUI(
    // DECL_EXTERNAL_CODECS_LOC_VARS
    CCodecs *codecs,
    const CObjectVector<COpenType> &formatIndices,
    const CIntVector &excludedFormatIndices,
    UStringVector &archivePaths,
    UStringVector &archivePathsFull,
    const NWildcard::CCensorNode &wildcardCensor,
    CExtractOptions &options,
    #ifndef Z7_SFX
    CHashBundle *hb,
    #endif
    bool showDialog,
    bool &messageWasDisplayed,
    CExtractCallbackImp *extractCallback,
    HWND hwndParent)
{
  messageWasDisplayed = false;

  CThreadExtracting extracter;
  /*
  #ifdef Z7_EXTERNAL_CODECS
  extracter.externalCodecs = _externalCodecs;
  #endif
  */
  extracter.codecs = codecs;
  extracter.FormatIndices = &formatIndices;
  extracter.ExcludedFormatIndices = &excludedFormatIndices;

  // **************** SSS Modification Start ****************
  bool deleteAfter = false;
  bool deletePermanently = false;
  SssReadDeleteSettings(deleteAfter, deletePermanently);
  // **************** SSS Modification End ****************

  // **************** 7-Zip ZS Modification Start ****************
#ifndef Z7_SFX
  bool OpnTrgFold = false;
#endif
  // **************** 7-Zip ZS Modification End ****************
  if (!options.TestMode)
  {
    FString outputDir = options.OutputDir;
    #ifndef UNDER_CE
    if (outputDir.IsEmpty())
      GetCurrentDir(outputDir);
    #endif
    if (showDialog)
    {
      // **************** SSS Modification Start ****************
      // XAML dialog path. The XAML page only exchanges a snapshot; all
      // registry persistence and the batch (Sss) state file handling stay
      // here, so the behavior matches the Win32 dialog exactly.
      #ifndef Z7_SFX
      // XAML is now the only dialog path (the Win32 fallback was removed).
      // If the XAML infrastructure is unavailable, tell the user instead
      // of silently skipping the dialog.
      if (!K7ModernAvailable())
      {
        ShowErrorMessage(L"Extract dialog (XAML) initialization failed.");
        messageWasDisplayed = true;
        return E_FAIL;
      }
      {
        CExtractDialog dialog; // not created; state exchange only
        NExtract::CInfo xInfo;
        xInfo.Load();
        // History lives in a file (registry is isolated in the packaged
        // environment), so load it over whatever the registry had.
        xInfo.Paths.Clear();
        LoadExtractHistoryFile(xInfo.Paths);

        FString outputDirFullX;
        if (!MyGetFullPathName(outputDir, outputDirFullX))
        {
          ShowErrorMessage(kIncorrectOutDir);
          messageWasDisplayed = true;
          return E_FAIL;
        }
        NName::NormalizeDirPathPrefix(outputDirFullX);

        dialog.DirPath = fs2us(outputDirFullX);
        dialog.OverwriteMode = options.OverwriteMode;
        dialog.OverwriteMode_Force = options.OverwriteMode_Force;
        dialog.PathMode = options.PathMode;
        dialog.PathMode_Force = options.PathMode_Force;
        dialog.ElimDup = options.ElimDup;
        dialog.DeleteAfterExtract = deleteAfter;
        dialog.OpenFolder = options.OpenFolder;
        if (archivePathsFull.Size() == 1)
          dialog.ArcPath = archivePathsFull[0];
        dialog.NtSecurity = options.NtOptions.NtSecurity;
        if (extractCallback->PasswordIsDefined)
          dialog.Password = extractCallback->Password;

        // SSS: one-by-one loop - carry the previous dialog's choices over.
        if (g_SssUseDlgState)
          SssReadDlgStateFile(dialog);

        K7_EXTRACT_DIALOG_CONTEXT ctx = {};
        wcscpy_s(ctx.DirPath, dialog.DirPath.Ptr());
        wcscpy_s(ctx.ArcPath, dialog.ArcPath.Ptr());
        ctx.PathMode = dialog.PathMode;
        ctx.OverwriteMode = dialog.OverwriteMode;
        ctx.PathMode_Force = dialog.PathMode_Force;
        ctx.OverwriteMode_Force = dialog.OverwriteMode_Force;
        ctx.PathModeDefault =
            xInfo.PathMode_Force ? xInfo.PathMode : 0xFFFFFFFF;
        ctx.OverwriteModeDefault =
            xInfo.OverwriteMode_Force ? xInfo.OverwriteMode : 0xFFFFFFFF;
        ctx.ElimDupDef = dialog.ElimDup.Def;
        ctx.ElimDupVal = dialog.ElimDup.Val;
        ctx.ElimDupDef2 = xInfo.ElimDup.Def;
        ctx.ElimDupVal2 = xInfo.ElimDup.Val;
        ctx.NtSecurityDef = dialog.NtSecurity.Def;
        ctx.NtSecurityVal = dialog.NtSecurity.Val;
        ctx.NtSecurityDef2 = xInfo.NtSecurity.Def;
        ctx.NtSecurityVal2 = xInfo.NtSecurity.Val;
        ctx.OpenFolderDef = dialog.OpenFolder.Def;
        ctx.OpenFolderVal = dialog.OpenFolder.Val;
        ctx.OpenFolderDef2 = xInfo.OpenFolder.Def;
        ctx.OpenFolderVal2 = xInfo.OpenFolder.Val;
        // The "auto show password" setting (HKCU\Software\NanaZip\FM\AutoShowPassword)
        // is the parent of the extract dialog's "show password" check box:
        // when it is on, the dialog checks the box by default; otherwise the
        // remembered value (xInfo.ShowPassword.Val) is used. The GetBoolsVal
        // rule is pair1.Def ? pair1.Val : (pair2.Def ? pair2.Val : pair1.Val),
        // so pair1 = (autoShow, TRUE) and pair2 = (TRUE, remembered) yields
        // autoShow ? TRUE : remembered.
        {
          DWORD autoShow = 0;
          HKEY key = nullptr;
          if (::RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\NanaZip\\FM", 0,
              KEY_READ, &key) == ERROR_SUCCESS)
          {
            DWORD size = sizeof(autoShow);
            ::RegQueryValueExW(key, L"AutoShowPassword", nullptr, nullptr,
                reinterpret_cast<LPBYTE>(&autoShow), &size);
            ::RegCloseKey(key);
          }
          ctx.ShowPasswordDef = (autoShow != 0) ? TRUE : FALSE;
          ctx.ShowPasswordVal = TRUE;
          ctx.ShowPasswordDef2 = TRUE;
          ctx.ShowPasswordVal2 = xInfo.ShowPassword.Val ? TRUE : FALSE;
        }
        // The "auto share password" setting is the parent of the dialog's
        // "share password" check box; changing the box in the dialog never
        // writes back to the setting.
        {
          DWORD autoShare = 0;
          HKEY key = nullptr;
          if (::RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\NanaZip\\FM", 0,
              KEY_READ, &key) == ERROR_SUCCESS)
          {
            DWORD size = sizeof(autoShare);
            ::RegQueryValueExW(key, L"AutoSharePassword", nullptr, nullptr,
                reinterpret_cast<LPBYTE>(&autoShare), &size);
            ::RegCloseKey(key);
          }
          ctx.SharePassword = (autoShare != 0) ? TRUE : FALSE;
        }
        ctx.SplitDestDef = FALSE;
        ctx.SplitDestVal = xInfo.SplitDest.Val;
        ctx.SplitDestDef2 = FALSE;
        ctx.SplitDestVal2 = xInfo.SplitDest.Val;
        ctx.DeleteAfterExtract = dialog.DeleteAfterExtract;
        wcscpy_s(ctx.Password, dialog.Password.Ptr());
        ctx.NumPaths = 0;
        FOR_VECTOR (i, xInfo.Paths)
        {
          if (i >= 16)
            break;
          wcscpy_s(ctx.Paths[i], xInfo.Paths[i].Ptr());
          ctx.NumPaths = (UInt32)(i + 1);
        }
        {
          DWORD pt = 0;
          HKEY key = nullptr;
          if (::RegOpenKeyExW(HKEY_CURRENT_USER,
              L"Software\\NanaZip\\Options", 0, KEY_READ, &key) == ERROR_SUCCESS)
          {
            DWORD size = sizeof(pt);
            ::RegQueryValueExW(key, L"FontSizeDialog", nullptr, nullptr,
                reinterpret_cast<LPBYTE>(&pt), &size);
            ::RegCloseKey(key);
          }
          ctx.FontSizeDialog = pt;
        }

        const int modernResult = ::K7ModernShowExtractDialog(hwndParent, &ctx);
        if (modernResult == -1)
        {
          ShowErrorMessage(L"Extract dialog (XAML) initialization failed.");
          messageWasDisplayed = true;
          return E_FAIL;
        }

        // Apply history removals from the drop-down "x" buttons regardless
        // of whether the dialog was confirmed or cancelled.
        if (ctx.NumRemovedPaths > 0)
        {
          for (UINT32 i = 0; i < ctx.NumRemovedPaths && i < 16; i++)
          {
            UString rm = ctx.RemovedPaths[i];
            FOR_VECTOR (j, xInfo.Paths)
            {
              if (xInfo.Paths[j] == rm)
              {
                xInfo.Paths.Delete(j);
                break;
              }
            }
          }
          SaveExtractHistoryFile(xInfo.Paths);
          xInfo.Save();
        }

        if (!ctx.OK)
          return E_ABORT;

        // Write the results back (mirrors the Create/OnOK flow).
        dialog.DirPath = ctx.OutDirPath;
        dialog.PathMode = (NExtract::NPathMode::EEnum)ctx.PathMode;
        dialog.OverwriteMode = (NExtract::NOverwriteMode::EEnum)ctx.OverwriteMode;
        dialog.ElimDup.Def = ctx.ElimDupDef;
        dialog.ElimDup.Val = ctx.ElimDupVal;
        dialog.NtSecurity.Def = ctx.NtSecurityDef;
        dialog.NtSecurity.Val = ctx.NtSecurityVal;
        dialog.OpenFolder.Def = ctx.OpenFolderDef;
        dialog.OpenFolder.Val = ctx.OpenFolderVal;
        dialog.DeleteAfterExtract = ctx.DeleteAfterExtract;
        dialog.Password = ctx.Password;

        outputDir = us2fs(dialog.DirPath);
        options.OverwriteMode = dialog.OverwriteMode;
        options.PathMode = dialog.PathMode;
        options.ElimDup = dialog.ElimDup;
        options.OpenFolder = dialog.OpenFolder;
        deleteAfter = dialog.DeleteAfterExtract;
        OpnTrgFold = false;
        options.NtOptions.NtSecurity = dialog.NtSecurity;
        extractCallback->Password = dialog.Password;
        extractCallback->PasswordIsDefined = !dialog.Password.IsEmpty();

        // SSS: one-by-one loop - remember this dialog's choices.
        if (g_SssUseDlgState)
        {
          SssWriteDlgStateFile(dialog);
          SssWriteDeleteMark(dialog.DeleteAfterExtract);
        }

        // Persist the remembered settings (mirrors CExtractDialog::OnOK).
        if (xInfo.PathMode != dialog.PathMode)
        {
          xInfo.PathMode_Force = true;
          xInfo.PathMode = dialog.PathMode;
        }
        if (!options.OverwriteMode_Force &&
            xInfo.OverwriteMode != dialog.OverwriteMode)
          xInfo.OverwriteMode_Force = true;
        xInfo.OverwriteMode = dialog.OverwriteMode;
        xInfo.ElimDup.Def = ctx.ElimDupDef2;
        xInfo.ElimDup.Val = ctx.ElimDupVal2;
        xInfo.NtSecurity.Def = ctx.NtSecurityDef2;
        xInfo.NtSecurity.Val = ctx.NtSecurityVal2;
        xInfo.OpenFolder.Def = ctx.OpenFolderDef2;
        xInfo.OpenFolder.Val = ctx.OpenFolderVal2;
        if ((ctx.ShowPasswordVal2 != FALSE) != xInfo.ShowPassword.Val)
        {
          xInfo.ShowPassword.Def = true;
          xInfo.ShowPassword.Val = (ctx.ShowPasswordVal2 != FALSE);
        }
        if ((ctx.SplitDestVal2 != FALSE) != xInfo.SplitDest.Val)
        {
          xInfo.SplitDest.Def = true;
          xInfo.SplitDest.Val = (ctx.SplitDestVal2 != FALSE);
        }
        // Put the current extraction folder at the front of the path
        // history (deduplicated, capped at 16 entries) so the drop-down has
        // content next time; then persist like the original dialog does.
        {
          UString newPath = dialog.DirPath;
          UStringVector merged;
          if (!newPath.IsEmpty())
            merged.Add(newPath);
          FOR_VECTOR (i, xInfo.Paths)
          {
            if (xInfo.Paths[i] != newPath)
            {
              merged.Add(xInfo.Paths[i]);
              if (merged.Size() >= 16)
                break;
            }
          }
          xInfo.Paths = merged;
        }
        // Persist the history to the file (the registry path is isolated
        // in the packaged environment; the file is what gets read back).
        SaveExtractHistoryFile(xInfo.Paths);
        xInfo.Save();

      }
      #endif
      // **************** SSS Modification End ****************
    }
    // **************** 7-Zip ZS Modification Start ****************
    // The "Open target folder" checkbox (ZS legacy) is hidden; keep
    // OpnTrgFold false so the built-in browse never triggers.
    // **************** 7-Zip ZS Modification End ****************
    if (!MyGetFullPathName(outputDir, options.OutputDir))
    {
      ShowErrorMessage(kIncorrectOutDir);
      messageWasDisplayed = true;
      return E_FAIL;
    }
    NName::NormalizeDirPathPrefix(options.OutputDir);
    
    /*
    if (!CreateComplexDirectory(options.OutputDir))
    {
      UString s = GetUnicodeString(NError::MyFormatMessage(GetLastError()));
      UString s2 = MyFormatNew(IDS_CANNOT_CREATE_FOLDER,
      #ifdef Z7_LANG
      0x02000603,
      #endif
      options.OutputDir);
      s2.Add_LF();
      s2 += s;
      MyMessageBox(s2);
      return E_FAIL;
    }
    */
  }
  
  UString title = LangString(options.TestMode ? IDS_PROGRESS_TESTING : IDS_PROGRESS_EXTRACTING);

  extracter.Title = title;
  extracter.ExtractCallbackSpec = extractCallback;
  extracter.ExtractCallbackSpec->ProgressDialog = &extracter;
  extracter.FolderArchiveExtractCallback = extractCallback;
  extracter.ExtractCallbackSpec->Init();

  extracter.CompressingMode = false;

  extracter.ArchivePaths = &archivePaths;
  extracter.ArchivePathsFull = &archivePathsFull;
  extracter.WildcardCensor = &wildcardCensor;
  extracter.Options = &options;
  #ifndef Z7_SFX
  extracter.HashBundle = hb;
  #endif

  extracter.IconID = IDI_ICON;

  RINOK(extracter.Create(title, hwndParent))
  messageWasDisplayed = extracter.ThreadFinishedOK && extracter.MessagesDisplayed;
  // **************** 7-Zip ZS Modification Start ****************
#ifndef Z7_SFX
  // browse/navigate to target path:
  if (OpnTrgFold && extracter.Result == S_OK) {
    // obtain path (directory or file) from first extracted:
    UString extrPath = extracter.FirstExtractedPath;
    if (extrPath.IsEmpty()) {
      extrPath = options.OutputDir;
    }
    else
    if (!options.OutputDir.IsEmpty()) {
      // first subpath relative selected in dialog or given by options.OutputDir:
      UString outDir = options.OutputDir;
      if (outDir.Back() != WCHAR_PATH_SEPARATOR)
        outDir += WCHAR_PATH_SEPARATOR;
      extrPath = extracter.FirstExtractedPath;
      if (extrPath.IsPrefixedBy(outDir)) {
        int subIdx = extrPath.Find(WCHAR_PATH_SEPARATOR, outDir.Len());
        if (subIdx != -1) {
          extrPath = extrPath.Left(subIdx-1);
        }
      }
    }
    if (!extrPath.IsEmpty()) {
      BrowseToPath(0 /* showDialog */, extrPath);
    }
  }
#endif
  // **************** 7-Zip ZS Modification End ****************
  // **************** SSS Modification Start ****************
  // Record success for the batch file manager, then delete only when 7zG
  // owns the deletion (dialog mode). In batch mode the file manager passed
  // -snd, so the archives stay until every archive has been extracted and
  // the file manager deletes them all in one shot.
  if (!options.TestMode && extracter.Result == S_OK && extractCallback->IsOK())
  {
    SssWriteBatchOk();
    if (!g_SssNoDelete && deleteAfter)
      SssDeleteArchivesAfterExtract(archivePathsFull, deletePermanently);
  }
  // **************** SSS Modification End ****************
  return extracter.Result;
}
