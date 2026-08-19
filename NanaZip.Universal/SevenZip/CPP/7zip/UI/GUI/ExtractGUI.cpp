// ExtractGUI.cpp

#include "StdAfx.h"

#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <memory>
#include <atomic>
#include <map>
#include <set>

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
#include <NanaZip.Password.h>

// **************** SSS Modification Start ****************
// Set by the File Manager via -sssid<id>: the batch password session this
// 7zG worker belongs to (see SssBatchPasswordMatch below). Declared early
// because the batch callback above uses it.
extern UString g_SssPasswordSessionId;
// Serializes password verification on this process (defined in
// ExtractCallback.cpp): the engine is not safe for concurrent
// Open/Extract passes, so every TestArchivePassword takes this lock.
extern std::mutex &SssBatchMatchMutex();
// **************** SSS Modification End ****************

#include "../FileManager/PropertyNameRes.h"
#include "../Common/OpenArchive.h"

// **************** SSS Modification Start ****************
#include "../../../Windows/Registry.h"
#include "../../../Windows/Shell.h"
// **************** SSS Modification End ****************

using namespace NWindows;
using namespace NFile;
using namespace NDir;

static const wchar_t * const kIncorrectOutDir = L"Incorrect output directory path";

// Temporary extraction-flow diagnostics. Only records control-flow markers,
// candidate indexes, boolean states, and HRESULTs; never records paths,
// passwords, API configuration, or response data.
// Non-static so ExtractCallback.cpp (the batch password callback path) can
// log into the same diagnostic file.
void ExtractFlowDiagLog(const wchar_t *message)
{
  wchar_t path[MAX_PATH] = {};
  const DWORD length = ::GetTempPathW(MAX_PATH, path);
  if (length == 0 || length >= MAX_PATH)
    return;
  wcscat_s(path, L"k7extract_flow_diag.log");
  // FILE_SHARE_WRITE so concurrent writers (the match worker thread and the
  // UI thread) do not silently drop diagnostics.
  HANDLE file = ::CreateFileW(path, FILE_APPEND_DATA,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE)
    return;
  DWORD written = 0;
  ::WriteFile(file, message,
      (DWORD)(wcslen(message) * sizeof(wchar_t)), &written, NULL);
  ::WriteFile(file, L"\r\n", 4, &written, NULL);
  ::CloseHandle(file);
}

static void ExtractFlowDiagLogIndex(const wchar_t *stage, size_t index)
{
  wchar_t message[128] = {};
  swprintf_s(message, L"%s index=%zu", stage, index);
  ExtractFlowDiagLog(message);
}

static void ExtractFlowDiagLogResult(const wchar_t *stage, HRESULT result)
{
  wchar_t message[160] = {};
  swprintf_s(message, L"%s hr=0x%08X", stage,
      static_cast<unsigned>(result));
  ExtractFlowDiagLog(message);
}

struct SssPasswordQueryContext
{
  std::wstring ArchivePath;
  CCodecs *Codecs;
  const CObjectVector<COpenType> *FormatIndices;
  const CIntVector *ExcludedFormatIndices;
  const UStringVector *ArchivePaths;
  const UStringVector *ArchivePathsFull;
  const NWildcard::CCensorNode *WildcardCensor;
  const CExtractOptions *Options;
};

// Throw-away output stream: the engine's normal extract path (testMode=0)
// writes decoded bytes into it and they are simply discarded, so the
// verification decodes real data (CRC is validated) but never touches the
// disk. This intentionally avoids the engine's test-mode path (testMode=1)
// which crashes on this build, while staying zero-side-effect.
struct CNullOutStream final :
    public ISequentialOutStream,
    public CMyUnknownImp
{
  UInt64 Written;

  CNullOutStream():
      Written(0)
  {}

  Z7_COM_UNKNOWN_IMP_1(ISequentialOutStream)
  Z7_COM7F_IMP(Write(const void *data, UInt32 size, UInt32 *processedSize))
};

Z7_COM7F_IMF(CNullOutStream::Write(
    const void * /* data */, UInt32 size, UInt32 *processedSize))
{
  Written += size;
  if (processedSize)
    *processedSize = size;
  return S_OK;
}

// Minimal archive test callback. GetStream returns a discarding stream
// instead of NULL: with a NULL stream the engine switches to its test-mode
// path (testMode=1), which crashes on this build; with a real (discarding)
// stream it takes the normal, well-tested extraction path and simply
// discards the decoded bytes. Zero side effects either way.
struct CTestExtractCallback final :
    public IArchiveExtractCallback,
    public ICryptoGetTextPassword,
    public CMyUnknownImp
{
  UString Password;
  bool PasswordWasAsked;
  bool HadError;

  CTestExtractCallback():
      PasswordWasAsked(false),
      HadError(false)
  {}

  Z7_COM_UNKNOWN_IMP_2(IArchiveExtractCallback, ICryptoGetTextPassword)

  Z7_COM7F_IMP(SetTotal(UInt64 total))
  Z7_COM7F_IMP(SetCompleted(const UInt64 *completeValue))
  Z7_COM7F_IMP(GetStream(
      UInt32 index, ISequentialOutStream **outStream, Int32 askExtractMode))
  Z7_COM7F_IMP(PrepareOperation(Int32 askExtractMode))
  Z7_COM7F_IMP(SetOperationResult(Int32 opRes))
  Z7_COM7F_IMP(CryptoGetTextPassword(BSTR *password))
};

Z7_COM7F_IMF(CTestExtractCallback::SetTotal(UInt64 /* total */))
{
  ExtractFlowDiagLog(L"[C] SetTotal");
  return S_OK;
}

Z7_COM7F_IMF(CTestExtractCallback::SetCompleted(
    const UInt64 * /* completeValue */))
{
  ExtractFlowDiagLog(L"[C] SetCompleted");
  return S_OK;
}

Z7_COM7F_IMF(CTestExtractCallback::GetStream(
    UInt32 /* index */, ISequentialOutStream **outStream,
    Int32 /* askExtractMode */))
{
  ExtractFlowDiagLog(L"[C] GetStream");
  // Return a throw-away stream instead of NULL: with NULL the engine
  // switches to its test-mode path (testMode=1), which crashes on this
  // build; with a real (discarding) stream it takes the normal extraction
  // path and discards the decoded bytes. Zero side effects.
  CMyComPtr<ISequentialOutStream> nullStream = new CNullOutStream;
  *outStream = nullStream.Detach();
  return S_OK;
}

