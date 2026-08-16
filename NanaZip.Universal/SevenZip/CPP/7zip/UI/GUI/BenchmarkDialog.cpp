// BenchmarkDialog.cpp

#include "StdAfx.h"

#include "../../../../C/CpuArch.h"

#include "../../../Common/Defs.h"
#include "../../../Common/IntToString.h"
#include "../../../Common/MyException.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/StringToInt.h"

#include "../../../Windows/Synchronization.h"
#include "../../../Windows/System.h"
#include "../../../Windows/Thread.h"
#include "../../../Windows/SystemInfo.h"

#include "../../../Windows/Control/ComboBox.h"
#include "../../../Windows/Control/Edit.h"

#include "../../Common/MethodProps.h"

#include "../FileManager/DialogSize.h"
// **************** NanaZip Modification Start ****************
//#include "../FileManager/HelpUtils.h"
// **************** NanaZip Modification End ****************
#include "../FileManager/LangUtils.h"
#include "../FileManager/resourceGui.h"

// **************** NanaZip Modification Start ****************
//#include "../../MyVersion.h"
#include <Mile.Project.Version.h>
// **************** NanaZip Modification End ****************

#include "../Common/Bench.h"

#include "BenchmarkDialogRes.h"
#include "BenchmarkDialog.h"

// **************** NanaZip Modification Start ****************
#include <NanaZip.Modern.h>
// **************** NanaZip Modification End ****************

using namespace NWindows;

// **************** NanaZip Modification Start ****************
//#define kHelpTopic "fm/benchmark.htm"
// **************** NanaZip Modification End ****************

static const UINT_PTR kTimerID = 4;
static const UINT kTimerElapse = 1000; // 1000

// use PRINT_ITER_TIME to show time of each iteration in log box
// #define PRINT_ITER_TIME

static const unsigned kRatingVector_NumBundlesMax = 20;

enum MyBenchMessages
{
  k_Message_Finished = WM_APP + 1
};

enum My_Message_WPARAM
{
  k_Msg_WPARM_Thread_Finished = 0,
  k_Msg_WPARM_Iter_Finished,
  k_Msg_WPARM_Enc1_Finished
};


struct CBenchPassResult
{
  CTotalBenchRes Enc;
  CTotalBenchRes Dec;
#ifdef PRINT_ITER_TIME
  DWORD Ticks;
#endif
  // CBenchInfo EncInfo; // for debug
  // CBenchPassResult() {};
};


struct CTotalBenchRes2: public CTotalBenchRes
{
  UInt64 UnpackSize;

  void Init()
  {
    CTotalBenchRes::Init();
    UnpackSize = 0;
  }

  void SetFrom_BenchInfo(const CBenchInfo &info)
  {
    NumIterations2 = 1;
    Generate_From_BenchInfo(info);
    UnpackSize = info.Get_UnpackSize_Full();
  }

  void Update_With_Res2(const CTotalBenchRes2 &r)
  {
    Update_With_Res(r);
    UnpackSize += r.UnpackSize;
  }
};

  
struct CSyncData
{
  UInt32 NumPasses_Finished;
#ifdef PRINT_ITER_TIME
  DWORD TotalTicks;
#endif
  int RatingVector_DeletedIndex;
  // UInt64 RatingVector_NumDeleted;

  bool BenchWasFinished; // all passes were finished
  bool NeedPrint_Freq;
  bool NeedPrint_RatingVector;
  bool NeedPrint_Enc_1;
  bool NeedPrint_Enc;
  bool NeedPrint_Dec_1;
  bool NeedPrint_Dec;
  bool NeedPrint_Tot; // intermediate Total was updated after current pass

  // UInt64 NumEncProgress; // for debug
  // UInt64 NumDecProgress; // for debug
  // CBenchInfo EncInfo; // for debug

  CTotalBenchRes2 Enc_BenchRes_1;
  CTotalBenchRes2 Enc_BenchRes;

  CTotalBenchRes2 Dec_BenchRes_1;
  CTotalBenchRes2 Dec_BenchRes;

  void Init();
};


void CSyncData::Init()
{
  NumPasses_Finished = 0;
  
  // NumEncProgress = 0;
  // NumDecProgress = 0;
  
  Enc_BenchRes.Init();
  Enc_BenchRes_1.Init();
  Dec_BenchRes.Init();
  Dec_BenchRes_1.Init();
  
  #ifdef PRINT_ITER_TIME
  TotalTicks = 0;
  #endif
  
  RatingVector_DeletedIndex = -1;
  // RatingVector_NumDeleted = 0;
  
  BenchWasFinished =
    NeedPrint_Freq =
    NeedPrint_RatingVector =
    NeedPrint_Enc_1 =
    NeedPrint_Enc   =
    NeedPrint_Dec_1 =
    NeedPrint_Dec   =
    NeedPrint_Tot   = false;
}


struct CBenchProgressSync
{
  bool Exit; // GUI asks BenchThread to Exit, and BenchThread reads that variable
  bool TextWasChanged;

  UInt32 NumThreads;
  UInt64 DictSize;
  UInt32 NumPasses_Limit;
  int Level;

  AString Text;

  /* BenchFinish_Task_HRESULT    - for result from benchmark code
     BenchFinish_Thread_HRESULT  - for Exceptions and service errors
             these arreos must be shown even if user escapes benchmark */
  HRESULT BenchFinish_Task_HRESULT;
  HRESULT BenchFinish_Thread_HRESULT;

  UInt32 NumFreqThreadsPrev;
  UString FreqString_Sync;
  UString FreqString_GUI;

  // must be written by benchmark thread, read by GUI thread */
  CRecordVector<CBenchPassResult> RatingVector;
  CSyncData sd;

  NWindows::NSynchronization::CCriticalSection CS;

  CBenchProgressSync()
  {
    NumPasses_Limit = 1;
  }

  void Init();
  
  void SendExit()
  {
    NWindows::NSynchronization::CCriticalSectionLock lock(CS);
    Exit = true;
  }
};


void CBenchProgressSync::Init()
{
  Exit = false;
  
  BenchFinish_Task_HRESULT = S_OK;
  BenchFinish_Thread_HRESULT = S_OK;
  
  sd.Init();
  RatingVector.Clear();
  
  NumFreqThreadsPrev = 0;
  FreqString_Sync.Empty();
  FreqString_GUI.Empty();
  
  Text.Empty();
  TextWasChanged = true;
}



struct CMyFont
{
  HFONT _font;
  CMyFont(): _font(NULL) {}
  ~CMyFont()
  {
    if (_font)
      DeleteObject(_font);
  }
  void Create(const LOGFONT *lplf)
  {
    _font = CreateFontIndirect(lplf);
  }
};


class CBenchmarkDialog;

struct CThreadBenchmark
{
  CBenchmarkDialog *BenchmarkDialog;
  DECL_EXTERNAL_CODECS_LOC_VARS_DECL
  // HRESULT Result;

  HRESULT Process();
  static THREAD_FUNC_DECL MyThreadFunction(void *param)
  {
    /* ((CThreadBenchmark *)param)->Result = */
    ((CThreadBenchmark *)param)->Process();
    return 0;
  }
};


