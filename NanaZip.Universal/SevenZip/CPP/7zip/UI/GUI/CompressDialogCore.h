// CompressDialogCore.h
// 压缩对话框规则层：无 UI 依赖，Win32 壳（CCompressDialog）与 XAML 壳共用。
// 规则只在这里存在一份：格式/等级/方法/字典/固实/线程/内存联动、内存估算、校验提交。

#ifndef ZIP7_INC_COMPRESS_DIALOG_CORE_H
#define ZIP7_INC_COMPRESS_DIALOG_CORE_H

#include "../../../Common/Wildcard.h"

#include "../Common/LoadCodecs.h"
#include "../Common/ZipRegistry.h"

#include "CompressDialogRes.h"
#include "ExtractRes.h"

namespace NCompressDialog
{
  namespace NUpdateMode
  {
    enum EEnum
    {
      kAdd,
      kUpdate,
      kFresh,
      kSync
    };
  }
  
  struct CInfo
  {
    NUpdateMode::EEnum UpdateMode;
    NWildcard::ECensorPathMode PathMode;

    bool SolidIsSpecified;
    // bool MultiThreadIsAllowed;
    UInt64 SolidBlockSize;
    UInt32 NumThreads;

    NCompression::CMemUse MemUsage;

    CRecordVector<UInt64> VolumeSizes;

    UInt32 Level;
    UString Method;
    UInt64 Dict64;
    // UInt64 Dict64_Chain;
    bool OrderMode;
    UInt32 Order;
    UString Options;
    // **************** 7-Zip ZS Modification Start ****************
    UString SplitVolume;
    // **************** 7-Zip ZS Modification End ****************

    UString EncryptionMethod;

    bool SFXMode;
    bool OpenShareForWrite;
    bool DeleteAfterCompressing;
    
    CBoolPair SymLinks;
    CBoolPair HardLinks;
    CBoolPair AltStreams;
    CBoolPair NtSecurity;

    CBoolPair PreserveATime;

    UInt32 TimePrec;
    CBoolPair MTime;
    CBoolPair CTime;
    CBoolPair ATime;
    CBoolPair SetArcMTime;
    
    UString ArcPath; // in: Relative or abs ; out: Relative or abs
    
    // FString CurrentDirPrefix;
    bool KeepName;

    bool GetFullPathName(UString &result) const;

    int FormatIndex;

    UString Password;
    bool EncryptHeadersIsAllowed;
    bool EncryptHeaders;

    CInfo():
        UpdateMode(NCompressDialog::NUpdateMode::kAdd),
        PathMode(NWildcard::k_RelatPath),
        SFXMode(false),
        OpenShareForWrite(false),
        DeleteAfterCompressing(false),
        FormatIndex(-1)
    {
      Level = Order = (UInt32)(Int32)-1;
      NumThreads = (UInt32)(Int32)-1;
      SolidIsSpecified = false;
      Dict64 = (UInt64)(Int64)(-1);
      // Dict64_Chain = (UInt64)(Int64)(-1);
      OrderMode = false;
      Method.Empty();
      Options.Empty();
      // **************** 7-Zip ZS Modification Start ****************
      SplitVolume.Empty();
      // **************** 7-Zip ZS Modification End ****************
      EncryptionMethod.Empty();
      TimePrec = (UInt32)(Int32)(-1);
    }
  };
}


struct CBool1
{
  bool Val;
  bool Supported;

  CBool1(): Val(false), Supported(false) {}
  
  void Init()
  {
    Val = false;
    Supported = false;
  }

  void SetTrueTrue()
  {
    Val = true;
    Supported = true;
  }

  void SetVal_as_Supported(bool val)
  {
    Val = val;
    Supported = true;
  }

  /*
  bool IsVal_True_and_Defined() const
  {
    return Def && Val;
  }
  */
};


// ================= 常量（原 CompressDialog.cpp 顶部 static 定义） =================

static const UInt32 kSolidLog_NoSolid = 0;
static const UInt32 kSolidLog_FullSolid = 64;

static const UInt32 kLzmaMaxDictSize = (UInt32)15 << 28;

static const UInt32 kFF_Filter      = 1 << 0;
static const UInt32 kFF_Solid       = 1 << 1;
static const UInt32 kFF_MultiThread = 1 << 2;
static const UInt32 kFF_Encrypt     = 1 << 3;
static const UInt32 kFF_EncryptFileNames  = 1 << 4;
static const UInt32 kFF_MemUse      = 1 << 5;
static const UInt32 kFF_SFX         = 1 << 6;

static const unsigned kHistorySize = 20;