Z7_COM7F_IMF(CTestExtractCallback::PrepareOperation(
    Int32 /* askExtractMode */))
{
  ExtractFlowDiagLog(L"[C] PrepareOperation");
  return S_OK;
}

Z7_COM7F_IMF(CTestExtractCallback::SetOperationResult(Int32 opRes))
{
  ExtractFlowDiagLogResult(L"[C] SetOperationResult", opRes);
  if (opRes != NArchive::NExtract::NOperationResult::kOK)
    HadError = true;
  return S_OK;
}

Z7_COM7F_IMF(CTestExtractCallback::CryptoGetTextPassword(BSTR *password))
{
  ExtractFlowDiagLog(L"[C] CryptoGetTextPassword");
  PasswordWasAsked = true;
  if (Password.IsEmpty())
    return E_ABORT;
  *password = ::SysAllocString(Password.Ptr());
  return (*password ? S_OK : E_OUTOFMEMORY);
}

static bool ArchiveHasEncryptedItems(
    const SssPasswordQueryContext &context)
{
  if (!context.ArchivePathsFull || context.ArchivePathsFull->IsEmpty())
    return false;

  CProgressDialog progress;
  CExtractCallbackImp openCallback;
  openCallback.ProgressDialog = &progress;
  // Supply an empty password without opening a user-facing dialog. For
  // header-encrypted archives Open then records PasswordWasAsked; for content
  // encryption the archive opens and its item flags can be inspected.
  openCallback.PasswordIsDefined = true;
  openCallback.Password.Empty();
  openCallback.TestMode = true;
  openCallback.Init();
  openCallback.PasswordArchivePath = context.ArchivePathsFull->Front();

  CArchiveLink arcLink;
  COpenOptions op;
  #ifndef Z7_SFX
  op.props = NULL;
  #endif
  op.codecs = context.Codecs;
  op.types = context.FormatIndices;
  op.excludedFormats = context.ExcludedFormatIndices;
  op.stdInMode = false;
  op.stream = NULL;
  op.seqStream = NULL;
  op.callback = NULL;
  op.callbackSpec = NULL;
  op.filePath = context.ArchivePathsFull->Front();

  const HRESULT openRes = arcLink.Open_Strict(op, &openCallback);
  if (openRes != S_OK)
  {
    // Header-encrypted archives cannot be enumerated without a password, but
    // the open path tells us that a password was requested. Treat that as
    // encrypted so the password dialog can run its automatic lookup.
    return openCallback.PasswordWasAsked || arcLink.PasswordWasAsked;
  }

  IInArchive *archive = arcLink.GetArchive();
  UInt32 numItems = 0;
  if (!archive || archive->GetNumberOfItems(&numItems) != S_OK)
    return false;
  for (UInt32 i = 0; i < numItems; i++)
  {
    bool encrypted = false;
    if (Archive_GetItemBoolProp(archive, i, kpidEncrypted, encrypted) == S_OK &&
        encrypted)
    {
      return true;
    }
  }
  return false;
}

// Encrypted-content pre-check for the extract dialog, started by the page
// after the dialog is already visible. The archive scan runs on a background
// thread with a snapshot of the open parameters; the outcome is posted back
// as K7_PASSWORD_ENCRYPTION_CHECK_DONE_MESSAGE so the UI thread is never
// blocked while the dialog appears.
static BOOLEAN WINAPI StartEncryptionCheck(
    LPVOID callbackContext,
    HWND notifyWindow)
{
  const SssPasswordQueryContext *context =
      static_cast<const SssPasswordQueryContext *>(callbackContext);
  if (!context || !context->ArchivePathsFull ||
      context->ArchivePathsFull->IsEmpty() || !notifyWindow)
    return FALSE;

  struct SssEncryptionCheckJob
  {
    CCodecs *Codecs = nullptr;
    CObjectVector<COpenType> FormatIndices;
    CIntVector ExcludedFormatIndices;
    UStringVector ArchivePathsFull;
  };
  SssEncryptionCheckJob job;
  job.Codecs = context->Codecs;
  job.FormatIndices = *context->FormatIndices;
  job.ExcludedFormatIndices = *context->ExcludedFormatIndices;
  job.ArchivePathsFull = *context->ArchivePathsFull;

  std::thread([job, notifyWindow]()
  {
    SssPasswordQueryContext ctx = {};
    ctx.Codecs = job.Codecs;
    ctx.FormatIndices = &job.FormatIndices;
    ctx.ExcludedFormatIndices = &job.ExcludedFormatIndices;
    ctx.ArchivePathsFull = &job.ArchivePathsFull;
    const bool hasEncrypted = ArchiveHasEncryptedItems(ctx);
    ::PostMessageW(notifyWindow, K7_PASSWORD_ENCRYPTION_CHECK_DONE_MESSAGE,
        hasEncrypted ? TRUE : FALSE, 0);
  }).detach();
  return TRUE;
}