class CBenchmarkDialog:
  public NWindows::NControl::CModalDialog
{
  bool _finishTime_WasSet;
  
  bool WasStopped_in_GUI;
  bool ExitWasAsked_in_GUI;
  bool NeedRestart;

  bool RamSize_Defined;

public:
  bool TotalMode;

private:

  NWindows::NControl::CComboBox m_Dictionary;
  NWindows::NControl::CComboBox m_NumThreads;
  NWindows::NControl::CComboBox m_NumPasses;
  NWindows::NControl::CEdit _consoleEdit;
  UINT_PTR _timer;

  UInt32 _startTime;
  UInt32 _finishTime;

  CMyFont _font;

  size_t RamSize;
  size_t RamSize_Limit;

  UInt32 NumPasses_Finished_Prev;

  UString ElapsedSec_Prev;

  void InitSyncNew()
  {
    NumPasses_Finished_Prev = (UInt32)(Int32)-1;
    ElapsedSec_Prev.Empty();
    Sync.Init();
  }

  virtual bool OnInit() Z7_override;
  virtual bool OnDestroy() Z7_override;
  virtual bool OnSize(WPARAM /* wParam */, int xSize, int ySize) Z7_override;
  virtual bool OnMessage(UINT message, WPARAM wParam, LPARAM lParam) Z7_override;
  virtual bool OnCommand(unsigned code, unsigned itemID, LPARAM lParam) Z7_override;
  // **************** NanaZip Modification Start ****************
  //virtual void OnHelp() Z7_override;
  // **************** NanaZip Modification End ****************
  virtual void OnCancel() Z7_override;
  virtual bool OnTimer(WPARAM timerID, LPARAM callback) Z7_override;
  virtual bool OnButtonClicked(unsigned buttonID, HWND buttonHWND) Z7_override;

  void Disable_Stop_Button();
  void OnStopButton();
  void RestartBenchmark();
  void StartBenchmark();

  void UpdateGui();

  void PrintTime();
  void PrintRating(UInt64 rating, UINT controlID);
  void PrintUsage(UInt64 usage, UINT controlID);
  void PrintBenchRes(const CTotalBenchRes2 &info, const UINT ids[]);
  void FormatBenchRes(
      const CTotalBenchRes2 &info,
      K7_BENCHMARK_STATUS &Status,
      bool enc,
      bool current);

  // **************** NanaZip Modification Start ****************
  HWND m_WindowHandle = nullptr;
  bool m_FirstRun = false;

  // Combo option values (index -> value) filled by FillContext, used by
  // the option-change messages from the XAML page.
  UInt64 m_DictSizes[K7_BENCH_MAX_COMBO_ITEMS];
  UInt32 m_DictSizesCount = 0;
  UInt32 m_ThreadValues[K7_BENCH_MAX_COMBO_ITEMS];
  UInt32 m_ThreadValuesCount = 0;
  UInt32 m_PassesValues[K7_BENCH_MAX_COMBO_ITEMS];
  UInt32 m_PassesValuesCount = 0;

  static LRESULT CALLBACK ModernWindowHandler(
      _In_ HWND hWnd,
      _In_ UINT uMsg,
      _In_ WPARAM wParam,
      _In_ LPARAM lParam,
      _In_ UINT_PTR uIdSubclass,
      _In_ DWORD_PTR dwRefData);

  bool ModernMessageRouter(UINT message, WPARAM wParam, LPARAM lParam);

  void FillContext(PK7_BENCHMARK_DIALOG_CONTEXT Context);
  // **************** NanaZip Modification End ****************

  UInt32 GetNumberOfThreads();
  size_t OnChangeDictionary();

  void SetItemText_Number(unsigned itemID, UInt64 val, LPCTSTR post = NULL);
  void Print_MemUsage(UString &s, UInt64 memUsage) const;
  bool IsMemoryUsageOK(UInt64 memUsage) const
    { return memUsage + (1 << 20) <= RamSize_Limit; }

  void MyKillTimer();

  void SendExit_Status(const wchar_t *message)
  {
    SetItemText(IDT_BENCH_ERROR_MESSAGE, message);
    Sync.SendExit();
  }

public:
  CBenchProgressSync Sync;

  CObjectVector<CProperty> Props;

  CSysString Bench2Text;

  NWindows::CThread _thread;
  CThreadBenchmark _threadBenchmark;

  CBenchmarkDialog():
      WasStopped_in_GUI(false),
      ExitWasAsked_in_GUI(false),
      NeedRestart(false),
      TotalMode(false),
      _timer(0)
      {}

  ~CBenchmarkDialog() Z7_DESTRUCTOR_override;

  bool PostMsg_Finish(WPARAM wparam)
  {
    // **************** NanaZip Modification Start ****************
    if (this->m_WindowHandle)
      return ::PostMessageW(this->m_WindowHandle, k_Message_Finished, wparam, 0) != FALSE;
    // **************** NanaZip Modification End ****************
    if ((HWND)*this)
      return PostMsg(k_Message_Finished, wparam);
    // the (HWND)*this is NULL only for some internal code failure
    return true;
  }

  INT_PTR Create(HWND wndParent = NULL)
  {
    // **************** NanaZip Modification Start ****************
    BIG_DIALOG_SIZE(332, 228);
    K7_BENCHMARK_DIALOG_CONTEXT Context = {};

    // Restore the last window position (persisted on WM_CLOSE below).
    {
      HKEY key = nullptr;
      if (::RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\NanaZip\\Options", 0,
          KEY_READ, &key) == ERROR_SUCCESS)
      {
        DWORD x = 0;
        DWORD y = 0;
        DWORD size = sizeof(DWORD);
        const bool hasX = (::RegQueryValueExW(key, L"BenchmarkWindowX", nullptr,
            nullptr, (LPBYTE)&x, &size) == ERROR_SUCCESS);
        size = sizeof(DWORD);
        const bool hasY = (::RegQueryValueExW(key, L"BenchmarkWindowY", nullptr,
            nullptr, (LPBYTE)&y, &size) == ERROR_SUCCESS);
        if (hasX && hasY)
        {
          Context.HasInitialPos = TRUE;
          Context.InitialX = (INT32)x;
          Context.InitialY = (INT32)y;
        }
        ::RegCloseKey(key);
      }
    }

    this->FillContext(&Context);
    return ::K7ModernShowBenchmarkDialog(
        wndParent,
        &Context,
        CBenchmarkDialog::ModernWindowHandler,
        this);
    // **************** NanaZip Modification End ****************
  }
  void MessageBoxError(LPCWSTR message)
  {
    // **************** NanaZip Modification Start ****************
    //MessageBoxW(*this, message, L"7-Zip", MB_ICONERROR);
    MessageBoxW(*this, message, L"NanaZip", MB_ICONERROR);
    // **************** NanaZip Modification End ****************
  }
  void MessageBoxError_Status(LPCWSTR message)
  {
    UString s ("ERROR: ");
    s += message;
    MessageBoxError(s);
    SetItemText(IDT_BENCH_ERROR_MESSAGE, s);
  }
};









UString HResultToMessage(HRESULT errorCode);

#ifdef Z7_LANG
static const UInt32 kLangIDs[] =
{
  IDT_BENCH_DICTIONARY,
  IDT_BENCH_MEMORY,
  IDT_BENCH_NUM_THREADS,
  IDT_BENCH_SIZE,
  IDT_BENCH_RATING_LABEL,
  IDT_BENCH_USAGE_LABEL,
  IDT_BENCH_RPU_LABEL,
  IDG_BENCH_COMPRESSING,
  IDG_BENCH_DECOMPRESSING,
  IDG_BENCH_TOTAL_RATING,
  IDT_BENCH_CURRENT,
  IDT_BENCH_RESULTING,
  IDT_BENCH_ELAPSED,
  IDT_BENCH_PASSES,
  IDB_STOP,
  IDB_RESTART
};

static const UInt32 kLangIDs_RemoveColon[] =
{
  IDT_BENCH_SPEED
};

#endif

static LPCTSTR const kProcessingString = TEXT("...");
static LPCTSTR const kGB = TEXT(" GB");
static LPCTSTR const kMB = TEXT(" MB");
static LPCTSTR const kKB = TEXT(" KB");
// static LPCTSTR const kMIPS = TEXT(" MIPS");
static LPCTSTR const kKBs = TEXT(" KB/s");

static const unsigned kMinDicLogSize = 18;

static const UInt32 kMinDicSize = (UInt32)1 << kMinDicLogSize;
static const size_t kMaxDicSize = (size_t)1 << (22 + sizeof(size_t) / 4 * 5);
// static const size_t kMaxDicSize = (size_t)1 << 16;
    /*
    #ifdef MY_CPU_64BIT
      (UInt32)(Int32)-1; // we can use it, if we want 4 GB buffer
      // (UInt32)15 << 28;
    #else
      (UInt32)1 << 27;
    #endif
    */


static int ComboBox_Add_UInt32(NWindows::NControl::CComboBox &cb, UInt32 v)
{
  WCHAR s[16];
  ConvertUInt32ToString(v, s);
  return (int)cb.AddString_SetItemData(s, (LPARAM)v);
}


// **************** NanaZip Modification Start ****************
static void SaveBenchmarkWindowPosition(HWND hWnd)
{
  RECT rc = {};
  if (!::GetWindowRect(hWnd, &rc))
    return;
  HKEY key = nullptr;
  if (::RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\NanaZip\\Options", 0,
      nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) == ERROR_SUCCESS)
  {
    DWORD x = (DWORD)rc.left;
    DWORD y = (DWORD)rc.top;
    ::RegSetValueExW(key, L"BenchmarkWindowX", 0, REG_DWORD,
        (LPBYTE)&x, sizeof(x));
    ::RegSetValueExW(key, L"BenchmarkWindowY", 0, REG_DWORD,
        (LPBYTE)&y, sizeof(y));
    ::RegCloseKey(key);
  }
}

static void CopyTruncatedStr(
    _Out_writes_z_(MaxLen) wchar_t *Dest,
    _In_ UINT32 MaxLen,
    const UString &Src)
{
  UString s = Src;
  if (s.Len() >= (int)MaxLen)
  {
    s.DeleteFrom(MaxLen - 1);
  }
  wcscpy_s(Dest, MaxLen, s.Ptr());
}

static void CopyTruncatedStr(
    _Out_writes_z_(MaxLen) wchar_t *Dest,
    _In_ UINT32 MaxLen,
    const AString &Src)
{
  CopyTruncatedStr(Dest, MaxLen, MultiByteToUnicodeString(Src, CP_ACP));
}