enum EMethodID
{
  kCopy,
  kLZMA,
  kLZMA2,
  kPPMd,
  kBZip2,
  kDeflate,
  kDeflate64,
  kPPMdZip,
  // kZSTD,
  // **************** 7-Zip ZS Modification Start ****************
  kFLZMA2,
  kZSTD,
  kBROTLI,
  kLZ4,
  kLZ5,
  kLIZARD_M1,
  kLIZARD_M2,
  kLIZARD_M3,
  kLIZARD_M4,
  // **************** 7-Zip ZS Modification End ****************
  kSha256,
  kSha1,
  kCrc32,
  kCrc64,
  kGnu,
  kPosix
};

static const UInt32 g_Levels[] =
{
  IDS_METHOD_STORE,
  IDS_METHOD_FASTEST,
  // **************** 7-Zip ZS Modification Start ****************
  //0,
  // **************** 7-Zip ZS Modification End ****************
  IDS_METHOD_FAST,
  // **************** 7-Zip ZS Modification Start ****************
  //0,
  // **************** 7-Zip ZS Modification End ****************
  IDS_METHOD_NORMAL,
  // **************** 7-Zip ZS Modification Start ****************
  //0,
  // **************** 7-Zip ZS Modification End ****************
  IDS_METHOD_MAXIMUM,
  // **************** 7-Zip ZS Modification Start ****************
  //0,
  // **************** 7-Zip ZS Modification End ****************
  IDS_METHOD_ULTRA
};

// **************** 7-Zip ZS Modification Start ****************
static const signed char g_LevelRanges[][2] = {
  { -64, 22 }, // zstd
  { 0, 11 }, // brotli
  { 1, 12 }, // lz4
  { 1, 15 }, // lz5
  { 10, 19 }, // lizard m1
  { 20, 29 }, // lizard m2
  { 30, 39 }, // lizard m3
  { 40, 49 }, // lizard m4
};
// **************** 7-Zip ZS Modification End ****************

struct CFormatInfo
{
  LPCSTR Name;
  UInt32 LevelsMask;
  unsigned NumMethods;
  const EMethodID *MethodIDs;

  UInt32 Flags;

  bool Filter_() const { return (Flags & kFF_Filter) != 0; }
  bool Solid_() const { return (Flags & kFF_Solid) != 0; }
  bool MultiThread_() const { return (Flags & kFF_MultiThread) != 0; }
  bool Encrypt_() const { return (Flags & kFF_Encrypt) != 0; }
  bool EncryptFileNames_() const { return (Flags & kFF_EncryptFileNames) != 0; }
  bool MemUse_() const { return (Flags & kFF_MemUse) != 0; }
  bool SFX_() const { return (Flags & kFF_SFX) != 0; }
};

static LPCSTR const kExeExt = ".exe";

// **************** 7-Zip ZS Modification Start ****************
#define k7zFormat "7z"
// **************** 7-Zip ZS Modification End ****************

static const
  // NCompressDialog::NUpdateMode::EEnum
  int
  k_UpdateMode_Vals[] =
{
  NCompressDialog::NUpdateMode::kAdd,
  NCompressDialog::NUpdateMode::kUpdate,
  NCompressDialog::NUpdateMode::kFresh,
  NCompressDialog::NUpdateMode::kSync
};
  
static const UInt32 k_UpdateMode_IDs[] =
{
  IDS_COMPRESS_UPDATE_MODE_ADD,
  IDS_COMPRESS_UPDATE_MODE_UPDATE,
  IDS_COMPRESS_UPDATE_MODE_FRESH,
  IDS_COMPRESS_UPDATE_MODE_SYNC
};

static const
  // NWildcard::ECensorPathMode
  int
  k_PathMode_Vals[] =
{
  NWildcard::k_RelatPath,
  NWildcard::k_FullPath,
  NWildcard::k_AbsPath,
};

static const UInt32 k_PathMode_IDs[] =
{
  IDS_PATH_MODE_RELAT,
  IDS_EXTRACT_PATHS_FULL,
  IDS_EXTRACT_PATHS_ABS
};


// ================= 规则核心 =================

// 文件扩展名点位置（壳与 Core 共用）
int GetExtDotPos(const UString &s);

extern const wchar_t * const k_IncorrectPathMessage;

class CCompressDialogCore
{
public:
  // ---- 输入（由调用方设置；XAML 适配器用 Info 做快照进出） ----
  const CObjectVector<CArcInfoEx> *ArcFormats;
  CUIntVector ArcIndices;          // 不能为空，必须包含 Info.FormatIndex（若 >= 0）
  AStringVector ExternalMethods;
  NCompressDialog::CInfo Info;
  NCompression::CInfo RegistryInfo;
  UString OriginalFileName;        // for bzip2, gzip2
  bool KeepName;

