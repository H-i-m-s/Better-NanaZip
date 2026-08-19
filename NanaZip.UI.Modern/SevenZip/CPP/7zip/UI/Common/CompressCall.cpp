// CompressCall.cpp

#include "StdAfx.h"

#include <wchar.h>

#include "../../../Common/IntToString.h"
#include "../../../Common/MyCom.h"
#include "../../../Common/Random.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileMapping.h"
#include "../../../Windows/MemoryLock.h"
#include "../../../Windows/ProcessUtils.h"
#include "../../../Windows/Synchronization.h"

#include "../FileManager/StringUtils.h"
#include "../FileManager/RegistryUtils.h"

#include <NanaZip.Password.h>

#include <memory>

#include "ZipRegistry.h"
#include "CompressCall.h"

using namespace NWindows;

// Trace of the FileManager side of a batch password session
// (k7batch_session_diag.log, same file as the pipe server logs).
static void SssFmDiagLog(const wchar_t *event)
{
  wchar_t temp[MAX_PATH] = {};
  if (::GetTempPathW(MAX_PATH, temp) == 0)
    return;
  UString path(temp);
  path += L"k7batch_session_diag.log";
  HANDLE h = ::CreateFileW(path, FILE_APPEND_DATA,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return;
  DWORD written = 0;
  ::WriteFile(h, event, (DWORD)(wcslen(event) * sizeof(wchar_t)),
      &written, NULL);
  ::WriteFile(h, L"\r\n", 4, &written, NULL);
  ::CloseHandle(h);
}

#define MY_TRY_BEGIN try {

#define MY_TRY_FINISH } \
  catch(...) { ErrorMessageHRESULT(E_FAIL); return E_FAIL; }

#define MY_TRY_FINISH_VOID } \
  catch(...) { ErrorMessageHRESULT(E_FAIL); }

#define k7zGui  "NanaZip.Universal.Windows.exe"

// 21.07 : we can disable wildcard
// #define ISWITCH_NO_WILDCARD_POSTFIX "w-"
#define ISWITCH_NO_WILDCARD_POSTFIX

#define kShowDialogSwitch  " -ad"
#define kEmailSwitch  " -seml."
#define kArchiveTypeSwitch  " -t"
#define kIncludeSwitch  " -i" ISWITCH_NO_WILDCARD_POSTFIX
#define kArcIncludeSwitches  " -an -ai" ISWITCH_NO_WILDCARD_POSTFIX
#define kHashIncludeSwitches  kIncludeSwitch
#define kStopSwitchParsing  " --"

static NCompression::CInfo m_RegistryInfo;
extern HWND g_HWND;

// Backport from newer 7-Zip ZS temporarily.
UString GetQuotedString(const UString &src)
{
  UString s2 ('\"');
  unsigned bcount = 0;
  wchar_t c; const wchar_t *f = src.Ptr(), *s = f, *b = f;
  // add string considering backslashes before quote (escape them):
  while (1)
  {
    c = *s++;
    switch (c)
    {
      case L'\\':
        // a backslash - save the position and count them up to quote-char or regular char
        if (!bcount) b = s-1;
        bcount++;
      break;
      case L'\0':
        // end of string (it is always quoted, so need to escape backslashes too):
      case L'"':
        // add part before backslash (and unescaped backslashes if some are there):
        s2.AddFrom(f, (unsigned)(s - f - 1));
        f = s;
        if (bcount) {
          // escape backslashes before quote (same count of BS again):
          s2.AddFrom(b, (unsigned)(s - b - 1));
        }
        // done if end of string
        if (c == L'\0') goto done;
        // escape this quote char:
        s2 += L"\\\"";
      break;
      default:
        // a regular character, reset backslash counter
        bcount = 0;
    }
  }
  s2.AddFrom(f, (unsigned)(s - f - 1));
done:
  s2 += '\"';
  return s2;
}

static void ErrorMessage(LPCWSTR message)
{
  MessageBoxW(g_HWND, message, L"NanaZip", MB_ICONERROR | MB_OK);
}

static void ErrorMessageHRESULT(HRESULT res, LPCWSTR s = NULL)
{
  UString s2 = NError::MyFormatMessage(res);
  if (s)
  {
    s2.Add_LF();
    s2 += s;
  }
  ErrorMessage(s2);
}

// Waits for the 7zG process to exit while pumping the message queue:
// without this, the File Manager UI thread blocks in WaitForSingleObject
// and the window goes "Not Responding" whenever 7zG keeps a modal dialog
// (the extract dialog) open for a while.
static void SssWaitWithMessagePump(CProcess &process)
{
  const HANDLE handle = process;
  for (;;)
  {
    const DWORD wait = ::MsgWaitForMultipleObjects(
        1, &handle, FALSE, INFINITE, QS_ALLINPUT);
    if (wait == WAIT_OBJECT_0)
    {
      break; // the process exited
    }
    MSG msg;
    while (::PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
    {
      ::TranslateMessage(&msg);
      ::DispatchMessageW(&msg);
    }
  }
}

static HRESULT Call7zGui(const UString &params,
    // LPCWSTR curDir,
    bool waitFinish,
    NSynchronization::CBaseEvent *event)
{
  UString imageName = fs2us(NWindows::NDLL::GetModuleDirPrefix());
  imageName += k7zGui;

  CProcess process;
  const WRes wres = process.Create(imageName, params, NULL); // curDir);
  if (wres != 0)
  {
    HRESULT hres = HRESULT_FROM_WIN32(wres);
    ErrorMessageHRESULT(hres, imageName);
    return hres;
  }
  if (waitFinish)
    SssWaitWithMessagePump(process);
  else if (event != NULL)
  {
    HANDLE handles[] = { process, *event };
    ::WaitForMultipleObjects(ARRAY_SIZE(handles), handles, FALSE, INFINITE);
    // The event only signals that 7zG parsed the archive map; it may
    // still be extracting every archive (or showing its dialog). Wait for
    // the process to really exit so the batch password session stays
    // alive for all of them - while pumping messages so the File Manager
    // window stays responsive.
    SssWaitWithMessagePump(process);
  }
  return S_OK;
}

static void AddLagePagesSwitch(UString &params)
{
  if (ReadLockMemoryEnable())
  #ifndef UNDER_CE
  if (NSecurity::Get_LargePages_RiskLevel() == 0)
  #endif
    params += " -slp";
}

class CRandNameGenerator
{
  CRandom _random;
public:
  CRandNameGenerator() { _random.Init(); }
  void GenerateName(UString &s, const char *prefix)
  {
    s += prefix;
    s.Add_UInt32((UInt32)(unsigned)_random.Generate());
  }
};

static HRESULT CreateMap(const UStringVector &names,
    CFileMapping &fileMapping, NSynchronization::CManualResetEvent &event,
    UString &params)
{
  size_t totalSize = 1;
  {
    FOR_VECTOR (i, names)
      totalSize += (names[i].Len() + 1);
  }
  totalSize *= sizeof(wchar_t);

  CRandNameGenerator random;

  UString mappingName;
  for (;;)
  {
    random.GenerateName(mappingName, "7zMap");
    const WRes wres = fileMapping.Create(PAGE_READWRITE, totalSize, GetSystemString(mappingName));
    if (fileMapping.IsCreated() && wres == 0)
      break;
    if (wres != ERROR_ALREADY_EXISTS)
      return HRESULT_FROM_WIN32(wres);
    fileMapping.Close();
  }

  UString eventName;
  for (;;)
  {
    random.GenerateName(eventName, "7zEvent");
    const WRes wres = event.CreateWithName(false, GetSystemString(eventName));
    if (event.IsCreated() && wres == 0)
      break;
    if (wres != ERROR_ALREADY_EXISTS)
      return HRESULT_FROM_WIN32(wres);
    event.Close();
  }

  params += '#';
  params += mappingName;
  params += ':';
  char temp[32];
  ConvertUInt64ToString(totalSize, temp);
  params += temp;

  params += ':';
  params += eventName;

  LPVOID data = fileMapping.Map(FILE_MAP_WRITE, 0, totalSize);
  if (!data)
    return E_FAIL;
  CFileUnmapper unmapper(data);
  {
    wchar_t *cur = (wchar_t *)data;
    *cur++ = 0; // it means wchar_t strings (UTF-16 in WIN32)
    FOR_VECTOR (i, names)
    {
      const UString &s = names[i];
      unsigned len = s.Len() + 1;
      wmemcpy(cur, (const wchar_t *)s, len);
      cur += len;
    }
  }
  return S_OK;
}

int FindRegistryFormat(const UString &name)
{
  FOR_VECTOR (i, m_RegistryInfo.Formats)
  {
    const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[i];
    if (name.IsEqualTo_NoCase(GetUnicodeString(fo.FormatID)))
      return i;
  }
  return -1;
}

int FindRegistryFormatAlways(const UString &name)
{
  int index = FindRegistryFormat(name);
  if (index < 0)
  {
    NCompression::CFormatOptions fo;
    fo.FormatID = GetSystemString(name);
    index = m_RegistryInfo.Formats.Add(fo);
  }
  return index;
}

HRESULT CompressFiles(
    const UString &arcPathPrefix,
    const UString &arcName,
    const UString &arcType,
    bool addExtension,
    const UStringVector &names,
    bool email, bool showDialog, bool waitFinish)
{
  MY_TRY_BEGIN
  UString params ('a');

  CFileMapping fileMapping;
  NSynchronization::CManualResetEvent event;
  params += kIncludeSwitch;
  RINOK(CreateMap(names, fileMapping, event, params));

  if (!arcType.IsEmpty() && arcType == L"7z")
  {
    int index;
    params += kArchiveTypeSwitch;
    params += arcType;
    m_RegistryInfo.Load();
    index = FindRegistryFormatAlways(arcType);
    if (index >= 0)
    {
      char temp[64];
      const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];

      if (!fo.Method.IsEmpty())
      {
        params += " -m0=";
        params += fo.Method;
      }

      /* Level = 0 is meaningful */
      if (fo.Level != static_cast<UInt32>(-1))
      {
        params += " -mx=";
        ConvertUInt32ToString(fo.Level, temp);
        params += temp;
      }

      if (fo.Dictionary && fo.Dictionary != static_cast<UInt32>(-1))
      {
        params += " -md=";
        ConvertUInt32ToString(fo.Dictionary, temp);
        params += temp;
        params += "b";
      }

      if (fo.BlockLogSize && fo.BlockLogSize != static_cast<UInt32>(-1))
      {
        params += " -ms=";
        ConvertUInt64ToString(1ULL << fo.BlockLogSize, temp);
        params += temp;
        params += "b";
      }

      if (fo.NumThreads && fo.NumThreads != static_cast<UInt32>(-1))
      {
        params += " -mmt=";
        ConvertUInt32ToString(fo.NumThreads, temp);
        params += temp;
      }

      if (!fo.Options.IsEmpty())
      {
        UStringVector strings;
        SplitString(fo.Options, strings);
        FOR_VECTOR (i, strings)
        {
          params += " -m";
          params += strings[i];
        }
      }
    }
  }

  if (email)
    params += kEmailSwitch;

  if (showDialog)
    params += kShowDialogSwitch;

  AddLagePagesSwitch(params);

  if (arcName.IsEmpty())
    params += " -an";

  if (addExtension)
    params += " -saa";
  else
    params += " -sae";

  params += kStopSwitchParsing;
  params.Add_Space();

  if (!arcName.IsEmpty())
  {
    params += GetQuotedString(
    // #ifdef UNDER_CE
      arcPathPrefix +
    // #endif
    arcName);
  }

  return Call7zGui(params,
      // (arcPathPrefix.IsEmpty()? 0: (LPCWSTR)arcPathPrefix),
      waitFinish, &event);
  MY_TRY_FINISH
}

static void ExtractGroupCommand(const UStringVector &arcPaths, UString &params, bool isHash, bool waitFinish)
{
  AddLagePagesSwitch(params);
  params += (isHash ? kHashIncludeSwitches : kArcIncludeSwitches);
  CFileMapping fileMapping;
  NSynchronization::CManualResetEvent event;
  HRESULT result = CreateMap(arcPaths, fileMapping, event, params);
  if (result == S_OK)
    result = Call7zGui(params, waitFinish, &event);
  if (result != S_OK)
    ErrorMessageHRESULT(result);
}

// **************** NanaZip Modification Start ****************
// Reads the per-batch result file written by 7zG (one UTF-16 line per
// archive: <code>\t<path>, 0 = extracted, 1 = skipped, 2 = failed) and
// shows the summary, naming the archives that were skipped because no
// verifying password was found. The file is deleted after reading.
static void SssShowBatchResultSummary(const UString &sessionId,
    unsigned total)
{
  wchar_t temp[MAX_PATH];
  UString path;
  if (::GetTempPathW(MAX_PATH, temp) != 0)
  {
    path = temp;
    path += L"sss_batch_result_";
    path += sessionId;
    path += L".txt";
  }
  if (path.IsEmpty())
    return;
  HANDLE h = ::CreateFileW(path, GENERIC_READ,
      FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return;
  const DWORD size = ::GetFileSize(h, NULL);
  std::vector<wchar_t> buf((size / sizeof(wchar_t)) + 1, 0);
  DWORD read = 0;
  if (size >= 2)
  {
    ::ReadFile(h, buf.data(), size - (size & 1), &read, NULL);
  }
  ::CloseHandle(h);
  ::DeleteFileW(path);

  unsigned done = 0;
  unsigned skipped = 0;
  unsigned failed = 0;
  UStringVector skippedNames;
  unsigned rows = 0;
  size_t pos = 0;
  while (pos < buf.size() && buf[pos] != 0)
  {
    // One line: <code>\t<path>\n
    const unsigned code = (unsigned)(buf[pos] - L'0');
    size_t tab = pos + 2; // skip "<code>\t"
    size_t lineEnd = tab;
    while (lineEnd < buf.size() &&
        buf[lineEnd] != L'\n' && buf[lineEnd] != 0)
    {
      lineEnd++;
    }
    UString name(buf.data() + tab);
    name.DeleteFrom((unsigned)(lineEnd - tab));
    if (code == 0)
      done++;
    else if (code == 1)
    {
      skipped++;
      // Show only the file name, not the full path.
      int slash = name.ReverseFind(L'\\');
      if (slash < 0)
        slash = name.ReverseFind(L'/');
      skippedNames.Add(name.Ptr(slash < 0 ? 0 : slash + 1));
    }
    else
      failed++;
    rows++;
    if (buf[lineEnd] == 0)
      break;
    pos = lineEnd + 1;
  }
  // Archives with no result row (7zG aborted before handling them) count
  // as failed; the summary still adds up to the submitted total.
  if (rows < total)
    failed += (unsigned)(total - rows);

  UString msg = L"已解压 ";
  msg.Add_UInt32(done);
  msg += L" 个归档";
  if (skipped > 0)
  {
    msg += L"，跳过 ";
    msg.Add_UInt32(skipped);
    msg += L" 个";
  }
  if (failed > 0)
  {
    msg += L"，";
    msg.Add_UInt32(failed);
    msg += L" 个失败";
  }
  if (skipped > 0 && !skippedNames.IsEmpty())
  {
    msg += L"\n\n无密码跳过：\n";
    FOR_VECTOR (i, skippedNames)
    {
      msg += skippedNames[i];
      if (i + 1 < skippedNames.Size())
        msg += L"\n";
    }
  }
  ::MessageBoxW(0, msg, L"批量解压", MB_ICONINFORMATION);
}

// void ExtractArchives(const UStringVector &arcPaths, const UString &outFolder, bool showDialog, bool elimDup, UInt32 writeZone);
void ExtractArchives(const UStringVector &arcPaths, const UString &outFolder, bool showDialog, bool elimDup, UInt32 writeZone, bool smartExtract, bool openFolder, UInt32 overwriteMode, bool waitFinish, bool suppressDelete, bool useDlgState, const UString &releaseBeforeDeleteMarker, const UString &passwordSessionId)
// **************** NanaZip Modification End ****************
{
  MY_TRY_BEGIN
  // Batch password session: when this call extracts more than one archive
  // and automatic lookup is enabled (and no session id was passed in),
  // start a session whose prefetch thread reads the password book once and
  // queries the cloud per archive, and whose pipe serves those candidates
  // to the 7zG process. 7zG never sees passwords on the command line.
  std::unique_ptr<NanaZipPassword::BatchSessionScope> batchScope;
  UString effectiveSessionId = passwordSessionId;
  // Multi-archive calls and non-dialog calls (right-click "extract here")
  // run silently: they get a batch password session when automatic lookup
  // is enabled. A single-archive call that shows the extract dialog keeps
  // the interactive path (dialog + password box) unchanged.
  if (effectiveSessionId.IsEmpty() &&
      (arcPaths.Size() > 1 || !showDialog))
  {
    bool autoQueryCloud = false;
    bool autoMatchLocal = false;
    DWORD matchPriority = 0;
    NanaZipPassword::ReadAutomaticPasswordSettings(
        autoQueryCloud, autoMatchLocal, matchPriority);
    if (autoQueryCloud || autoMatchLocal)
    {
      std::vector<std::wstring> paths;
      for (unsigned i = 0; i < arcPaths.Size(); i++)
        paths.push_back(std::wstring(arcPaths[i].Ptr()));
      batchScope.reset(new NanaZipPassword::BatchSessionScope(paths));
      effectiveSessionId = UString(batchScope->SessionId().c_str());
      SssFmDiagLog(L"[Q4-FM] session created");
    }
  }
  UString params ('x');
  if (!outFolder.IsEmpty())
  {
    params += " -o";
    params += GetQuotedString(outFolder);
  }
  if (elimDup)
    params += " -spe";
  // **************** NanaZip Modification Start ****************
  if (smartExtract)
    params += " -sps";
  if (openFolder)
    params += " -sre";
  // **************** SSS Modification Start ****************
  // Force an overwrite policy for every archive in this call.
  // NOverwriteMode: kOverwrite=1 -> -aoa, kSkip=2 -> -aos, kRename=3 -> -aou.
  if (overwriteMode != (UInt32)(Int32)-1)
  {
    static const wchar_t * const kAoSwitch[] = { L" -aoa", L" -aos", L" -aou" };
    if (overwriteMode >= 1 && overwriteMode <= 3)
      params += kAoSwitch[overwriteMode - 1];
  }
  // Suppress 7zG's own delete-after-extract: the batch file manager deletes
  // every archive together once the whole batch has finished (see
  // CPanel::SssExtractAll in PanelOperations.cpp).
  if (suppressDelete || !releaseBeforeDeleteMarker.IsEmpty())
    params += L" -snd";
  // SSS: carry the previous archive's dialog choices into this one
  // (one-by-one extraction loop; see ExtractGUI.cpp SssReadDlgStateFile).
  if (useDlgState)
    params += L" -ssdlg";
  // SSS: the File Manager's batch password session (prefetched candidates
  // served over a named pipe; 7zG never sees passwords on the command
  // line). Empty for normal extraction.
  if (!effectiveSessionId.IsEmpty())
  {
    params += L" -sssid";
    params += GetQuotedString(effectiveSessionId);
  }
  // A file manager that is currently browsing the archive still owns an
  // archive handle. Give 7zG a per-run marker destination in that case:
  // it records a successful delete request but leaves the source untouched.
  // The file manager releases its archive folder first, then consumes the
  // marker and performs the deletion itself.
  if (!releaseBeforeDeleteMarker.IsEmpty())
  {
    // Match 7-Zip's other string switches (for example, -o"<path>").
    // The generated marker is an absolute temporary path and its quoted
    // value is attached directly to this switch.
    params += L" -srd";
    params += GetQuotedString(releaseBeforeDeleteMarker);
  }
  // **************** SSS Modification End ****************
  // **************** NanaZip Modification End ****************
  if (writeZone != (UInt32)(Int32)-1)
  {
    params += " -snz";
    params.Add_UInt32(writeZone);
  }
  if (showDialog)
    params += kShowDialogSwitch;
  ExtractGroupCommand(arcPaths, params, false, waitFinish);
  SssFmDiagLog(L"[Q4-FM] 7zG finished");
  // batchScope (if any) is destroyed here: the scope destructor stops the
  // session and joins the prefetch/pipe threads. The trace marks each
  // joined stage so a hang can be attributed to the exact thread.
  batchScope.reset();
  SssFmDiagLog(L"[Q4-FM] session cleaned");
  // **************** NanaZip Modification Start ****************
  // Multi-archive dialog path: 7zG wrote one result line per archive
  // (extracted / skipped / failed); show the summary here, naming the
  // archives that were skipped for lack of a verifying password.
  if (showDialog && !effectiveSessionId.IsEmpty())
    SssShowBatchResultSummary(effectiveSessionId, arcPaths.Size());
  else if (!effectiveSessionId.IsEmpty())
  {
    // Silent one-by-one path: the loop in PanelOperations reports its own
    // summary, so just drop the result file written by 7zG.
    wchar_t temp[MAX_PATH];
    UString rp;
    if (::GetTempPathW(MAX_PATH, temp) != 0)
    {
      rp = temp;
      rp += L"sss_batch_result_";
      rp += effectiveSessionId;
      rp += L".txt";
      NFile::NDir::DeleteFileAlways(us2fs(rp));
    }
  }
  // **************** NanaZip Modification End ****************
  MY_TRY_FINISH_VOID
}


void TestArchives(const UStringVector &arcPaths, bool hashMode)
{
  MY_TRY_BEGIN
  UString params ('t');
  if (hashMode)
  {
    params += kArchiveTypeSwitch;
    params += "hash";
  }
  ExtractGroupCommand(arcPaths, params, false, false);
  MY_TRY_FINISH_VOID
}


void CalcChecksum(const UStringVector &paths,
    const UString &methodName,
    const UString &arcPathPrefix,
    const UString &arcFileName)
{
  MY_TRY_BEGIN

  if (!arcFileName.IsEmpty())
  {
    CompressFiles(
      arcPathPrefix,
      arcFileName,
      UString("hash"),
      false, // addExtension,
      paths,
      false, // email,
      false, // showDialog,
      false  // waitFinish
      );
    return;
  }

  UString params ('h');
  if (!methodName.IsEmpty())
  {
    params += " -scrc";
    params += methodName;
    /*
    if (!arcFileName.IsEmpty())
    {
      // not used alternate method of generating file
      params += " -scrf=";
      params += GetQuotedString(arcPathPrefix + arcFileName);
    }
    */
  }
  ExtractGroupCommand(paths, params, true, false);
  MY_TRY_FINISH_VOID
}

void Benchmark(bool totalMode)
{
  MY_TRY_BEGIN
  UString params ('b');
  if (totalMode)
    params += " -mm=*";
  AddLagePagesSwitch(params);
  HRESULT result = Call7zGui(params, false, NULL);
  if (result != S_OK)
    ErrorMessageHRESULT(result);
  MY_TRY_FINISH_VOID
}