// Prepares the XAML dialog context (system info + combo options + initial
// values) before K7ModernShowBenchmarkDialog shows the window. The Win32
// OnInit does the same work for the Win32 dialog; it is not called in the
// XAML mode.
void CBenchmarkDialog::FillContext(PK7_BENCHMARK_DIALOG_CONTEXT Context)
{
  // Dialog font size from the registry (mirrors the ExtractDialog font).
  {
    DWORD pt = 0;
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\NanaZip\\Options", 0,
        KEY_READ, &key) == ERROR_SUCCESS)
    {
      DWORD size = sizeof(pt);
      ::RegQueryValueExW(key, L"FontSizeDialog", nullptr, nullptr,
          (LPBYTE)&pt, &size);
      ::RegCloseKey(key);
    }
    Context->FontSizeDialog = pt;
  }

  Context->TotalMode = TotalMode ? TRUE : FALSE;

  UInt32 numCPUs = 1; // process threads
  UInt32 numCPUs_Sys = 1; // system threads
  {
    NSystem::CProcessAffinity threadsInfo;
    threadsInfo.InitST();
  #ifndef Z7_ST
    threadsInfo.Get_and_return_NumProcessThreads_and_SysThreads(numCPUs, numCPUs_Sys);
  #endif

    AString s (" / ");
    s.Add_UInt32(numCPUs);
    s += GetProcessThreadsInfo(threadsInfo);
    CopyTruncatedStr(Context->HardwareThreads,
        Z7_ARRAY_SIZE(Context->HardwareThreads), s);

    {
      AString s2;
      GetSysInfo(s, s2);
      CopyTruncatedStr(Context->Sys1,
          Z7_ARRAY_SIZE(Context->Sys1), s);
      if (s != s2 && !s2.IsEmpty())
        CopyTruncatedStr(Context->Sys2,
            Z7_ARRAY_SIZE(Context->Sys2), s2);
      GetCpuName_MultiLine(s, s2); // s2==registers
      CopyTruncatedStr(Context->Cpu,
          Z7_ARRAY_SIZE(Context->Cpu), s);
    }
    {
      GetOsInfoText(s);
      s += " : ";
      AddCpuFeatures(s);
      CopyTruncatedStr(Context->Features,
          Z7_ARRAY_SIZE(Context->Features), s);
    }
    {
      s = "NanaZip " MILE_PROJECT_VERSION_UTF8_STRING " (" MY_CPU_NAME ")";
      CopyTruncatedStr(Context->Version,
          Z7_ARRAY_SIZE(Context->Version), s);
    }
  }

  // ----- Num Threads ----------

  UInt32 numThreads = Sync.NumThreads;
  if (numThreads == (UInt32)(Int32)-1)
    numThreads = numCPUs;
  numThreads &= ~(UInt32)1;
  if (numThreads == 0)
    numThreads = 1;
  numThreads = MyMin(numThreads, (UInt32)(1u << 14));

  if (numCPUs_Sys == 0)
    numCPUs_Sys = 1;
  const UInt32 numTheads_Combo = numCPUs_Sys * 2;
  UInt32 v = 1;
  UInt32 count = 0;
  UInt32 cur = 0;
  for (; v <= numTheads_Combo;)
  {
    UInt32 index = count;
    WCHAR tmp[32];
    ConvertUInt32ToString(v, tmp);
    wcscpy_s(Context->ThreadItems[count], K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);
    m_ThreadValues[count] = v;
    count++;
    const UInt32 vNext = v + (v < 2 ? 1 : 2);
    if (v <= numThreads)
    if (numThreads < vNext || vNext > numTheads_Combo)
    {
      if (v != numThreads)
      {
        WCHAR tmp2[32];
        ConvertUInt32ToString(numThreads, tmp2);
        wcscpy_s(Context->ThreadItems[count], K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp2);
        m_ThreadValues[count] = numThreads;
        cur = count;
        count++;
      }
      else
        cur = index;
    }
    v = vNext;
  }
  Context->ThreadItemsCount = count;
  Context->ThreadIndex = cur;
  m_ThreadValuesCount = count;
  Sync.NumThreads = m_ThreadValues[cur];

  // ----- Dictionary ----------

  RamSize = (UInt64)(sizeof(size_t)) << 29;
  RamSize_Defined = NSystem::GetRamSize(RamSize);
  RamSize_Limit = RamSize / 16 * 15;

  if (Sync.DictSize == (UInt64)(Int64)-1)
  {
    unsigned dicSizeLog = 25;
    if (RamSize_Defined)
    for (; dicSizeLog > kBenchMinDicLogSize; dicSizeLog--)
      if (IsMemoryUsageOK(GetBenchMemoryUsage(
          Sync.NumThreads, Sync.Level, (UInt64)1 << dicSizeLog, TotalMode)))
        break;
    Sync.DictSize = (UInt64)1 << dicSizeLog;
  }

  if (Sync.DictSize < kMinDicSize) Sync.DictSize = kMinDicSize;
  if (Sync.DictSize > kMaxDicSize) Sync.DictSize = kMaxDicSize;

  cur = 0;
  count = 0;
  for (unsigned i = (kMinDicLogSize - 1) * 2; i <= (32 - 1) * 2; i++)
  {
    const size_t dict = (size_t)(2 + (i & 1)) << (i / 2);
    TCHAR s[32];
    const TCHAR *post;
    UInt32 d;
         if (dict >= ((UInt32)1 << 31)) { d = (UInt32)(dict >> 30); post = kGB; }
    else if (dict >= ((UInt32)1 << 21)) { d = (UInt32)(dict >> 20); post = kMB; }
    else                                { d = (UInt32)(dict >> 10); post = kKB; }
    ConvertUInt32ToString(d, s);
    lstrcat(s, post);
    wcscpy_s(Context->DictItems[count], K7_BENCH_MAX_SHORT_TEXT_LENGTH, s);
    m_DictSizes[count] = dict;
    {
      UString memS;
      Print_MemUsage(memS, GetBenchMemoryUsage(
          Sync.NumThreads, Sync.Level, dict, TotalMode));
      CopyTruncatedStr(Context->DictMemoryItems[count],
          K7_BENCH_MAX_SHORT_TEXT_LENGTH, memS);
    }
    if (dict <= Sync.DictSize)
      cur = count;
    count++;
    if (dict >= kMaxDicSize)
      break;
  }
  Context->DictItemsCount = count;
  Context->DictIndex = cur;
  m_DictSizesCount = count;

  // ----- Num Passes ----------

  cur = 0;
  count = 0;
  v = 1;
  for (;;)
  {
    UInt32 index = count;
    WCHAR tmp[32];
    ConvertUInt32ToString(v, tmp);
    wcscpy_s(Context->PassesItems[count], K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);
    m_PassesValues[count] = v;
    count++;
    const bool isLast = (v >= 10000000);
    UInt32 vNext = v * 10;
         if (v < 2) vNext = 2;
    else if (v < 5) vNext = 5;
    else if (v < 10) vNext = 10;

    if (v <= Sync.NumPasses_Limit)
    if (isLast || Sync.NumPasses_Limit < vNext)
    {
      if (v != Sync.NumPasses_Limit)
      {
        WCHAR tmp2[32];
        ConvertUInt32ToString(Sync.NumPasses_Limit, tmp2);
        wcscpy_s(Context->PassesItems[count], K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp2);
        m_PassesValues[count] = Sync.NumPasses_Limit;
        cur = count;
        count++;
      }
      else
        cur = index;
    }
    v = vNext;
    if (isLast)
      break;
  }
  Context->PassesItemsCount = count;
  Context->PassesIndex = cur;
  m_PassesValuesCount = count;
}
// **************** NanaZip Modification End ****************

bool CBenchmarkDialog::OnInit()
{
  #ifdef Z7_LANG
  LangSetWindowText(*this, IDD_BENCH);
  LangSetDlgItems(*this, kLangIDs, Z7_ARRAY_SIZE(kLangIDs));
  LangSetDlgItems_RemoveColon(*this, kLangIDs_RemoveColon, Z7_ARRAY_SIZE(kLangIDs_RemoveColon));
  LangSetDlgItemText(*this, IDT_BENCH_CURRENT2, IDT_BENCH_CURRENT);
  LangSetDlgItemText(*this, IDT_BENCH_RESULTING2, IDT_BENCH_RESULTING);
  #endif

  InitSyncNew();

  if (TotalMode)
  {
    _consoleEdit.Attach(GetItem(IDE_BENCH2_EDIT));
    LOGFONT f;
    memset(&f, 0, sizeof(f));
    f.lfHeight = 14;
    f.lfWidth = 0;
    f.lfWeight = FW_DONTCARE;
    f.lfCharSet = DEFAULT_CHARSET;
    f.lfOutPrecision = OUT_DEFAULT_PRECIS;
    f.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    f.lfQuality = DEFAULT_QUALITY;

    f.lfPitchAndFamily = FIXED_PITCH;
    // MyStringCopy(f.lfFaceName, TEXT(""));
    // f.lfFaceName[0] = 0;
    _font.Create(&f);
    if (_font._font)
      _consoleEdit.SendMsg(WM_SETFONT, (WPARAM)_font._font, TRUE);
  }

  UInt32 numCPUs = 1; // process threads
  UInt32 numCPUs_Sys = 1; // system threads

  {
    NSystem::CProcessAffinity threadsInfo;
    threadsInfo.InitST();
#ifndef Z7_ST
    threadsInfo.Get_and_return_NumProcessThreads_and_SysThreads(numCPUs, numCPUs_Sys);
#endif

    AString s ("/ ");
    s.Add_UInt32(numCPUs);
    s += GetProcessThreadsInfo(threadsInfo);
    SetItemTextA(IDT_BENCH_HARDWARE_THREADS, s);
  
    {
      AString s2;
      GetSysInfo(s, s2);
      SetItemTextA(IDT_BENCH_SYS1, s);
      if (s != s2 && !s2.IsEmpty())
        SetItemTextA(IDT_BENCH_SYS2, s2);

      GetCpuName_MultiLine(s, s2); // s2==registers
      SetItemTextA(IDT_BENCH_CPU, s);
    }
    {
      GetOsInfoText(s);
      s += " : ";
      AddCpuFeatures(s);
      SetItemTextA(IDT_BENCH_CPU_FEATURE, s);
    }

    // **************** NanaZip Modification Start ****************
    //s = "7-Zip " MY_VERSION_CPU;
    s = "NanaZip " MILE_PROJECT_VERSION_UTF8_STRING " (" MY_CPU_NAME ")";
    // **************** NanaZip Modification End ****************
    SetItemTextA(IDT_BENCH_VER, s);
  }


  // ----- Num Threads ----------

  UInt32 numThreads = Sync.NumThreads;
  if (numThreads == (UInt32)(Int32)-1)
    numThreads = numCPUs;
  numThreads &= ~(UInt32)1;
  if (numThreads == 0)
    numThreads = 1;
  numThreads = MyMin(numThreads, (UInt32)(1u << 14));

  m_NumThreads.Attach(GetItem(IDC_BENCH_NUM_THREADS));
  if (numCPUs_Sys == 0)
    numCPUs_Sys = 1;
  const UInt32 numTheads_Combo = numCPUs_Sys * 2;
  UInt32 v = 1;
  int cur = 0;
  for (; v <= numTheads_Combo;)
  {
    int index = ComboBox_Add_UInt32(m_NumThreads, v);
    const UInt32 vNext = v + (v < 2 ? 1 : 2);
    if (v <= numThreads)
    if (numThreads < vNext || vNext > numTheads_Combo)
    {
      if (v != numThreads)
        index = ComboBox_Add_UInt32(m_NumThreads, numThreads);
      cur = index;
    }
    v = vNext;
  }
  m_NumThreads.SetCurSel(cur);
  Sync.NumThreads = GetNumberOfThreads();


  // ----- Dictionary ----------

  m_Dictionary.Attach(GetItem(IDC_BENCH_DICTIONARY));
  
  RamSize = (UInt64)(sizeof(size_t)) << 29;
  RamSize_Defined = NSystem::GetRamSize(RamSize);

  
  #ifdef UNDER_CE
  const UInt32 kNormalizedCeSize = (16 << 20);
  if (RamSize > kNormalizedCeSize && RamSize < (33 << 20))
    RamSize = kNormalizedCeSize;
  #endif
  RamSize_Limit = RamSize / 16 * 15;

  if (Sync.DictSize == (UInt64)(Int64)-1)
  {
    unsigned dicSizeLog = 25;
    #ifdef UNDER_CE
    dicSizeLog = 20;
    #endif
    if (RamSize_Defined)
    for (; dicSizeLog > kBenchMinDicLogSize; dicSizeLog--)
      if (IsMemoryUsageOK(GetBenchMemoryUsage(
          Sync.NumThreads, Sync.Level, (UInt64)1 << dicSizeLog, TotalMode)))
        break;
    Sync.DictSize = (UInt64)1 << dicSizeLog;
  }
  
  if (Sync.DictSize < kMinDicSize) Sync.DictSize = kMinDicSize;
  if (Sync.DictSize > kMaxDicSize) Sync.DictSize = kMaxDicSize;

  cur = 0;
  for (unsigned i = (kMinDicLogSize - 1) * 2; i <= (32 - 1) * 2; i++)
   {
      const size_t dict = (size_t)(2 + (i & 1)) << (i / 2);
      // if (i == (32 - 1) * 2) dict = kMaxDicSize;
      TCHAR s[32];
      const TCHAR *post;
      UInt32 d;
           if (dict >= ((UInt32)1 << 31)) { d = (UInt32)(dict >> 30); post = kGB; }
      else if (dict >= ((UInt32)1 << 21)) { d = (UInt32)(dict >> 20); post = kMB; }
      else                                { d = (UInt32)(dict >> 10); post = kKB; }
      ConvertUInt32ToString(d, s);
      lstrcat(s, post);
      const int index = (int)m_Dictionary.AddString(s);
      m_Dictionary.SetItemData(index, (LPARAM)dict);
      if (dict <= Sync.DictSize)
        cur = index;
      if (dict >= kMaxDicSize)
        break;
    }
  m_Dictionary.SetCurSel(cur);


  // ----- Num Passes ----------

  m_NumPasses.Attach(GetItem(IDC_BENCH_NUM_PASSES));
  cur = 0;
  v = 1;
  for (;;)
  {
    int index = ComboBox_Add_UInt32(m_NumPasses, v);
    const bool isLast = (v >= 10000000);
    UInt32 vNext = v * 10;
         if (v < 2) vNext = 2;
    else if (v < 5) vNext = 5;
    else if (v < 10) vNext = 10;

    if (v <= Sync.NumPasses_Limit)
    if (isLast || Sync.NumPasses_Limit < vNext)
    {
      if (v != Sync.NumPasses_Limit)
        index = ComboBox_Add_UInt32(m_NumPasses, Sync.NumPasses_Limit);
      cur = index;
    }
    v = vNext;
    if (isLast)
      break;
  }
  m_NumPasses.SetCurSel(cur);

  if (TotalMode)
    NormalizeSize(true);
  else
    NormalizePosition();

  RestartBenchmark();

  return CModalDialog::OnInit();
}