  CBool1 SymLinks;
  CBool1 HardLinks;
  CBool1 AltStreams;
  CBool1 NtSecurity;
  CBool1 PreserveATime;

  // ---- 界面选择状态（语义值；-1/Auto 语义与原控件 item data 完全一致） ----
  int FormatIndex;                 // 当前选中格式的 arcIndex
  int PrevFormat;                  // 上一次格式（SetArchiveName2 用）
  UInt32 Level;                    // (UInt32)(Int32)-1 = Auto
  int MethodID;                    // -1 = Auto（第一项）
  UInt64 Dict64;                   // (UInt64)(Int64)-1 = Auto
  UInt32 Order;                    // -1 = Auto
  UInt32 BlockLogSize;             // -1 = Auto；0 = 非固实；64 = 全固实
  UInt32 NumThreads;               // -1 = Auto
  int MemUseIndex;                 // 内存项在 MemUseStrings 中的索引
  int EncryptionMethodIndex;       // 加密方法项索引
  int DefaultEncryptionMethodIndex;
  bool ShowPassword;
  bool SfxChecked;
  bool OpenShareForWrite;
  bool DeleteAfterCompressing;
  bool EncryptHeadersChecked;
  UString Password;
  UString PasswordConfirmation;
  UString VolumeText;
  UString DirPrefix;               // 归档所在目录
  UString StartDirPrefix;          // 打开对话框时的目录（GetFinalPath_Smart 兜底）
  UString ArchiveName;             // 归档文件名（不含目录）

  // ---- Auto 值（联动计算） ----
  int AutoMethodId;
  UInt32 AutoDict;                 // (UInt32)(Int32)-1 表示未知
  UInt32 AutoOrder;
  UInt64 AutoSolid;
  UInt32 AutoNumThreads;

  // ---- RAM ----
  bool RamSizeDefined;
  size_t RamSize;                  // 完整 RAM
  size_t RamSizeReduced;           // 64 位完整 / 32 位削减
  UInt64 RamUsageAuto;             // 自动内存限制（80%）

  // ---- 动态列表（Display=显示文本，Value=语义值=原 item data） ----
  struct COptionItem
  {
    UString Display;
    UInt64 Value;
    COptionItem(): Value(0) {}
    COptionItem(const UString &display, UInt64 value): Display(display), Value(value) {}
  };
  CObjectVector<COptionItem> FormatItems;          // Value = arcIndex
  CObjectVector<COptionItem> LevelItems;           // Value = 等级
  CObjectVector<COptionItem> MethodItems;          // Value = methodID（-1 = Auto）
  CObjectVector<COptionItem> DictionaryItems;      // Value = 字典字节（-1 = Auto）
  CObjectVector<COptionItem> OrderItems;           // Value = order（-1 = Auto）
  CObjectVector<COptionItem> SolidItems;           // Value = log2（-1 = Auto）
  CObjectVector<COptionItem> ThreadItems;          // Value = 线程数（-1 = Auto）
  CObjectVector<COptionItem> MemUseItems;          // Value = MemUseStrings 索引
  CObjectVector<COptionItem> EncryptionMethodItems;
  UStringVector MemUseStrings;

  // ---- 能力标志 ----
  bool SolidSupported;
  bool MultiThreadSupported;
  bool EncryptSupported;
  bool EncryptFileNamesSupported;
  bool MemUseSupported;
  bool SfxSupported;               // 当前格式 + 方法支持 SFX

  // ---- 显示文本（壳负责画到控件） ----
  UString HardwareThreadsText;     // "N" 或 "N / M"
  UString MemoryValueText;         // "X MB / Y MB / Z MB"
  UString DecompressMemoryText;
  UString OptionsSummaryText;      // Options 按钮旁边的摘要行

  UString VolumeConfirmText;       // 小分卷确认文本（ValidateAndCommit 填充）
  bool VolumeConfirmed;            // 小分卷确认已通过（壳确认后置位再重试提交）

public:
  CCompressDialogCore();

  void Clear();

  // ---- 输入 - 外部方法收集（原 SetMethods） ----
  void SetMethods(const CObjectVector<CCodecInfoUser> &userCodecs);

  // ---- 初始化 ----
  void Initialize();               // RAM 读取 + RegistryInfo.Load()（Info 须已填）
  void CalcFormats();              // 格式列表生成 + 初始选中