static bool TestArchivePassword(
    const std::wstring &password,
    const SssPasswordQueryContext &context)
{
  ExtractFlowDiagLog(L"[T] enter");
  if (context.ArchivePaths->IsEmpty())
    return false;

  CProgressDialog progress;
  CExtractCallbackImp openCallback;
  openCallback.ProgressDialog = &progress;
  openCallback.PasswordIsDefined = true;
  openCallback.Password = password.c_str();
  openCallback.TestMode = true;
  openCallback.Init();
  ExtractFlowDiagLog(L"[T] callback ready");

  // Open the archive with the candidate password (head decryption).
  CArchiveLink arcLink;
  COpenOptions op;
  #ifndef Z7_SFX
  op.props = NULL;
  #endif
  op.codecs = context.Codecs;
  op.types = context.FormatIndices;
  op.excludedFormats = context.ExcludedFormatIndices;
  op.stdInMode = false;
  op.stream = NULL;
  op.seqStream = NULL;
  op.callback = NULL;
  op.callbackSpec = NULL;
  op.filePath = context.ArchivePaths->Front();
  ExtractFlowDiagLog(L"[T] calling Open");
  const HRESULT openRes = arcLink.Open_Strict(op, &openCallback);
  ExtractFlowDiagLogResult(L"[T] Open returned", openRes);
  if (openRes != S_OK)
  {
    // Not opened: the candidate cannot be a verified match. Note that a
    // 7z archive may encrypt file data without encrypting the header, so
    // the engine legitimately asks no password during Open; the password
    // is only needed when the encrypted items are extracted below.
    ExtractFlowDiagLog(L"[T] open rejected");
    return false;
  }
  ExtractFlowDiagLog(L"[T] open ok");

  // Test-extract only the encrypted items: they are the verdict for the
  // candidate password. Unencrypted items decode fine with any password
  // and must not influence the result.
  IInArchive *archive = arcLink.GetArchive();
  UInt32 numItems = 0;
  if (archive->GetNumberOfItems(&numItems) != S_OK)
    return false;
  CRecordVector<UInt32> indices;
  for (UInt32 i = 0; i < numItems; i++)
  {
    bool isEncrypted = false;
    if (Archive_GetItemBoolProp(archive, i, kpidEncrypted, isEncrypted) == S_OK &&
        isEncrypted)
    {
      indices.Add(i);
      // Every encrypted item must decode cleanly: an archive may mix
      // per-file passwords (ZIP), so verifying only the first item could
      // accept a password that leaves the other files broken.
    }
  }
  if (indices.IsEmpty())
  {
    // No encrypted content at all: local password matching is meaningless.
    ExtractFlowDiagLog(L"[T] no encrypted items");
    return false;
  }
  // 7-Zip COM 约定：传给引擎的回调必须是堆对象。
  // CMyUnknownImp 初始引用计数为 0，引擎在 Extract 返回前 AddRef/Release
  // 归零后执行 delete this；栈对象会被 delete 栈内存 -> 堆损坏 0xc0000374。
  // （正式提取的 CArchiveExtractCallback 一直是堆对象，所以从不触发。）
  CTestExtractCallback *testCallbackSpec = new CTestExtractCallback;
  CMyComPtr<IArchiveExtractCallback> testCallback = testCallbackSpec;
  testCallbackSpec->Password = password.c_str();
  ExtractFlowDiagLog(L"[T] calling Extract");
  const HRESULT extractRes = archive->Extract(
      &indices[0],
      indices.Size(),
      0, // normal extract path (testMode=0): the engine writes into the
         // discarding stream, so nothing touches disk
      testCallback);
  ExtractFlowDiagLogResult(L"[T] Extract returned", extractRes);
  // The password is correct when every encrypted item decoded cleanly.
  // A wrong password fails CRC/data validation and sets HadError.
  const bool ok = extractRes == S_OK && !testCallbackSpec->HadError;
  ExtractFlowDiagLog(ok ? L"[T] accepted" : L"[T] rejected");
  return ok;
}

// ================= Batch pipeline prefetch + result file =================
// The File Manager prefetches every archive's cloud result in parallel;
// 7zG additionally verifies each archive's local password-book candidates
// on a worker while the previous archive is still extracting (pipeline:
// archive N extracts while N+1's password is being looked up). The verdict
// follows the mixed-mode rule set by the user: both sides done -> local
// wins; cloud done while local is still testing -> cloud; local first ->
// local. A trailing CloudReady flag in the pipe response tells "still in
// flight" apart from "done, no password", so the worker retries only while
// the lookup is actually running.

struct SssPrefetchResult
{
  std::atomic<bool> LocalDone{false}; // password-book candidates all tested
  std::wstring LocalPassword;         // non-empty = local hit (verified)
  std::atomic<bool> CloudDone{false}; // cloud lookup finished
  std::wstring CloudPassword;         // non-empty = cloud hit (verified)
};

static std::mutex g_PrefetchMutex;
static std::condition_variable g_PrefetchCv;
static std::map<std::wstring, std::shared_ptr<SssPrefetchResult>>
    g_PrefetchResults;
static std::atomic<bool> g_PrefetchStopped{false};

// %TEMP%\sss_batch_result_<sessionid>.txt - one line per archive:
// <code>\t<path>, code 0 = extracted ok, 1 = skipped (no verifying
// password), 2 = failed. The File Manager reads it after 7zG exits to show
// the batch summary (and names the skipped archives). Numbers only, no
// password ever reaches this file.
static UString SssBatchResultFilePath()
{
  wchar_t temp[MAX_PATH];
  UString p;
  if (::GetTempPathW(MAX_PATH, temp) != 0)
  {
    p = temp;
    p += L"sss_batch_result_";
    p += g_SssPasswordSessionId;
    p += L".txt";
  }
  return p;
}

static void SssRecordBatchResult(const UString &archivePath, UINT32 code)
{
  if (g_SssPasswordSessionId.IsEmpty())
    return;
  const UString path = SssBatchResultFilePath();
  if (path.IsEmpty())
    return;
  static std::mutex fileMutex;
  std::lock_guard<std::mutex> lock(fileMutex);
  HANDLE h = ::CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
      OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return;
  UString line;
  line.Add_UInt32(code);
  line += L'\t';
  line += archivePath;
  line += L'\n';
  DWORD written = 0;
  ::WriteFile(h, line.Ptr(),
      (DWORD)(line.Len() * sizeof(wchar_t)), &written, NULL);
  ::CloseHandle(h);
}

// Archives that were silently skipped (no verifying password candidate).
// The result file is written here so the loop below never overwrites a
// skip with a "success".
static std::mutex g_BatchSkippedMutex;
static std::set<std::wstring> g_BatchSkippedPaths;

void SssRecordBatchSkip(const UString &archivePath)
{
  if (g_SssPasswordSessionId.IsEmpty())
    return;
  {
    std::lock_guard<std::mutex> lock(g_BatchSkippedMutex);
    g_BatchSkippedPaths.insert(std::wstring(archivePath.Ptr()));
  }
  SssRecordBatchResult(archivePath, 1);
}

// Called by the extraction loop (UI/Common/Extract.cpp) once an archive is
// handled: skipped archives keep their code=1 row, everything else is
// recorded as ok (0) or failed (2).
void SssRecordBatchArchiveResult(const UString &archivePath, bool ok)
{
  if (g_SssPasswordSessionId.IsEmpty())
    return;
  {
    std::lock_guard<std::mutex> lock(g_BatchSkippedMutex);
    if (g_BatchSkippedPaths.find(std::wstring(archivePath.Ptr())) !=
        g_BatchSkippedPaths.end())
    {
      return; // already recorded as skipped
    }
  }
  SssRecordBatchResult(archivePath, ok ? 0 : 2);
}

// Verifies one candidate against one archive. Runs under the batch match
// mutex (same as the extraction thread's password callback) so the 7-Zip
// engine never has two Open/Extract passes on the same process at once.
static bool SssVerifyPassword(const std::wstring &candidate,
    const SssPasswordQueryContext &context)
{
  if (candidate.empty())
    return false;
  std::lock_guard<std::mutex> lock(SssBatchMatchMutex());
  return TestArchivePassword(candidate, context);
}