bool CBenchmarkDialog::OnSize(WPARAM /* wParam */, int xSize, int ySize)
{
  int mx, my;
  GetMargins(8, mx, my);

  if (!TotalMode)
  {
    RECT rect;
    GetClientRectOfItem(IDT_BENCH_LOG, rect);
    int x = xSize - rect.left - mx;
    int y = ySize - rect.top - my;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    MoveItem(IDT_BENCH_LOG, rect.left, rect.top, x, y, true);
    return false;
  }

  // **************** NanaZip Modification Start ****************
  // int bx1, bx2, by;
  int bx1, by;
  // **************** NanaZip Modification End ****************

  GetItemSizes(IDCANCEL, bx1, by);
  // **************** NanaZip Modification Start ****************
  //GetItemSizes(IDHELP, bx2, by);
  // **************** NanaZip Modification End ****************

  {
    int y = ySize - my - by;
    int x = xSize - mx - bx1;
    
    InvalidateRect(NULL);
    
    MoveItem(IDCANCEL, x, y, bx1, by);
    // **************** NanaZip Modification Start ****************
    //MoveItem(IDHELP, x - mx - bx2, y, bx2, by);
    // **************** NanaZip Modification End ****************
  }

  if (_consoleEdit)
  {
    int yPos = ySize - my - by;
    RECT rect;
    GetClientRectOfItem(IDE_BENCH2_EDIT, rect);
    int y = rect.top;
    int ySize2 = yPos - my - y;
    const int kMinYSize = 20;
    int xx = xSize - mx * 2;
    if (ySize2 < kMinYSize)
    {
      ySize2 = kMinYSize;
    }
    _consoleEdit.Move(mx, y, xx, ySize2);
  }
  return false;
}


UInt32 CBenchmarkDialog::GetNumberOfThreads()
{
  return (UInt32)m_NumThreads.GetItemData_of_CurSel();
}


#define UINT_TO_STR_3(s, val) { \
  s[0] = (wchar_t)('0' + (val) / 100); \
  s[1] = (wchar_t)('0' + (val) % 100 / 10); \
  s[2] = (wchar_t)('0' + (val) % 10); \
  s += 3; s[0] = 0; }

static WCHAR *NumberToDot3(UInt64 val, WCHAR *s)
{
  s = ConvertUInt64ToString(val / 1000, s);
  const UInt32 rem = (UInt32)(val % 1000);
  *s++ = '.';
  UINT_TO_STR_3(s, rem)
  return s;
}

void CBenchmarkDialog::SetItemText_Number(unsigned itemID, UInt64 val, LPCTSTR post)
{
  TCHAR s[64];
  ConvertUInt64ToString(val, s);
  if (post)
    lstrcat(s, post);
  SetItemText(itemID, s);
}

static void AddSize_MB(UString &s, UInt64 size)
{
  s.Add_UInt64((size + (1 << 20) - 1) >> 20);
  s += kMB;
}

void CBenchmarkDialog::Print_MemUsage(UString &s, UInt64 memUsage) const
{
  AddSize_MB(s, memUsage);
  if (RamSize_Defined)
  {
    s += " / ";
    AddSize_MB(s, RamSize);
  }
}

size_t CBenchmarkDialog::OnChangeDictionary()
{
  const size_t dict = (size_t)m_Dictionary.GetItemData_of_CurSel();
  const UInt64 memUsage = GetBenchMemoryUsage(GetNumberOfThreads(),
      Sync.Level,
      dict,
      false); // totalBench mode

  UString s;
  Print_MemUsage(s, memUsage);

  #ifdef Z7_LARGE_PAGES
  {
    AString s2;
    Add_LargePages_String(s2);
    if (!s2.IsEmpty())
    {
      s.Add_Space();
      s += s2;
    }
  }
  #endif

  SetItemText(IDT_BENCH_MEMORY_VAL, s);

  return dict;
}


static const UInt32 g_IDs[] =
{
  IDT_BENCH_COMPRESS_SIZE1,
  IDT_BENCH_COMPRESS_SIZE2,
  IDT_BENCH_COMPRESS_USAGE1,
  IDT_BENCH_COMPRESS_USAGE2,
  IDT_BENCH_COMPRESS_SPEED1,
  IDT_BENCH_COMPRESS_SPEED2,
  IDT_BENCH_COMPRESS_RATING1,
  IDT_BENCH_COMPRESS_RATING2,
  IDT_BENCH_COMPRESS_RPU1,
  IDT_BENCH_COMPRESS_RPU2,
  
  IDT_BENCH_DECOMPR_SIZE1,
  IDT_BENCH_DECOMPR_SIZE2,
  IDT_BENCH_DECOMPR_SPEED1,
  IDT_BENCH_DECOMPR_SPEED2,
  IDT_BENCH_DECOMPR_RATING1,
  IDT_BENCH_DECOMPR_RATING2,
  IDT_BENCH_DECOMPR_USAGE1,
  IDT_BENCH_DECOMPR_USAGE2,
  IDT_BENCH_DECOMPR_RPU1,
  IDT_BENCH_DECOMPR_RPU2,
  
  IDT_BENCH_TOTAL_USAGE_VAL,
  IDT_BENCH_TOTAL_RATING_VAL,
  IDT_BENCH_TOTAL_RPU_VAL
};
  

static const unsigned k_Ids_Enc_1[] = {
  IDT_BENCH_COMPRESS_USAGE1,
  IDT_BENCH_COMPRESS_SPEED1,
  IDT_BENCH_COMPRESS_RPU1,
  IDT_BENCH_COMPRESS_RATING1,
  IDT_BENCH_COMPRESS_SIZE1 };

static const unsigned k_Ids_Enc[] = {
  IDT_BENCH_COMPRESS_USAGE2,
  IDT_BENCH_COMPRESS_SPEED2,
  IDT_BENCH_COMPRESS_RPU2,
  IDT_BENCH_COMPRESS_RATING2,
  IDT_BENCH_COMPRESS_SIZE2 };

static const unsigned k_Ids_Dec_1[] = {
  IDT_BENCH_DECOMPR_USAGE1,
  IDT_BENCH_DECOMPR_SPEED1,
  IDT_BENCH_DECOMPR_RPU1,
  IDT_BENCH_DECOMPR_RATING1,
  IDT_BENCH_DECOMPR_SIZE1 };

static const unsigned k_Ids_Dec[] = {
  IDT_BENCH_DECOMPR_USAGE2,
  IDT_BENCH_DECOMPR_SPEED2,
  IDT_BENCH_DECOMPR_RPU2,
  IDT_BENCH_DECOMPR_RATING2,
  IDT_BENCH_DECOMPR_SIZE2 };

static const unsigned k_Ids_Tot[] = {
  IDT_BENCH_TOTAL_USAGE_VAL,
  0,
  IDT_BENCH_TOTAL_RPU_VAL,
  IDT_BENCH_TOTAL_RATING_VAL,
  0 };


void CBenchmarkDialog::MyKillTimer()
{
  if (_timer != 0)
  {
    // **************** NanaZip Modification Start ****************
    if (this->m_WindowHandle)
      ::KillTimer(this->m_WindowHandle, kTimerID);
    // **************** NanaZip Modification End ****************
    _timer = 0;
  }
}


bool CBenchmarkDialog::OnDestroy()
{
  /* actually timer was removed before.
     also the timer must be removed by Windows, when window  will be removed. */
  MyKillTimer(); // it's optional code
  return false; // we return (false) to perform default dialog operation
}

void SetErrorMessage_MemUsage(UString &s, UInt64 reqSize, UInt64 ramSize, UInt64 ramLimit, const UString &usageString);