  // ---- 列表生成（规则） ----
  void CalcLevels();
  void CalcMethods(int keepMethodId = -1);
  void CalcEncryptionMethod();
  void CalcDictionary();
  void CalcOrder();
  void CalcSolidBlockSize();
  void CalcThreads();
  void CalcMemUse();

  // ---- 联动 ----
  void FormatChanged(bool isChanged);
  void CheckSFX();                 // 更新 SfxSupported
  void ComprMethodChanged();
  void CheckSFXNameChange();
  bool IsSfx() const { return SfxSupported && SfxChecked; }
  void SetSfxChecked(bool val) { SfxChecked = val; }
  void SetArchiveName(const UString &name);
  void SetArchiveName2(bool prevWasSFX);
  bool ArcPathChanged(const UString &path);   // 返回是否切换了格式

  // ---- 路径 ----
  bool SetArcPathFields(const UString &path, UString &name, bool always);
  bool GetFinalPath_Smart(UString &resPath) const;

  // ---- 取值（替代原控件读取） ----
  UInt32 GetLevel() const { return Level; }
  UInt32 GetLevelSpec() const { return (Level == (UInt32)(Int32)-1) ? 1 : Level; }
  UInt32 GetLevel2() const { return (Level == (UInt32)(Int32)-1) ? 5 : Level; }
  UInt64 GetDictSpec() const { return Dict64; }
  UInt64 GetDict2() const;
  UInt32 GetOrderSpec() const { return Order; }
  UInt32 GetNumThreadsSpec() const { return NumThreads; }
  UInt32 GetNumThreads2() const;
  UInt32 GetBlockSizeSpec() const { return BlockLogSize; }
  int GetMethodID() const { return (MethodID < 0) ? AutoMethodId : MethodID; }
  UString GetMethodSpec(UString &estimatedName) const;
  UString GetMethodSpec() const;
  bool IsMethodEqualTo(const UString &s) const;
  UString GetEncryptionMethodSpec() const;
  UString Get_MemUse_Spec() const;
  UInt64 Get_MemUse_Bytes() const;
  bool GetOrderMode() const;

  // ---- 内存估算 ----
  UInt64 GetMemoryUsage_DecompMem(UInt64 &decompressMemory) const;
  UInt64 GetMemoryUsage_Dict_DecompMem(UInt64 dict, UInt64 &decompressMemory) const;
  UInt64 GetMemoryUsage_Threads_Dict_DecompMem(UInt32 numThreads, UInt64 dict, UInt64 &decompressMemory) const;

  // ---- 格式 / 注册表 ----
  unsigned GetFormatIndex() const { return (unsigned)FormatIndex; }
  bool IsZipFormat() const;
  bool IsXzFormat() const;
  unsigned GetStaticFormatIndex() const;
  const CArcInfoEx &Get_ArcInfoEx() const { return (*ArcFormats)[GetFormatIndex()]; }
  NCompression::CFormatOptions &Get_FormatOptions();
  int FindRegistryFormat(const UString &name) const;
  unsigned FindRegistryFormat_Always(const UString &name);

  // ---- 选项保存 ----
  void SaveOptionsInMem();

  // ---- 校验 + 提交（原 OnOK 拆分） ----
  enum ECommitResult
  {
    kCommitOk,
    kCommitBlocked,               // errorMessage 已填
    kCommitNeedVolumeConfirm      // 需小分卷确认（VolumeConfirmText 已填）
  };
  ECommitResult ValidateAndCommit(UString &errorMessage);

  // ---- 列表辅助 ----
  int FindExactIndex(const CObjectVector<COptionItem> &items, UInt64 value) const;
  int FindNearestIndex(const CObjectVector<COptionItem> &items, UInt64 value) const;

  // ---- 选择命令（XAML 与 Win32 壳共用入口；内部执行联动链） ----
  void OnFormatSelected(int arcIndex);
  void OnLevelSelected(UInt32 value);
  void OnMethodSelected(int methodId);
  void OnDictionarySelected(UInt64 value);
  void OnOrderSelected(UInt32 value);
  void OnSolidSelected(UInt32 value);
  void OnThreadsSelected(UInt32 value);
  void OnMemUseSelected(int memUseStringsIndex);
  void OnEncryptionMethodSelected(int itemIndex);
  void OnSfxChecked(bool checked);
  void OnUpdateModeSelected(int value) { Info.UpdateMode = (NCompressDialog::NUpdateMode::EEnum)value; }
  void OnPathModeSelected(int value) { Info.PathMode = (NWildcard::ECensorPathMode)value; }

  // ---- 显示文本生成 ----
  void UpdateMemoryTexts();
  void UpdateOptionsSummary();
};

#endif