// Prefetches one archive: asks the session for candidates, verifies local
// password-book candidates while the cloud lookup is still running, and
// stops as soon as a side decides (local hit -> local wins, cloud ready ->
// verify and use cloud; cloud done without a password -> local decides).
static void SssPrefetchOne(const UString &archivePath,
    const SssPasswordQueryContext &baseContext)
{
  auto result = std::make_shared<SssPrefetchResult>();
  {
    std::lock_guard<std::mutex> lock(g_PrefetchMutex);
    g_PrefetchResults[std::wstring(archivePath.Ptr())] = result;
  }

  SssPasswordQueryContext ctx = baseContext; // same codecs, different archive
  UStringVector singlePaths;
  singlePaths.Add(archivePath);
  ctx.ArchivePaths = &singlePaths;
  ctx.ArchivePathsFull = &singlePaths;
  ctx.ArchivePath = std::wstring(archivePath.Ptr());

  std::vector<std::wstring> localCandidates;
  bool haveCandidates = false;
  size_t localIndex = 0;
  bool cloudFinished = false;

  for (;;)
  {
    if (g_PrefetchStopped.load())
      break;

    // Ask the session for this archive's candidates. Once the cloud
    // lookup has finished (ready flag set), never ask again: the result
    // will not change and only the local candidates still need testing.
    if (!cloudFinished)
    {
      NanaZipPassword::BatchCandidates candidates;
      if (!NanaZipPassword::RequestBatchCandidates(
          std::wstring(g_SssPasswordSessionId.Ptr()),
          std::wstring(archivePath.Ptr()),
          300000, candidates))
      {
        break; // pipe gone (session ended) -> stop quietly
      }
      if (!haveCandidates)
      {
        localCandidates = candidates.LocalCandidates;
        haveCandidates = true;
      }
      if (candidates.CloudReady)
      {
        cloudFinished = true;
        if (!candidates.CloudPassword.empty() &&
            SssVerifyPassword(candidates.CloudPassword, ctx))
        {
          result->CloudPassword = candidates.CloudPassword;
        }
        result->CloudDone = true;
        {
          std::lock_guard<std::mutex> lock(g_PrefetchMutex);
          g_PrefetchCv.notify_all();
        }
        if (!result->CloudPassword.empty())
          break; // cloud hit: done (a late local hit must not change it)
        // Cloud lookup finished but no usable password: the local
        // candidates below are the only hope, so keep testing them.
      }
    }

    // Test one local candidate in the gaps between cloud retries (or
    // straight through once the cloud result turned out unusable).
    if (localIndex < localCandidates.size())
    {
      if (SssVerifyPassword(localCandidates[localIndex], ctx))
      {
        result->LocalPassword = localCandidates[localIndex];
        result->LocalDone = true;
        {
          std::lock_guard<std::mutex> lock(g_PrefetchMutex);
          g_PrefetchCv.notify_all();
        }
        break; // local hit: done, a late cloud result must not change it
      }
      ++localIndex;
      if (localIndex >= localCandidates.size() && cloudFinished)
      {
        result->LocalDone = true; // all rejected, cloud unusable -> done
        {
          std::lock_guard<std::mutex> lock(g_PrefetchMutex);
          g_PrefetchCv.notify_all();
        }
        break;
      }
    }
    else if (!result->LocalDone.load())
    {
      result->LocalDone = true; // all local candidates rejected
      {
        std::lock_guard<std::mutex> lock(g_PrefetchMutex);
        g_PrefetchCv.notify_all();
      }
      if (cloudFinished)
        break; // nothing left to wait for
      // otherwise keep waiting for the cloud result below
    }

    if (cloudFinished)
      break; // cloud done (unusable) and locals exhausted -> done
    ::Sleep(150); // short retry interval while the FM prefetch finishes
  }
}

static void SssPrefetchWorker(const UStringVector &archivePathsFull,
    const SssPasswordQueryContext &baseContext)
{
  for (unsigned i = 0; i < archivePathsFull.Size(); ++i)
  {
    if (g_PrefetchStopped.load())
      break;
    SssPrefetchOne(archivePathsFull[i], baseContext);
  }
}