void CBenchmarkDialog::StartBenchmark()
{
  NeedRestart = false;
  WasStopped_in_GUI = false;

  SetItemText_Empty(IDT_BENCH_ERROR_MESSAGE);
  
  MyKillTimer(); // optional code. timer was killed before

  // **************** NanaZip Modification Start ****************
  // XAML mode: the options come from the Sync fields (updated by the
  // option-change messages from the XAML page); the Win32 combo controls
  // are not created.
  const size_t dict = (size_t)Sync.DictSize;
  const UInt32 numThreads = Sync.NumThreads;
  const UInt32 numPasses = Sync.NumPasses_Limit;
  // **************** NanaZip Modification End ****************

  const UInt64 memUsage = GetBenchMemoryUsage(numThreads, Sync.Level, dict,
      false); // totalBench
  if (!IsMemoryUsageOK(memUsage))
  {
    UString s2;
    LangString_OnlyFromLangFile(IDS_MEM_REQUIRED_MEM_SIZE, s2);
    if (s2.IsEmpty())
    {
      s2 = LangString(IDT_BENCH_MEMORY);
      if (s2.IsEmpty())
        GetItemText(IDT_BENCH_MEMORY, s2);
      s2.RemoveChar(L':');
    }
    UString s;
    SetErrorMessage_MemUsage(s, memUsage, RamSize, RamSize_Limit, s2);
    // **************** NanaZip Modification Start ****************
    if (this->m_WindowHandle)
    {
      K7_BENCHMARK_STATUS Status = {};
      Status.HasError = TRUE;
      CopyTruncatedStr(Status.Error, K7_BENCH_MAX_TEXT_LENGTH, s);
      ::K7ModernUpdateBenchmarkStatus(this->m_WindowHandle, &Status);
    }
    // **************** NanaZip Modification End ****************
    return;
  }

  _startTime = GetTickCount();
  _finishTime = _startTime;
  _finishTime_WasSet = false;

  {
    NWindows::NSynchronization::CCriticalSectionLock lock(Sync.CS);
    InitSyncNew();
    Sync.DictSize = dict;
    Sync.NumThreads = numThreads;
    Sync.NumPasses_Limit = numPasses;
  }

  // **************** NanaZip Modification Start ****************
  if (this->m_WindowHandle)
    _timer = ::SetTimer(this->m_WindowHandle, kTimerID, kTimerElapse, NULL);
  // **************** NanaZip Modification End ****************
  if (_thread.Create(CThreadBenchmark::MyThreadFunction, &_threadBenchmark) != 0)
  {
    MyKillTimer();
    // **************** NanaZip Modification Start ****************
    if (this->m_WindowHandle)
    {
      K7_BENCHMARK_STATUS Status = {};
      Status.HasError = TRUE;
      CopyTruncatedStr(Status.Error, K7_BENCH_MAX_TEXT_LENGTH,
          UString(L"Can't create thread"));
      ::K7ModernUpdateBenchmarkStatus(this->m_WindowHandle, &Status);
    }
    // **************** NanaZip Modification End ****************
  }
  else
  {
    // Reflect the running state immediately (the XAML Stop button is
    // enabled via the Running flag).
    UpdateGui();
  }
  return;
}


void CBenchmarkDialog::RestartBenchmark()
{
  if (ExitWasAsked_in_GUI)
    return;

  if (_thread.IsCreated())
  {
    NeedRestart = true;
    SendExit_Status(L"Stop for restart ...");
  }
  else
    StartBenchmark();
}


void CBenchmarkDialog::Disable_Stop_Button()
{
  // **************** NanaZip Modification Start ****************
  // XAML mode: the Stop button state is driven by the Running flag in the
  // status refresh; there is no Win32 button to disable.
  // **************** NanaZip Modification End ****************
}


void CBenchmarkDialog::OnStopButton()
{
  if (ExitWasAsked_in_GUI)
    return;

  // **************** NanaZip Modification Start ****************
  //Disable_Stop_Button();
  // **************** NanaZip Modification End ****************

  WasStopped_in_GUI = true;
  if (_thread.IsCreated())
  {
    SendExit_Status(L"Stop ...");
  }
}



void CBenchmarkDialog::OnCancel()
{
  ExitWasAsked_in_GUI = true;
  
  /*
  SendMsg_NextDlgCtl_Prev();
  EnableItem(IDCANCEL, false);
  */

  if (_thread.IsCreated())
    SendExit_Status(L"Cancel ...");
  // **************** NanaZip Modification Start ****************
  else if (this->m_WindowHandle)
    ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
  // **************** NanaZip Modification End ****************
}


// **************** NanaZip Modification Start ****************
//void CBenchmarkDialog::OnHelp()
//{
//  ShowHelpWindow(kHelpTopic);
//}
// **************** NanaZip Modification End ****************



// void GetTimeString(UInt64 timeValue, wchar_t *s);

void CBenchmarkDialog::PrintTime()
{
  const UInt32 curTime =
    _finishTime_WasSet ?
      _finishTime :
      ::GetTickCount();

  const UInt32 elapsedTime = (curTime - _startTime);

  WCHAR s[64];

  WCHAR *p = ConvertUInt32ToString(elapsedTime / 1000, s);

  if (_finishTime_WasSet)
  {
    *p++ = '.';
    UINT_TO_STR_3(p, elapsedTime % 1000)
  }

  // p = NumberToDot3((UInt64)elapsedTime, s);

  MyStringCopy(p, L" s");

  // if (WasStopped_in_GUI) wcscat(s, L" X"); // for debug

  if (s == ElapsedSec_Prev)
    return;

  ElapsedSec_Prev = s;

  // static cnt = 0; cnt++; wcscat(s, L" ");
  // UString s2; s2.Add_UInt32(cnt); wcscat(s, s2.Ptr());

  SetItemText(IDT_BENCH_ELAPSED_VAL, s);
}


static UInt64 GetMips(UInt64 ips)
{
  return (ips + 500000) / 1000000;
}


static UInt64 GetUsagePercents(UInt64 usage)
{
  return Benchmark_GetUsage_Percents(usage);
}


static UInt32 GetRating(const CTotalBenchRes &info)
{
  UInt64 numIter = info.NumIterations2;
  if (numIter == 0)
    numIter = 1000000;
  const UInt64 rating64 = GetMips(info.Rating / numIter);
  // return rating64;
  UInt32 rating32 = (UInt32)rating64;
  if (rating32 != rating64)
    rating32 = (UInt32)(Int32)-1;
  return rating32;
}


static void AddUsageString(UString &s, const CTotalBenchRes &info)
{
  UInt64 numIter = info.NumIterations2;
  if (numIter == 0)
    numIter = 1000000;
  UInt64 usage = GetUsagePercents(info.Usage / numIter);

  wchar_t w[32];
  wchar_t *p = ConvertUInt64ToString(usage, w);
  p[0] = '%';
  p[1] = 0;
  unsigned len = (unsigned)(size_t)(p - w);
  while (len < 5)
  {
    s.Add_Space();
    len++;
  }
  s += w;
}


static void Add_Dot3String(UString &s, UInt64 val)
{
  WCHAR temp[32];
  NumberToDot3(val, temp);
  s += temp;
}


static void AddRatingString(UString &s, const CTotalBenchRes &info)
{
  // AddUsageString(s, info);
  // s.Add_Space();
  // s.Add_UInt32(GetRating(info));
  Add_Dot3String(s, GetRating(info));
}


static void AddRatingsLine(UString &s, const CTotalBenchRes &enc, const CTotalBenchRes &dec
    #ifdef PRINT_ITER_TIME
    , DWORD ticks
    #endif
    )
{
  // AddUsageString(s, enc); s.Add_Space();

  AddRatingString(s, enc);
  s += "  ";
  AddRatingString(s, dec);
  
  CTotalBenchRes tot_BenchRes;
  tot_BenchRes.SetSum(enc, dec);
  
  s += "  ";
  AddRatingString(s, tot_BenchRes);
  
  s.Add_Space();  AddUsageString(s, tot_BenchRes);

  
  #ifdef PRINT_ITER_TIME
  s.Add_Space();
  {
    Add_Dot3String(s, ticks;
    s += " s";
    // s.Add_UInt32(ticks); s += " ms";
  }
  #endif
}


void CBenchmarkDialog::PrintRating(UInt64 rating, UINT controlID)
{
  // SetItemText_Number(controlID, GetMips(rating), kMIPS);
  WCHAR s[64];
  MyStringCopy(NumberToDot3(GetMips(rating), s), L" GIPS");
  SetItemText(controlID, s);
}

void CBenchmarkDialog::PrintUsage(UInt64 usage, UINT controlID)
{
  SetItemText_Number(controlID, GetUsagePercents(usage), TEXT("%"));
}


// void SetItemText_Number

void CBenchmarkDialog::PrintBenchRes(
    const CTotalBenchRes2 &info,
    const UINT ids[])
{
  if (info.NumIterations2 == 0)
    return;
  if (ids[1] != 0)
    SetItemText_Number(ids[1], (info.Speed >> 10) / info.NumIterations2, kKBs);
  PrintRating(info.Rating / info.NumIterations2, ids[3]);
  PrintRating(info.RPU / info.NumIterations2, ids[2]);
  PrintUsage(info.Usage / info.NumIterations2, ids[0]);
  if (ids[4] != 0)
  {
    UInt64 val = info.UnpackSize;
    LPCTSTR kPostfix;
    if (val >= ((UInt64)1 << 40))
    {
      kPostfix = kGB;
      val >>= 30;
    }
    else
    {
      kPostfix = kMB;
      val >>= 20;
    }
    SetItemText_Number(ids[4], val, kPostfix);
  }
}


// static UInt32 k_Message_Finished_cnt = 0;
// static UInt32 k_OnTimer_cnt = 0;

bool CBenchmarkDialog::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  if (message != k_Message_Finished)
    return CModalDialog::OnMessage(message, wParam, lParam);

  {
    if (wParam == k_Msg_WPARM_Thread_Finished)
    {
      _finishTime = GetTickCount();
      _finishTime_WasSet = true;
      MyKillTimer();

      if (_thread.Wait_Close() != 0)
      {
        MessageBoxError_Status(L"Thread Wait Error");
      }

      if (!WasStopped_in_GUI)
      {
        WasStopped_in_GUI = true;
        Disable_Stop_Button();
      }

      HRESULT res = Sync.BenchFinish_Thread_HRESULT;
      if (res != S_OK)
      // if (!ExitWasAsked_in_GUI || res != E_ABORT)
        MessageBoxError_Status(HResultToMessage(res));

      if (ExitWasAsked_in_GUI)
      {
        // **************** NanaZip Modification Start ****************
        //CModalDialog::OnCancel();
        if (this->m_WindowHandle)
          ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
        // **************** NanaZip Modification End ****************
        return true;
      }
    
      SetItemText_Empty(IDT_BENCH_ERROR_MESSAGE);

      res = Sync.BenchFinish_Task_HRESULT;
      if (res != S_OK)
      {
        if (!WasStopped_in_GUI || res != E_ABORT)
        {
          UString m;
          if (res == S_FALSE)
            m = "Decoding error";
          else if (res == CLASS_E_CLASSNOTAVAILABLE)
            // **************** NanaZip Modification Start ****************
            //m = "Can't find 7z.dll";
            m = "Can't find NanaZip.Core.dll";
            // **************** NanaZip Modification End ****************
          else
            m = HResultToMessage(res);
          MessageBoxError_Status(m);
        }
      }

      if (NeedRestart)
      {
        StartBenchmark();
        return true;
      }
    }
    // k_Message_Finished_cnt++;
    UpdateGui();
    return true;
  }
}