// Batch password callback for the File Manager session (-sssid): consumes
// the prefetched verdict for the current archive (pipeline result, already
// verified) and returns it following MatchPriority. No session request is
// made here; the prefetch worker owns the pipe traffic.
static bool SssBatchPasswordMatch(
    const UString &archivePath,
    LPVOID queryContext,
    UString &password,
    UINT32 &source)
{
  if (g_SssPasswordSessionId.IsEmpty() || archivePath.IsEmpty())
    return false;
  {
    wchar_t diagName[300];
    const wchar_t *slash = wcsrchr(archivePath.Ptr(), L'\\');
    const wchar_t *name = slash ? slash + 1 : archivePath.Ptr();
    swprintf_s(diagName, L"[Q4] match enter %s", name);
    ExtractFlowDiagLog(diagName);
  }

  std::shared_ptr<SssPrefetchResult> result;
  {
    std::lock_guard<std::mutex> lock(g_PrefetchMutex);
    auto it = g_PrefetchResults.find(std::wstring(archivePath.Ptr()));
    if (it != g_PrefetchResults.end())
      result = it->second;
  }
  if (!result)
  {
    // Startup race (the worker creates entries as it goes): prefetch this
    // archive synchronously, then read the verdict.
    const SssPasswordQueryContext *context =
        static_cast<const SssPasswordQueryContext *>(queryContext);
    if (!context)
      return false;
    SssPrefetchOne(archivePath, *context);
    std::lock_guard<std::mutex> lock(g_PrefetchMutex);
    auto it = g_PrefetchResults.find(std::wstring(archivePath.Ptr()));
    if (it != g_PrefetchResults.end())
      result = it->second;
  }
  if (!result)
    return false;

  bool autoQueryCloud = false;
  bool autoMatchLocal = false;
  DWORD matchPriority = 0;
  NanaZipPassword::ReadAutomaticPasswordSettings(
      autoQueryCloud, autoMatchLocal, matchPriority);

  auto waitFor = [&](const std::atomic<bool> &flag) -> bool
  {
    std::unique_lock<std::mutex> lock(g_PrefetchMutex);
    g_PrefetchCv.wait(lock, [&]()
    {
      return g_PrefetchStopped.load() || flag.load();
    });
    return !g_PrefetchStopped.load();
  };

  if (matchPriority == 1)
  {
    // Cloud first: wait for the cloud verdict, then the book.
    if (!waitFor(result->CloudDone))
      return false;
    if (!result->CloudPassword.empty())
    {
      password = result->CloudPassword.c_str();
      source = (UINT32)NanaZipPassword::PasswordSource::Cloud;
      return true;
    }
    if (!waitFor(result->LocalDone))
      return false;
    if (!result->LocalPassword.empty())
    {
      password = result->LocalPassword.c_str();
      source = (UINT32)NanaZipPassword::PasswordSource::Local;
      return true;
    }
    return false;
  }

  if (matchPriority == 2)
  {
    // Mixed (user rule): both done -> local wins; cloud done while local
    // is still testing -> cloud; local first -> local. When the cloud
    // result turned out unusable (empty), the local verdict is the only
    // hope, so keep waiting for it instead of skipping early.
    {
      std::unique_lock<std::mutex> lock(g_PrefetchMutex);
      g_PrefetchCv.wait(lock, [&]()
      {
        return g_PrefetchStopped.load() ||
            result->LocalDone.load() || result->CloudDone.load();
      });
    }
    if (g_PrefetchStopped.load())
      return false;
    if (!result->LocalPassword.empty())
    {
      password = result->LocalPassword.c_str();
      source = (UINT32)NanaZipPassword::PasswordSource::Local;
      return true;
    }
    if (!result->CloudPassword.empty())
    {
      password = result->CloudPassword.c_str();
      source = (UINT32)NanaZipPassword::PasswordSource::Cloud;
      return true;
    }
    if (!result->LocalDone.load())
    {
      // Cloud done but unusable while local is still testing: the local
      // candidates are the only chance, wait for their verdict.
      if (!waitFor(result->LocalDone))
        return false;
      if (!result->LocalPassword.empty())
      {
        password = result->LocalPassword.c_str();
        source = (UINT32)NanaZipPassword::PasswordSource::Local;
        return true;
      }
    }
    ExtractFlowDiagLog(L"[Q4] no candidate matched (mixed)");
    return false;
  }

  // Local first (default): the password book, then the cloud result.
  if (!waitFor(result->LocalDone))
    return false;
  if (!result->LocalPassword.empty())
  {
    password = result->LocalPassword.c_str();
    source = (UINT32)NanaZipPassword::PasswordSource::Local;
    return true;
  }
  if (!waitFor(result->CloudDone))
    return false;
  if (!result->CloudPassword.empty())
  {
    password = result->CloudPassword.c_str();
    source = (UINT32)NanaZipPassword::PasswordSource::Cloud;
    return true;
  }
  return false;
}

// Async local password match. The XAML page starts it through
// QueryPasswordForDialog (Source == 2) and receives the outcome as
// K7_PASSWORD_MATCH_DONE_MESSAGE posted to the dialog window, so no nested
// modal window is ever created (nested XAML ContentWindow message loops are
// unsafe on this architecture). Every request is an independent task with its
// own RequestId; the result is routed back by that id, so closing a dialog
// and reopening one never blocks a new request and stale results are ignored
// by the page that no longer owns them.
struct SssLocalPasswordMatchJob
{
  CCodecs *Codecs = nullptr;
  CObjectVector<COpenType> FormatIndices;
  CIntVector ExcludedFormatIndices;
  UStringVector ArchivePaths;
  UStringVector ArchivePathsFull;
  CExtractOptions Options;
};

struct SssLocalPasswordMatchTask
{
  std::atomic<bool> Cancelled{false};
  SssLocalPasswordMatchJob Job;
  HWND NotifyWindow = NULL;
};

static std::atomic<UINT64> g_NextPasswordRequestId{1};
static std::mutex g_PasswordTasksMutex;
static std::map<UINT64, std::shared_ptr<SssLocalPasswordMatchTask>>
    g_PasswordTasks;

static void LocalPasswordMatchWorker(
    std::shared_ptr<SssLocalPasswordMatchTask> task,
    UINT64 requestId)
{
  int status = K7_PASSWORD_MATCH_STATUS_NOMATCH;
  std::wstring acceptedPassword;
  try
  {
    const SssLocalPasswordMatchJob job = task->Job;

    // CCensorNode is not copyable; rebuild the equivalent "include
    // everything" wildcard so the worker never touches the caller's censor.
    NWildcard::CCensorNode wildcardCensor;
    wildcardCensor.Add_Wildcard();

    bool accepted = false;
    bool cancelled = false;
    std::vector<NanaZipPassword::Candidate> candidates;
    const bool candidatesLoaded =
        NanaZipPassword::LoadLocalCandidates(candidates) &&
        !candidates.empty();
    if (candidatesLoaded)
    {
      for (size_t i = 0; i < candidates.size(); ++i)
      {
        if (task->Cancelled.load())
        {
          cancelled = true;
          ExtractFlowDiagLog(L"[Q3] match cancelled");
          break;
        }
        ExtractFlowDiagLogIndex(L"[Q3] local candidate begin", i);
        SssPasswordQueryContext ctx;
        ctx.ArchivePath.clear();
        ctx.Codecs = job.Codecs;
        ctx.FormatIndices = &job.FormatIndices;
        ctx.ExcludedFormatIndices = &job.ExcludedFormatIndices;
        ctx.ArchivePaths = &job.ArchivePaths;
        ctx.ArchivePathsFull = &job.ArchivePathsFull;
        ctx.WildcardCensor = &wildcardCensor;
        ctx.Options = &job.Options;
        if (TestArchivePassword(candidates[i].Value, ctx))
        {
          accepted = true;
          acceptedPassword = candidates[i].Value;
          ExtractFlowDiagLog(L"[Q3] local candidate accepted");
          break;
        }
        ExtractFlowDiagLog(L"[Q3] local candidate rejected");
      }
    }
    else
    {
      ExtractFlowDiagLog(L"[Q3] local candidates empty");
    }

    if (!accepted && !cancelled)
      cancelled = task->Cancelled.load();

    status = accepted
        ? K7_PASSWORD_MATCH_STATUS_MATCHED
        : (cancelled ? K7_PASSWORD_MATCH_STATUS_CANCELLED
                     : K7_PASSWORD_MATCH_STATUS_NOMATCH);
  }
  catch (...)
  {
    // The 7-Zip engine can throw (bad archive, allocation failure, ...).
    // A bare std::thread would terminate the whole process on an uncaught
    // exception, so everything is guarded and the page is told "no match"
    // instead of crashing.
    ExtractFlowDiagLog(L"[Q3] match worker exception");
    status = K7_PASSWORD_MATCH_STATUS_NOMATCH;
  }

  K7_PASSWORD_MATCH_RESULT *result =
      new (std::nothrow) K7_PASSWORD_MATCH_RESULT{};
  if (result)
  {
    result->RequestId = requestId;
    result->Status = static_cast<UINT32>(status);
    result->Source = K7_PASSWORD_QUERY_SOURCE_LOCAL;
    if (status == K7_PASSWORD_MATCH_STATUS_MATCHED)
    {
      wcsncpy_s(result->Password, K7_PASSWORD_MAX_PASSWORD_LENGTH,
          acceptedPassword.c_str(), _TRUNCATE);
    }
    ExtractFlowDiagLogResult(L"[Q3] match worker notify", status);
    // PostMessage fails silently when the dialog was already destroyed; the
    // heap block is then released here instead of leaking.
    if (!::PostMessageW(task->NotifyWindow, K7_PASSWORD_MATCH_DONE_MESSAGE,
        0, reinterpret_cast<LPARAM>(result)))
    {
      delete result;
    }
  }

  // The task is done; drop it from the table so a later cancel is a no-op
  // and the request id can never collide with a future request.
  {
    std::lock_guard<std::mutex> lock(g_PasswordTasksMutex);
    g_PasswordTasks.erase(requestId);
  }
}

static VOID WINAPI CancelLocalPasswordMatch(LPVOID, UINT64 requestId)
{
  std::lock_guard<std::mutex> lock(g_PasswordTasksMutex);
  const auto it = g_PasswordTasks.find(requestId);
  if (it != g_PasswordTasks.end())
    it->second->Cancelled = true;
}

// Async cloud lookup. The WinHTTP request can take seconds when the
// endpoint is unreachable or the API key is wrong, so it must never run on
// the UI thread. The outcome is posted back as
// K7_PASSWORD_MATCH_DONE_MESSAGE with the same request id, exactly like the
// local match path, so the dialog appears and stays responsive while the
// request is in flight.
static void CloudPasswordQueryWorker(
    std::wstring archivePath,
    UINT64 requestId,
    HWND notifyWindow)
{
  int status = K7_PASSWORD_MATCH_STATUS_NOMATCH;
  std::wstring value;
  try
  {
    if (NanaZipPassword::QueryCloudPassword(archivePath, value))
      status = K7_PASSWORD_MATCH_STATUS_MATCHED;
  }
  catch (...)
  {
    // Malformed config or engine exception: report "no match" instead of
    // terminating the process from a bare std::thread.
    ExtractFlowDiagLog(L"[Q3] cloud worker exception");
    status = K7_PASSWORD_MATCH_STATUS_NOMATCH;
  }

  K7_PASSWORD_MATCH_RESULT *result =
      new (std::nothrow) K7_PASSWORD_MATCH_RESULT{};
  if (result)
  {
    result->RequestId = requestId;
    result->Status = static_cast<UINT32>(status);
    result->Source = K7_PASSWORD_QUERY_SOURCE_CLOUD;
    if (status == K7_PASSWORD_MATCH_STATUS_MATCHED)
    {
      wcsncpy_s(result->Password, K7_PASSWORD_MAX_PASSWORD_LENGTH,
          value.c_str(), _TRUNCATE);
    }
    // PostMessage fails silently when the dialog was already destroyed; the
    // heap block is then released here instead of leaking.
    if (!::PostMessageW(notifyWindow, K7_PASSWORD_MATCH_DONE_MESSAGE,
        0, reinterpret_cast<LPARAM>(result)))
    {
      delete result;
    }
  }
}

static UINT32 WINAPI QueryPasswordForDialog(
    LPCWSTR archivePath,
    UINT32 source,
    LPVOID callbackContext,
    HWND notifyWindow,
    UINT64 *requestId,
    LPWSTR password,
    UINT32 passwordCapacity)
{
  if (!password || passwordCapacity == 0 || !requestId)
    return K7_PASSWORD_QUERY_RESULT_NOT_FOUND;
  *requestId = 0;

  const SssPasswordQueryContext *context =
      static_cast<const SssPasswordQueryContext *>(callbackContext);
  const std::wstring path = context && !context->ArchivePath.empty()
      ? context->ArchivePath
      : (archivePath ? archivePath : L"");
  if (path.empty())
    return K7_PASSWORD_QUERY_RESULT_NOT_FOUND;

  if (source == K7_PASSWORD_QUERY_SOURCE_CLOUD)
  {
    // Async cloud lookup: the network request runs on a worker thread so
    // the dialog appears and stays responsive while it is in flight. The
    // outcome arrives through K7_PASSWORD_MATCH_DONE_MESSAGE with the same
    // request id, used by both the manual button and the automatic lookup.
    if (!notifyWindow)
      return K7_PASSWORD_QUERY_RESULT_NOT_FOUND;
    const UINT64 id = g_NextPasswordRequestId.fetch_add(1);
    *requestId = id;
    std::thread(CloudPasswordQueryWorker, path, id, notifyWindow).detach();
    return K7_PASSWORD_QUERY_RESULT_PENDING;
  }

  if (source == K7_PASSWORD_QUERY_SOURCE_LOCAL)
  {
    if (!context || !notifyWindow)
      return K7_PASSWORD_QUERY_RESULT_NOT_FOUND;

    // Every request gets its own task and request id; a new dialog can always
    // start a fresh match even when an older task is still finishing, and the
    // result is routed back by request id instead of by "the latest window".
    auto task = std::make_shared<SssLocalPasswordMatchTask>();
    task->NotifyWindow = notifyWindow;
    task->Job.Codecs = context->Codecs;
    task->Job.FormatIndices = *context->FormatIndices;
    task->Job.ExcludedFormatIndices = *context->ExcludedFormatIndices;
    task->Job.ArchivePaths = *context->ArchivePaths;
    task->Job.ArchivePathsFull = *context->ArchivePathsFull;
    task->Job.Options = *context->Options;

    const UINT64 id = g_NextPasswordRequestId.fetch_add(1);
    {
      std::lock_guard<std::mutex> lock(g_PasswordTasksMutex);
      g_PasswordTasks[id] = task;
    }
    *requestId = id;
    ExtractFlowDiagLog(L"[Q3] local match started async");
    std::thread(LocalPasswordMatchWorker, task, id).detach();
    return K7_PASSWORD_QUERY_RESULT_PENDING;
  }

  return K7_PASSWORD_QUERY_RESULT_NOT_FOUND;
}

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