bool CBenchmarkDialog::OnTimer(WPARAM timerID, LPARAM /* callback */)
{
  // k_OnTimer_cnt++;
  if (timerID == kTimerID)
    UpdateGui();
  return true;
}


// **************** NanaZip Modification Start ****************
// Formats one bench result into the XAML status fields (the Win32
// PrintBenchRes writes the same data into dialog controls).
void CBenchmarkDialog::FormatBenchRes(
    const CTotalBenchRes2 &info,
    K7_BENCHMARK_STATUS &Status,
    bool enc,
    bool current)
{
  if (info.NumIterations2 == 0)
    return;

  WCHAR tmp[64];

  // Usage
  {
    wchar_t w[32];
    wchar_t *p = ConvertUInt64ToString(
        GetUsagePercents(info.Usage / info.NumIterations2), w);
    p[0] = '%';
    p[1] = 0;
    wcscpy_s(tmp, Z7_ARRAY_SIZE(tmp), w);
  }
  if (enc && current)
    wcscpy_s(Status.EncUsage1, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);
  else if (enc)
    wcscpy_s(Status.EncUsage2, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);
  else if (current)
    wcscpy_s(Status.DecUsage1, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);
  else
    wcscpy_s(Status.DecUsage2, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);

  // Speed
  {
    ConvertUInt64ToString((info.Speed >> 10) / info.NumIterations2, tmp);
    lstrcat(tmp, kKBs);
  }
  if (enc && current)
    wcscpy_s(Status.EncSpeed1, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);
  else if (enc)
    wcscpy_s(Status.EncSpeed2, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);
  else if (current)
    wcscpy_s(Status.DecSpeed1, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);
  else
    wcscpy_s(Status.DecSpeed2, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);

  // Rating
  {
    MyStringCopy(NumberToDot3(
        GetMips(info.Rating / info.NumIterations2), tmp), L" GIPS");
  }
  if (enc && current)
    wcscpy_s(Status.EncRating1, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);
  else if (enc)
    wcscpy_s(Status.EncRating2, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);
  else if (current)
    wcscpy_s(Status.DecRating1, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);
  else
    wcscpy_s(Status.DecRating2, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);

  // RPU
  {
    MyStringCopy(NumberToDot3(
        GetMips(info.RPU / info.NumIterations2), tmp), L" GIPS");
  }
  if (enc && current)
    wcscpy_s(Status.EncRpu1, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);
  else if (enc)
    wcscpy_s(Status.EncRpu2, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);
  else if (current)
    wcscpy_s(Status.DecRpu1, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);
  else
    wcscpy_s(Status.DecRpu2, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);

  // Size
  {
    UInt64 val = info.UnpackSize;
    LPCTSTR kPostfix;
    if (val >= ((UInt64)1 << 40))
    {
      kPostfix = kGB;
      val >>= 30;
    }
    else
    {
      kPostfix = kMB;
      val >>= 20;
    }
    ConvertUInt64ToString(val, tmp);
    lstrcat(tmp, kPostfix);
  }
  if (enc && current)
    wcscpy_s(Status.EncSize1, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);
  else if (enc)
    wcscpy_s(Status.EncSize2, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);
  else if (current)
    wcscpy_s(Status.DecSize1, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);
  else
    wcscpy_s(Status.DecSize2, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);
}
// **************** NanaZip Modification End ****************

void CBenchmarkDialog::UpdateGui()
{
  // **************** NanaZip Modification Start ****************
  K7_BENCHMARK_STATUS Status = {};

  // Elapsed time (the Win32 PrintTime does the same formatting).
  {
    const UInt32 curTime =
      _finishTime_WasSet ?
        _finishTime :
        ::GetTickCount();
    const UInt32 elapsedTime = (curTime - _startTime);
    WCHAR s[64];
    WCHAR *p = ConvertUInt32ToString(elapsedTime / 1000, s);
    if (_finishTime_WasSet)
    {
      *p++ = '.';
      UINT_TO_STR_3(p, elapsedTime % 1000)
    }
    MyStringCopy(p, L" s");
    wcscpy_s(Status.Elapsed, K7_BENCH_MAX_SHORT_TEXT_LENGTH, s);
  }

  if (TotalMode)
  {
    bool wasChanged = false;
    {
      NWindows::NSynchronization::CCriticalSectionLock lock(Sync.CS);
      if (Sync.TextWasChanged)
      {
        wasChanged = true;
        Bench2Text += Sync.Text;
        Sync.Text.Empty();
        Sync.TextWasChanged = false;
      }
    }
    if (wasChanged)
    {
      CopyTruncatedStr(Status.Log, K7_BENCH_MAX_LOG_LENGTH,
          UString(Bench2Text.Ptr()));
    }
    Status.Running = WasStopped_in_GUI ? FALSE : TRUE;
  }
  else
  {
    CSyncData sd;
    CRecordVector<CBenchPassResult> RatingVector;

    {
      NWindows::NSynchronization::CCriticalSectionLock lock(Sync.CS);
      sd = Sync.sd;

      if (sd.NeedPrint_RatingVector)
        RatingVector = Sync.RatingVector;

      if (sd.NeedPrint_Freq)
      {
        Sync.FreqString_GUI = Sync.FreqString_Sync;
        sd.NeedPrint_RatingVector = true;
      }

      Sync.sd.NeedPrint_RatingVector = false;
      Sync.sd.NeedPrint_Enc_1 = false;
      Sync.sd.NeedPrint_Enc   = false;
      Sync.sd.NeedPrint_Dec_1 = false;
      Sync.sd.NeedPrint_Dec   = false;
      Sync.sd.NeedPrint_Tot   = false;
      Sync.sd.NeedPrint_Freq = false;
    }

    Status.PassesFinished = sd.NumPasses_Finished;

    if (sd.NeedPrint_Enc_1) FormatBenchRes(sd.Enc_BenchRes_1, Status, true, true);
    if (sd.NeedPrint_Enc)   FormatBenchRes(sd.Enc_BenchRes,   Status, true, false);
    if (sd.NeedPrint_Dec_1) FormatBenchRes(sd.Dec_BenchRes_1, Status, false, true);
    if (sd.NeedPrint_Dec)   FormatBenchRes(sd.Dec_BenchRes,   Status, false, false);

    if (sd.BenchWasFinished && sd.NeedPrint_Tot)
    {
      CTotalBenchRes2 tot_BenchRes = sd.Enc_BenchRes;
      tot_BenchRes.Update_With_Res2(sd.Dec_BenchRes);
      if (tot_BenchRes.NumIterations2 != 0)
      {
        WCHAR tmp[64];
        MyStringCopy(NumberToDot3(
            GetMips(tot_BenchRes.Rating / tot_BenchRes.NumIterations2),
            tmp), L" GIPS");
        wcscpy_s(Status.TotalRating, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);
        MyStringCopy(NumberToDot3(
            GetMips(tot_BenchRes.RPU / tot_BenchRes.NumIterations2),
            tmp), L" GIPS");
        wcscpy_s(Status.TotalRpu, K7_BENCH_MAX_SHORT_TEXT_LENGTH, tmp);
        {
          wchar_t w[32];
          wchar_t *p = ConvertUInt64ToString(
              GetUsagePercents(tot_BenchRes.Usage / tot_BenchRes.NumIterations2),
              w);
          p[0] = '%';
          p[1] = 0;
          wcscpy_s(Status.TotalUsage, K7_BENCH_MAX_SHORT_TEXT_LENGTH, w);
        }
      }
    }

    if (sd.NeedPrint_RatingVector)
    {
      UString s;
      s += Sync.FreqString_GUI;
      if (!RatingVector.IsEmpty())
      {
        if (!s.IsEmpty())
          s.Add_LF();
        s += "Compr Decompr Total   CPU"
            #ifdef PRINT_ITER_TIME
            "   Time"
            #endif
            ;
        s.Add_LF();
      }
      for (unsigned i = 0; i < RatingVector.Size(); i++)
      {
        if (i != 0)
          s.Add_LF();
        if ((int)i == sd.RatingVector_DeletedIndex)
        {
          s += "...";
          s.Add_LF();
        }
        const CBenchPassResult &pair = RatingVector[i];
        AddRatingsLine(s, pair.Enc, pair.Dec
            #ifdef PRINT_ITER_TIME
            , pair.Ticks
            #endif
            );
      }

      if (sd.BenchWasFinished)
      {
        s.Add_LF();
        s += "-------------";
        s.Add_LF();
        {
          AddRatingsLine(s, sd.Enc_BenchRes, sd.Dec_BenchRes
                #ifdef PRINT_ITER_TIME
                , (DWORD)(sd.TotalTicks / (sd.NumPasses_Finished ? sd.NumPasses_Finished : 1))
                #endif
                );
        }
      }
      CopyTruncatedStr(Status.Log, K7_BENCH_MAX_LOG_LENGTH, s);
    }

    Status.Running = (!sd.BenchWasFinished && !WasStopped_in_GUI) ? TRUE : FALSE;
    Status.Finished = sd.BenchWasFinished ? TRUE : FALSE;
    Status.Stopped = WasStopped_in_GUI ? TRUE : FALSE;
  }

  if (this->m_WindowHandle)
  {
    ::K7ModernUpdateBenchmarkStatus(this->m_WindowHandle, &Status);
  }
  // **************** NanaZip Modification End ****************
}