static bool TryAutomaticPasswordCandidates(
    CCodecs *codecs,
    const CObjectVector<COpenType> &formatIndices,
    const CIntVector &excludedFormatIndices,
    const UStringVector &archivePaths,
    const UStringVector &archivePathsFull,
    const NWildcard::CCensorNode &wildcardCensor,
    const CExtractOptions &options,
    CExtractCallbackImp *extractCallback,
    bool showDialog)
{
  ExtractFlowDiagLog(L"[F2] automatic candidate phase enter");
  // In dialog mode, an empty password must reach the formal extraction
  // callback immediately so that CryptoGetTextPassword can show the password
  // dialog. The XAML extraction page still provides explicit local/cloud
  // lookup actions before confirmation.
  if (showDialog)
  {
    ExtractFlowDiagLog(L"[F2] automatic candidate phase skipped dialog mode");
    return false;
  }
  if (archivePathsFull.Size() != 1 || options.TestMode ||
      extractCallback->PasswordIsDefined)
  {
    ExtractFlowDiagLog(L"[F2] automatic candidate phase skipped");
    return false;
  }

  std::vector<NanaZipPassword::Candidate> candidates;
  NanaZipPassword::BuildAutomaticCandidates(
      std::wstring(archivePathsFull[0].Ptr()), candidates);
  ExtractFlowDiagLogIndex(L"[F2] candidates built count", candidates.size());
  for (size_t i = 0; i < candidates.size(); ++i)
  {
    ExtractFlowDiagLogIndex(L"[F2] candidate begin", i);
    CExtractOptions testOptions = options;
    testOptions.TestMode = true;
    testOptions.OpenFolder.Val = false;
    UStringVector testPaths = archivePaths;
    UStringVector testPathsFull = archivePathsFull;
    CProgressDialog progress;
    CExtractCallbackImp testCallback;
    testCallback.ProgressDialog = &progress;
    testCallback.PasswordIsDefined = true;
    testCallback.Password = candidates[i].Value.c_str();
    testCallback.TestMode = true;
    testCallback.Init();
    UString errorMessage;
    CDecompressStat stat;
    const HRESULT result = Extract(
        codecs,
        formatIndices,
        excludedFormatIndices,
        testPaths,
        testPathsFull,
        wildcardCensor,
        testOptions,
        &testCallback,
        &testCallback,
        &testCallback,
        #ifndef Z7_SFX
        NULL,
        #endif
        errorMessage,
        stat);
    ExtractFlowDiagLogResult(L"[F2] candidate extract returned", result);
    if (result == S_OK && testCallback.IsOK())
    {
      if (!testCallback.PasswordWasAsked ||
          !testCallback.EncryptedFileWasVerified)
      {
        ExtractFlowDiagLog(L"[F2] candidate rejected no verified content");
        return false; // no password-protected archive content was verified
      }
      extractCallback->Password = candidates[i].Value.c_str();
      extractCallback->PasswordIsDefined = true;
      extractCallback->PasswordSource = candidates[i].Source;
      extractCallback->PasswordArchivePath = archivePathsFull[0];
      ExtractFlowDiagLog(L"[F2] candidate accepted");
      return true;
    }
    ExtractFlowDiagLog(L"[F2] candidate rejected");
  }
  ExtractFlowDiagLog(L"[F2] automatic candidate phase exhausted");
  return false;
}

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
// Set by the File Manager via -srd<path> when it is browsing the source
// archive. A successful delete request is recorded at this path so the
// File Manager can release its archive handle before deleting the source.
extern UString g_SssReleaseBeforeDeleteMarker;

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

// The marker contains the deletion mode selected in the extract dialog:
// '1' for permanent deletion, '0' for Recycle Bin. 7zG writes it only
// after a fully successful extraction; the File Manager consumes it after
// it has released the archive it is currently browsing.
static void SssWriteReleaseBeforeDeleteMarker(bool permanently)
{
  if (g_SssReleaseBeforeDeleteMarker.IsEmpty())
    return;
  HANDLE h = ::CreateFileW(
      g_SssReleaseBeforeDeleteMarker.Ptr(),
      GENERIC_WRITE,
      FILE_SHARE_READ,
      NULL,
      CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL,
      NULL);
  if (h == INVALID_HANDLE_VALUE)
    return;
  const wchar_t marker = permanently ? L'1' : L'0';
  DWORD written = 0;
  ::WriteFile(h, &marker, sizeof(marker), &written, NULL);
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
  ExtractFlowDiagLog(L"[F1] ExtractGUI enter");
  ExtractFlowDiagLog(showDialog
      ? L"[F1] showDialog=true"
      : L"[F1] showDialog=false");
  ExtractFlowDiagLog(options.TestMode
      ? L"[F1] testMode=true"
      : L"[F1] testMode=false");

  // The automatic candidate test is deliberately performed after the
  // optional extract dialog. In -ad mode, running a full test extraction on
  // the UI thread before showing the dialog makes the dialog appear hung.
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
  SssPasswordQueryContext queryContext = {};
  queryContext.ArchivePath = archivePathsFull.Size() == 1
      ? std::wstring(archivePathsFull[0].Ptr()) : std::wstring();
  queryContext.Codecs = codecs;
  queryContext.FormatIndices = &formatIndices;
  queryContext.ExcludedFormatIndices = &excludedFormatIndices;
  queryContext.ArchivePaths = &archivePaths;
  queryContext.ArchivePathsFull = &archivePathsFull;
  queryContext.WildcardCensor = &wildcardCensor;
  queryContext.Options = &options;
#ifdef NANAZIP_MODERN
  extractCallback->PasswordQueryCallback = QueryPasswordForDialog;
  extractCallback->PasswordQueryContext = &queryContext;
  extractCallback->PasswordQueryCancelCallback = CancelLocalPasswordMatch;
  // Batch password session (File Manager prefetch, -sssid): when set, the
  // password callback asks the session for candidates instead of showing
  // the password dialog, and skips the archive when none verifies.
  extractCallback->PasswordSessionId = g_SssPasswordSessionId;
  extractCallback->BatchPasswordMatchCallback = SssBatchPasswordMatch;
#endif
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
      ExtractFlowDiagLog(K7ModernAvailable()
          ? L"[F3] K7ModernAvailable=true"
          : L"[F3] K7ModernAvailable=false");
      if (!K7ModernAvailable())
      {
        ExtractFlowDiagLog(L"[F3] unavailable branch");
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
        wcsncpy_s(ctx.DirPath, dialog.DirPath.Ptr(), _TRUNCATE);
        wcsncpy_s(ctx.ArcPath, dialog.ArcPath.Ptr(), _TRUNCATE);
        ctx.QueryCallback = QueryPasswordForDialog;
        ctx.QueryContext = &queryContext;
        ctx.QueryIsAsync = TRUE;
        ctx.QueryCancelCallback = CancelLocalPasswordMatch;
        bool autoQueryCloud = false;
        bool autoMatchLocal = false;
        DWORD matchPriority = 0;
        NanaZipPassword::ReadAutomaticPasswordSettings(
            autoQueryCloud, autoMatchLocal, matchPriority);
        ctx.AutoQueryCloud = autoQueryCloud ? TRUE : FALSE;
        ctx.AutoMatchLocal = autoMatchLocal ? TRUE : FALSE;
        ctx.MatchPriority = matchPriority;
        ctx.PasswordSource = extractCallback->PasswordSource == NanaZipPassword::PasswordSource::Cloud ? 1
            : (extractCallback->PasswordSource == NanaZipPassword::PasswordSource::Local ? 2
            : (extractCallback->PasswordSource == NanaZipPassword::PasswordSource::CommandLine ? 3 : 0));
        // The encrypted-content pre-check runs after the dialog is shown so
        // the dialog appears immediately; the page starts it through this
        // callback and receives the outcome as a window message.
        ctx.EncryptionCheckCallback = StartEncryptionCheck;
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

        ExtractFlowDiagLog(L"[F4] calling K7ModernShowExtractDialog");
        const int modernResult = ::K7ModernShowExtractDialog(hwndParent, &ctx);
        ExtractFlowDiagLogResult(L"[F4] K7ModernShowExtractDialog returned",
            modernResult);
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
        extractCallback->SharePasswordAuthorized = ctx.SharePassword != FALSE;
        extractCallback->PasswordSource = dialog.Password.IsEmpty()
            ? NanaZipPassword::PasswordSource::None
            : (ctx.PasswordSource == 1 ? NanaZipPassword::PasswordSource::Cloud
            : (ctx.PasswordSource == 2 ? NanaZipPassword::PasswordSource::Local
            : (ctx.PasswordSource == 3 ? NanaZipPassword::PasswordSource::CommandLine
            : NanaZipPassword::PasswordSource::Manual)));
        extractCallback->PasswordArchivePath = archivePathsFull.Size() == 1
            ? us2fs(archivePathsFull[0]) : FString();

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

    // Automatic candidates are validated by a no-write test pass for the
    // no-dialog path. In dialog mode the page's explicit local/cloud lookup
    // actions are used instead, so an empty password can reach the formal
    // extraction callback and open the password dialog.
    const bool candidateAccepted = TryAutomaticPasswordCandidates(
        codecs,
        formatIndices,
        excludedFormatIndices,
        archivePaths,
        archivePathsFull,
        wildcardCensor,
        options,
        extractCallback,
        showDialog);
    ExtractFlowDiagLog(candidateAccepted
        ? L"[F2] automatic candidate phase accepted"
        : L"[F2] automatic candidate phase returned");

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

  // **************** NanaZip Modification Start ****************
  // Batch pipeline: start the password prefetch worker before the
  // extraction thread. It verifies every archive's local candidates and
  // waits for the cloud result while the previous archive is still
  // extracting, so each archive's password is ready (or almost ready)
  // when its turn comes instead of being tested synchronously in the
  // extract callback.
  g_PrefetchStopped = false;
  std::thread prefetchThread;
  if (!g_SssPasswordSessionId.IsEmpty() && archivePathsFull.Size() > 1)
  {
    // Fresh result file for this batch (the File Manager reads it after
    // 7zG exits to build the summary; the file name carries the session
    // id, so a leftover from a crashed run is simply replaced).
    NDir::DeleteFileAlways(us2fs(SssBatchResultFilePath()));
    {
      std::lock_guard<std::mutex> lock(g_BatchSkippedMutex);
      g_BatchSkippedPaths.clear();
    }
    {
      std::lock_guard<std::mutex> lock(g_PrefetchMutex);
      g_PrefetchResults.clear();
    }
    prefetchThread =
        std::thread(SssPrefetchWorker, archivePathsFull, queryContext);
  }
  // **************** NanaZip Modification End ****************

  const HRESULT createRes = extracter.Create(title, hwndParent);
  if (createRes != S_OK)
  {
    // Create failed: the prefetch worker (if started) must be stopped and
    // joined before returning, otherwise destroying the joinable
    // std::thread terminates the process.
    g_PrefetchStopped = true;
    {
      std::lock_guard<std::mutex> lock(g_PrefetchMutex);
      g_PrefetchCv.notify_all();
    }
    if (prefetchThread.joinable())
    {
      prefetchThread.join();
    }
    return createRes;
  }
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
    if (extractCallback->SharePasswordAuthorized &&
        (extractCallback->PasswordSource == NanaZipPassword::PasswordSource::Manual ||
         extractCallback->PasswordSource == NanaZipPassword::PasswordSource::Local) &&
        !extractCallback->PasswordArchivePath.IsEmpty() &&
        !extractCallback->Password.IsEmpty())
    {
      NanaZipPassword::SharePassword(
          std::wstring(extractCallback->PasswordArchivePath.Ptr()),
          std::wstring(extractCallback->Password.Ptr()));
    }
    SssWriteBatchOk();
    if (!g_SssReleaseBeforeDeleteMarker.IsEmpty() && deleteAfter)
      SssWriteReleaseBeforeDeleteMarker(deletePermanently);
    else if (!g_SssNoDelete && deleteAfter)
      SssDeleteArchivesAfterExtract(archivePathsFull, deletePermanently);
  }
  // **************** NanaZip Modification Start ****************
  // Stop the prefetch worker and join it. The session pipe is still alive
  // (the File Manager waits for 7zG to exit), so the worker finishes its
  // current request or sleep and then sees the stop flag; the join is
  // bounded by one pipe round trip plus one retry interval.
  g_PrefetchStopped = true;
  {
    std::lock_guard<std::mutex> lock(g_PrefetchMutex);
    g_PrefetchCv.notify_all();
  }
  if (prefetchThread.joinable())
  {
    prefetchThread.join();
  }
  // **************** NanaZip Modification End ****************
  return extracter.Result;
}