bool CBenchmarkDialog::OnCommand(unsigned code, unsigned itemID, LPARAM lParam)
{
  if (code == CBN_SELCHANGE &&
      (itemID == IDC_BENCH_DICTIONARY ||
       itemID == IDC_BENCH_NUM_PASSES ||
       itemID == IDC_BENCH_NUM_THREADS))
  {
    RestartBenchmark();
    return true;
  }
  return CModalDialog::OnCommand(code, itemID, lParam);
}


bool CBenchmarkDialog::OnButtonClicked(unsigned buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDB_RESTART:
      RestartBenchmark();
      return true;
    case IDB_STOP:
      OnStopButton();
      return true;
  }
  return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
}





// ---------- Benchmark Thread ----------

struct CBenchCallback Z7_final: public IBenchCallback
{
  UInt64 dictionarySize;
  CBenchProgressSync *Sync;
  CBenchmarkDialog *BenchmarkDialog;
  
  HRESULT SetEncodeResult(const CBenchInfo &info, bool final) Z7_override;
  HRESULT SetDecodeResult(const CBenchInfo &info, bool final) Z7_override;
};

HRESULT CBenchCallback::SetEncodeResult(const CBenchInfo &info, bool final)
{
  bool needPost = false;
  {
    NSynchronization::CCriticalSectionLock lock(Sync->CS);
    if (Sync->Exit)
      return E_ABORT;
    CSyncData &sd = Sync->sd;
    // sd.NumEncProgress++;
    CTotalBenchRes2 &br = sd.Enc_BenchRes_1;
    {
      UInt64 dictSize = Sync->DictSize;
      if (final)
      {
        // sd.EncInfo = info;
      }
      else
      {
        /* if (!final), then CBenchInfo::NumIterations means totalNumber of threads.
           so we can reduce the dictionary */
        if (dictSize > info.UnpackSize)
          dictSize = info.UnpackSize;
      }
      br.Rating = info.GetRating_LzmaEnc(dictSize);
    }
    br.SetFrom_BenchInfo(info);
    sd.NeedPrint_Enc_1 = true;
    if (final)
    {
      sd.Enc_BenchRes.Update_With_Res2(br);
      sd.NeedPrint_Enc = true;
      needPost = true;
    }
  }

  if (needPost)
    BenchmarkDialog->PostMsg(k_Message_Finished, k_Msg_WPARM_Enc1_Finished);

  return S_OK;
}


HRESULT CBenchCallback::SetDecodeResult(const CBenchInfo &info, bool final)
{
  NSynchronization::CCriticalSectionLock lock(Sync->CS);
  if (Sync->Exit)
    return E_ABORT;
  CSyncData &sd = Sync->sd;
  // sd.NumDecProgress++;
  CTotalBenchRes2 &br = sd.Dec_BenchRes_1;
  br.Rating = info.GetRating_LzmaDec();
  br.SetFrom_BenchInfo(info);
  sd.NeedPrint_Dec_1 = true;
  if (final)
    sd.Dec_BenchRes.Update_With_Res2(br);
  return S_OK;
}


struct CBenchCallback2 Z7_final: public IBenchPrintCallback
{
  CBenchProgressSync *Sync;
  bool TotalMode;

  void Print(const char *s) Z7_override;
  void NewLine() Z7_override;
  HRESULT CheckBreak() Z7_override;
};

void CBenchCallback2::Print(const char *s)
{
  if (TotalMode)
  {
    NSynchronization::CCriticalSectionLock lock(Sync->CS);
    Sync->Text += s;
    Sync->TextWasChanged = true;
  }
}

void CBenchCallback2::NewLine()
{
  Print("\xD\n");
}

HRESULT CBenchCallback2::CheckBreak()
{
  if (Sync->Exit)
    return E_ABORT;
  return S_OK;
}



struct CFreqCallback Z7_final: public IBenchFreqCallback
{
  CBenchmarkDialog *BenchmarkDialog;

  virtual HRESULT AddCpuFreq(unsigned numThreads, UInt64 freq, UInt64 usage) Z7_override;
  virtual HRESULT FreqsFinished(unsigned numThreads) Z7_override;
};

HRESULT CFreqCallback::AddCpuFreq(unsigned numThreads, UInt64 freq, UInt64 usage)
{
  HRESULT res;
  {
    CBenchProgressSync &sync = BenchmarkDialog->Sync;
    NSynchronization::CCriticalSectionLock lock(sync.CS);
    UString &s = sync.FreqString_Sync;
    if (sync.NumFreqThreadsPrev != numThreads)
    {
      sync.NumFreqThreadsPrev = numThreads;
      if (!s.IsEmpty())
        s.Add_LF();
      s.Add_UInt32(numThreads);
      s += "T Frequency (MHz):";
      s.Add_LF();
    }
    s.Add_Space();
    if (numThreads != 1)
    {
      s.Add_UInt64(GetUsagePercents(usage));
      s.Add_Char('%');
      s.Add_Space();
    }
    s.Add_UInt64(GetMips(freq));
    // BenchmarkDialog->Sync.sd.NeedPrint_Freq = true;
    res = sync.Exit ? E_ABORT : S_OK;
  }
  // BenchmarkDialog->PostMsg(k_Message_Finished, k_Msg_WPARM_Enc1_Finished);
  return res;
}

HRESULT CFreqCallback::FreqsFinished(unsigned /* numThreads */)
{
  HRESULT res;
  {
    CBenchProgressSync &sync = BenchmarkDialog->Sync;
    NSynchronization::CCriticalSectionLock lock(sync.CS);
    sync.sd.NeedPrint_Freq = true;
    BenchmarkDialog->PostMsg(k_Message_Finished, k_Msg_WPARM_Enc1_Finished);
    res = sync.Exit ? E_ABORT : S_OK;
  }
  BenchmarkDialog->PostMsg(k_Message_Finished, k_Msg_WPARM_Enc1_Finished);
  return res;
}



// define USE_DUMMY only for debug
// #define USE_DUMMY
#ifdef USE_DUMMY
static unsigned dummy = 1;
static unsigned Dummy(unsigned limit)
{
  unsigned sum = 0;
  for (unsigned k = 0; k < limit; k++)
  {
    sum += dummy;
    if (sum == 0)
      break;
  }
  return sum;
}
#endif


HRESULT CThreadBenchmark::Process()
{
  /* the first benchmark pass can be slow,
     if we run benchmark while the window is being created,
     and (no freq detecion loop) && (dictionary is small) (-mtic is small) */
    
  // Sleep(300); // for debug
  #ifdef USE_DUMMY
  Dummy(1000 * 1000 * 1000); // for debug
  #endif

  CBenchProgressSync &sync = BenchmarkDialog->Sync;
  HRESULT finishHRESULT = S_OK;
  
  try
  {
    for (UInt32 passIndex = 0;; passIndex++)
    {
      // throw 1; // to debug
      // throw CSystemException(E_INVALIDARG); // to debug

      UInt64 dictionarySize;
      UInt32 numThreads;
      {
        NSynchronization::CCriticalSectionLock lock(sync.CS);
        if (sync.Exit)
          break;
        dictionarySize = sync.DictSize;
        numThreads = sync.NumThreads;
      }

      #ifdef PRINT_ITER_TIME
      const DWORD startTick = GetTickCount();
      #endif
      
      CBenchCallback callback;
      
      callback.dictionarySize = dictionarySize;
      callback.Sync = &sync;
      callback.BenchmarkDialog = BenchmarkDialog;
      
      CBenchCallback2 callback2;
      callback2.TotalMode = BenchmarkDialog->TotalMode;
      callback2.Sync = &sync;
      
      CFreqCallback freqCallback;
      freqCallback.BenchmarkDialog = BenchmarkDialog;

      HRESULT result;
     
      try
      {
        CObjectVector<CProperty> props;

        props = BenchmarkDialog->Props;

        if (BenchmarkDialog->TotalMode)
        {
          props = BenchmarkDialog->Props;
        }
        else
        {
          {
            CProperty prop;
            prop.Name = "mt";
            prop.Value.Add_UInt32(numThreads);
            props.Add(prop);
          }
          {
            CProperty prop;
            prop.Name = 'd';
            prop.Name.Add_UInt32((UInt32)(dictionarySize >> 10));
            prop.Name.Add_Char('k');
            props.Add(prop);
          }
        }
        
        result = Bench(EXTERNAL_CODECS_LOC_VARS
            BenchmarkDialog->TotalMode ? &callback2 : NULL,
            BenchmarkDialog->TotalMode ? NULL : &callback,
            props, 1, false,
            (!BenchmarkDialog->TotalMode) && passIndex == 0 ? &freqCallback: NULL);
        
        // result = S_FALSE; // for debug;
        // throw 1;
      }
      catch(...)
      {
        result = E_FAIL;
      }

      #ifdef PRINT_ITER_TIME
      const DWORD numTicks = GetTickCount() - startTick;
      #endif

      bool finished = true;

      NSynchronization::CCriticalSectionLock lock(sync.CS);

      if (result != S_OK)
      {
        sync.BenchFinish_Task_HRESULT = result;
        break;
      }

      {
        CSyncData &sd = sync.sd;

        sd.NumPasses_Finished++;
        #ifdef PRINT_ITER_TIME
        sd.TotalTicks += numTicks;
        #endif

        if (BenchmarkDialog->TotalMode)
          break;

        {
          CTotalBenchRes tot_BenchRes = sd.Enc_BenchRes_1;
          tot_BenchRes.Update_With_Res(sd.Dec_BenchRes_1);

          sd.NeedPrint_RatingVector = true;
          {
            CBenchPassResult pair;
            // pair.EncInfo = sd.EncInfo; // for debug
            pair.Enc = sd.Enc_BenchRes_1;
            pair.Dec = sd.Dec_BenchRes_1;
            #ifdef PRINT_ITER_TIME
            pair.Ticks = numTicks;
            #endif
            sync.RatingVector.Add(pair);
            // pair.Dec_Defined = true;
          }
        }
          
        sd.NeedPrint_Dec = true;
        sd.NeedPrint_Tot = true;

        if (sync.RatingVector.Size() > kRatingVector_NumBundlesMax)
        {
          // sd.RatingVector_NumDeleted++;
          sd.RatingVector_DeletedIndex = (int)(kRatingVector_NumBundlesMax / 4);
          sync.RatingVector.Delete((unsigned)(sd.RatingVector_DeletedIndex));
        }

        if (sync.sd.NumPasses_Finished < sync.NumPasses_Limit)
          finished = false;
        else
        {
          sync.sd.BenchWasFinished = true;
          // BenchmarkDialog->_finishTime = GetTickCount();
          // return 0;
        }
      }

      if (BenchmarkDialog->TotalMode)
        break;

      /*
      if (newTick - prevTick < 1000)
        numSameTick++;
      if (numSameTick > 5 || finished)
      {
        prevTick = newTick;
        numSameTick = 0;
      */
      // for (unsigned i = 0; i < 1; i++)
      {
        // we suppose that PostMsg messages will be processed in order.
        if (!BenchmarkDialog->PostMsg_Finish(k_Msg_WPARM_Iter_Finished))
        {
          finished = true;
          finishHRESULT = E_FAIL;
          // throw 1234567;
        }
      }
      if (finished)
        break;
    }
    // return S_OK;
  }
  catch(CSystemException &e)
  {
    finishHRESULT = e.ErrorCode;
    // BenchmarkDialog->MessageBoxError(HResultToMessage(e.ErrorCode));
    // return E_FAIL;
  }
  catch(...)
  {
    finishHRESULT = E_FAIL;
    // BenchmarkDialog->MessageBoxError(HResultToMessage(E_FAIL));
    // return E_FAIL;
  }

  if (finishHRESULT != S_OK)
  {
    NSynchronization::CCriticalSectionLock lock(sync.CS);
    sync.BenchFinish_Thread_HRESULT = finishHRESULT;
  }
  if (!BenchmarkDialog->PostMsg_Finish(k_Msg_WPARM_Thread_Finished))
  {
    // sync.BenchFinish_Thread_HRESULT = E_FAIL;
  }
  return 0;
}



static void ParseNumberString(const UString &s, NCOM::CPropVariant &prop)
{
  const wchar_t *end;
  UInt64 result = ConvertStringToUInt64(s, &end);
  if (*end != 0 || s.IsEmpty())
    prop = s;
  else if (result <= (UInt32)0xFFFFFFFF)
    prop = (UInt32)result;
  else
    prop = result;
}


HRESULT Benchmark(
    DECL_EXTERNAL_CODECS_LOC_VARS
    const CObjectVector<CProperty> &props, UInt32 numIterations, HWND hwndParent)
{
  CBenchmarkDialog bd;

  bd.TotalMode = false;
  bd.Props = props;
  if (numIterations == 0)
    numIterations = 1;
  bd.Sync.NumPasses_Limit = numIterations;
  bd.Sync.DictSize = (UInt64)(Int64)-1;
  bd.Sync.NumThreads = (UInt32)(Int32)-1;
  bd.Sync.Level = -1;

  COneMethodInfo method;

  UInt32 numCPUs = 1;
  #ifndef Z7_ST
  numCPUs = NSystem::GetNumberOfProcessors();
  #endif
  UInt32 numThreads = numCPUs;

  FOR_VECTOR (i, props)
  {
    const CProperty &prop = props[i];
    UString name = prop.Name;
    name.MakeLower_Ascii();
    if (name.IsEqualTo_Ascii_NoCase("m") && prop.Value.IsEqualTo("*"))
    {
      bd.TotalMode = true;
      continue;
    }

    NCOM::CPropVariant propVariant;
    if (!prop.Value.IsEmpty())
      ParseNumberString(prop.Value, propVariant);
    if (name.IsPrefixedBy("mt"))
    {
      #ifndef Z7_ST
      RINOK(ParseMtProp(name.Ptr(2), propVariant, numCPUs, numThreads))
      if (numThreads != numCPUs)
        bd.Sync.NumThreads = numThreads;
      #endif
      continue;
    }
    /*
    if (name.IsEqualTo("time"))
    {
      // UInt32 testTime = 4;
      // RINOK(ParsePropToUInt32(L"", propVariant, testTime));
      continue;
    }
    RINOK(method.ParseMethodFromPROPVARIANT(name, propVariant));
    */
    // here we need to parse DictSize property, and ignore unknown properties
    method.ParseMethodFromPROPVARIANT(name, propVariant);
  }

  if (bd.TotalMode)
  {
    // bd.Bench2Text.Empty();
    // **************** NanaZip Modification Start ****************
    //bd.Bench2Text = "7-Zip " MY_VERSION_CPU;
    bd.Bench2Text = "NanaZip " MILE_PROJECT_VERSION_UTF8_STRING " (" MY_CPU_NAME ")";
    // **************** NanaZip Modification End ****************
    // bd.Bench2Text.Add_Char((char)0xD);
    bd.Bench2Text.Add_LF();
  }

  {
    UInt64 dict;
    if (method.Get_DicSize(dict))
      bd.Sync.DictSize = dict;
  }
  bd.Sync.Level = (int)method.GetLevel();

  // Dummy(1000 * 1000 * 1);

  {
    CThreadBenchmark &benchmarker = bd._threadBenchmark;
    #ifdef Z7_EXTERNAL_CODECS
    benchmarker._externalCodecs = _externalCodecs;
    #endif
    benchmarker.BenchmarkDialog = &bd;
  }

  bd.Create(hwndParent);

  return S_OK;
}


CBenchmarkDialog::~CBenchmarkDialog()
{
  if (_thread.IsCreated())
  {
    /* the following code will be not executed in normal code flow.
       it can be called, if there is some internal failure in dialog code. */
    Attach(NULL);
    MessageBoxError(L"The flaw in benchmark thread code");
    Sync.SendExit();
    _thread.Wait_Close();
  }
}
// **************** NanaZip Modification Start ****************
bool CBenchmarkDialog::ModernMessageRouter(UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
  case WM_COMMAND:
  {
    int Code = HIWORD(wParam);
    int ItemID = LOWORD(wParam);
    if (BN_CLICKED == Code)
    {
      if (K7_BENCH_WINDOW_COMMAND_RESTART == ItemID)
      {
        RestartBenchmark();
        return true;
      }
      if (K7_BENCH_WINDOW_COMMAND_STOP == ItemID)
      {
        OnStopButton();
        return true;
      }
      if (K7_BENCH_WINDOW_COMMAND_CANCEL == ItemID)
      {
        OnCancel();
        return true;
      }
    }
    return false;
  }
  case K7_BENCH_WINDOW_MSG_SET_DICTIONARY:
  {
    const UInt32 index = (UInt32)wParam;
    if (index < m_DictSizesCount)
    {
      NWindows::NSynchronization::CCriticalSectionLock lock(Sync.CS);
      Sync.DictSize = m_DictSizes[index];
    }
    return true;
  }
  case K7_BENCH_WINDOW_MSG_SET_THREADS:
  {
    const UInt32 index = (UInt32)wParam;
    if (index < m_ThreadValuesCount)
    {
      NWindows::NSynchronization::CCriticalSectionLock lock(Sync.CS);
      Sync.NumThreads = m_ThreadValues[index];
    }
    return true;
  }
  case K7_BENCH_WINDOW_MSG_SET_PASSES:
  {
    const UInt32 index = (UInt32)wParam;
    if (index < m_PassesValuesCount)
    {
      NWindows::NSynchronization::CCriticalSectionLock lock(Sync.CS);
      Sync.NumPasses_Limit = m_PassesValues[index];
    }
    return true;
  }
  case WM_KEYDOWN:
  {
    // Esc asks to cancel (stops the thread and closes after the cleanup,
    // like the Win32 OnCancel).
    if (VK_ESCAPE == wParam)
    {
      OnCancel();
      return true;
    }
    return false;
  }
  case WM_TIMER:
  {
    if (wParam == kTimerID)
    {
      UpdateGui();
      return true;
    }
    return false;
  }
  case k_Message_Finished:
  {
    return OnMessage(k_Message_Finished, wParam, lParam);
  }
  case WM_CLOSE:
  {
    // Esc / X: stop the thread first; the window closes after the cleanup
    // (OnMessage posts WM_CLOSE again once the thread is gone).
    if (_thread.IsCreated())
    {
      OnCancel();
      return true;
    }
    // **************** NanaZip Modification Start ****************
    // Persist the window position before the window is destroyed.
    SaveBenchmarkWindowPosition(m_WindowHandle);
    // **************** NanaZip Modification End ****************
    return false;
  }
  default:
    break;
  }
  return false;
}

LRESULT CALLBACK CBenchmarkDialog::ModernWindowHandler(
    _In_ HWND hWnd,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam,
    _In_ UINT_PTR uIdSubclass,
    _In_ DWORD_PTR dwRefData)
{
  UNREFERENCED_PARAMETER(uIdSubclass);

  CBenchmarkDialog *Instance =
      reinterpret_cast<CBenchmarkDialog*>(dwRefData);

  if (Instance)
  {
    if (!Instance->m_FirstRun)
    {
      Instance->m_FirstRun = true;
      Instance->m_WindowHandle = hWnd;
      // Start the refresh timer (the Win32 OnInit does the same).
      Instance->_timer = ::SetTimer(hWnd, kTimerID, kTimerElapse, NULL);
    }
    if (Instance->ModernMessageRouter(uMsg, wParam, lParam))
    {
      return 0;
    }
  }

  return ::DefSubclassProc(
      hWnd,
      uMsg,
      wParam,
      lParam);
}
// **************** NanaZip Modification End ****************
