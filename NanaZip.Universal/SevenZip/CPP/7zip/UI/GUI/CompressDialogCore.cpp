// CompressDialogCore.cpp
// 压缩对话框规则层实现。从 CompressDialog.cpp 逐函数迁移，行为与原版逐行等价。

#include "StdAfx.h"

#include "../../../../C/CpuArch.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/System.h"

#include "../../Common/MethodProps.h"
#include "../../ICoder.h"

#include "../Common/ZipRegistry.h"

#include "../FileManager/FormatUtils.h"
#include "../FileManager/LangUtils.h"
#include "../FileManager/PropertyName.h"
#include "../FileManager/SplitUtils.h"
#include "../FileManager/resourceGui.h"

#include "CompressDialogCore.h"
#include "CompressDialogRes.h"
#include "ExtractRes.h"
#include "resource2.h"

using namespace NWindows;
using namespace NFile;
using namespace NName;
using namespace NDir;

// in ExtractDialog.cpp
extern void AddUniqueString(UStringVector &strings, const UString &srcString);

const wchar_t * const k_IncorrectPathMessage = L"Incorrect archive path";


static LPCSTR const kMethodsNames[] =
{
    "Copy"
  , "LZMA"
  , "LZMA2"
  , "PPMd"
  , "BZip2"
  , "Deflate"
  , "Deflate64"
  , "PPMd"
  // , "ZSTD"
  // **************** 7-Zip ZS Modification Start ****************
  , "FLZMA2"
  , "zstd"
  , "Brotli"
  , "LZ4"
  , "LZ5"
  , "Lizard"
  , "Lizard"
  , "Lizard"
  , "Lizard"
  // **************** 7-Zip ZS Modification End ****************
  , "SHA256"
  , "SHA1"
  , "CRC32"
  , "CRC64"
  , "GNU"
  , "POSIX"
};

// **************** 7-Zip ZS Modification Start ****************
static LPCSTR const kMethodsNamesLong[] =
{
    "Copy [std]"
  , "LZMA [std]"
  , "LZMA2 [std]"
  , "PPMd [std]"
  , "BZip2 [std]"
  , "Deflate [std]"
  , "Deflate64 [std]"
  , "PPMd [std]"
  , "LZMA2, Fast [std]"
  , "Zstandard"
  , "Brotli"
  , "LZ4"
  , "LZ5"
  , "Lizard, FastLZ4"
  , "Lizard, LIZv1"
  , "Lizard, FastLZ4 + Huffman"
  , "Lizard, LIZv1 + Huffman"
  , "SHA256"
  , "SHA1"
  , "CRC32"
  , "CRC64"
  , "GNU"
  , "POSIX"
};

static const EMethodID g_ZstdMethods[] =
{
  kZSTD
};

static const EMethodID g_BrotliMethods[] =
{
  kBROTLI
};

static const EMethodID g_LizardMethods[] =
{
  kLIZARD_M1,
  kLIZARD_M2,
  kLIZARD_M3,
  kLIZARD_M4
};

static const EMethodID g_Lz4Methods[] =
{
  kLZ4
};

static const EMethodID g_Lz5Methods[] =
{
  kLZ5
};
// **************** 7-Zip ZS Modification End ****************

static const EMethodID g_7zMethods[] =
{
  kLZMA2,
  kLZMA,
  kPPMd,
  kBZip2
  , kDeflate
  , kDeflate64
  // , kZSTD
  // **************** 7-Zip ZS Modification Start ****************
  , kZSTD
  , kBROTLI
  , kLZ4
  , kLZ5
  , kLIZARD_M1
  , kLIZARD_M2
  , kLIZARD_M3
  , kLIZARD_M4
  , kFLZMA2
  // **************** 7-Zip ZS Modification End ****************
  , kCopy
};

static const EMethodID g_7zSfxMethods[] =
{
  kCopy,
  kLZMA,
  kLZMA2,
  // **************** 7-Zip ZS Modification Start ****************
  //kPPMd
  kPPMd,
  kFLZMA2,
  kZSTD
  // **************** 7-Zip ZS Modification End ****************
};

static const EMethodID g_ZipMethods[] =
{
  kDeflate,
  kDeflate64,
  kBZip2,
  kLZMA,
  // **************** 7-Zip ZS Modification Start ****************
  //kPPMdZip
  // **************** 7-Zip ZS Modification End ****************
  // , kZSTD
  // **************** 7-Zip ZS Modification Start ****************
  kPPMdZip,
  kZSTD,
  kCopy
  // **************** 7-Zip ZS Modification End ****************
};

static const EMethodID g_GZipMethods[] =
{
  kDeflate
};

static const EMethodID g_BZip2Methods[] =
{
  kBZip2
};

static const EMethodID g_XzMethods[] =
{
  kLZMA2
};

static const EMethodID g_TarMethods[] =
{
  kGnu,
  kPosix
};

static const EMethodID g_HashMethods[] =
{
    kSha256
  , kSha1
  // , kCrc32
  // , kCrc64
};

#define METHODS_PAIR(x) Z7_ARRAY_SIZE(x), x

static const CFormatInfo g_Formats[] =
{
  {
    "",
    // **************** 7-Zip ZS Modification Start ****************
    //// (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    //((UInt32)1 << 10) - 1,
    //// (UInt32)(Int32)-1,
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    // **************** 7-Zip ZS Modification End ****************
    0, NULL,
    kFF_MultiThread | kFF_MemUse
  },
  {
    "7z",
    // **************** 7-Zip ZS Modification Start ****************
    //// (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    //(1 << 10) - 1,
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    // **************** 7-Zip ZS Modification End ****************
    METHODS_PAIR(g_7zMethods),
    kFF_Filter | kFF_Solid | kFF_MultiThread | kFF_Encrypt |
    kFF_EncryptFileNames | kFF_MemUse | kFF_SFX
    // | kFF_Time_Win
  },
  {
    "Zip",
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_ZipMethods),
    kFF_MultiThread | kFF_Encrypt | kFF_MemUse
    // | kFF_Time_Win | kFF_Time_Unix | kFF_Time_DOS
  },
  {
    "GZip",
    (1 << 1) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_GZipMethods),
    kFF_MemUse
    // | kFF_Time_Unix
  },
  {
    "BZip2",
    (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_BZip2Methods),
    kFF_MultiThread | kFF_MemUse
  },
  {
    "xz",
    // **************** 7-Zip ZS Modification Start ****************
    //// (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    //(1 << 10) - 1 - (1 << 0), // store (1 << 0) is not supported
    (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9), // store (1 << 0) is not supported
    // **************** 7-Zip ZS Modification End ****************
    METHODS_PAIR(g_XzMethods),
    kFF_Solid | kFF_MultiThread | kFF_MemUse
  },
  // **************** 7-Zip ZS Modification Start ****************
  {
    "zstd",
    (1 << 1) | (1 << 3) | (1 << 11) | (1 << 19) | (1 << 20),
    METHODS_PAIR(g_ZstdMethods),
    kFF_MultiThread
  },
  {
    "Brotli",
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 6) | (1 << 9) | (1 << 11),
    METHODS_PAIR(g_BrotliMethods),
    kFF_MultiThread
  },
  {
    "Lizard",
    (1 << 10) | (1 << 13) | (1 << 15) | (1 << 17) | (1 << 19),
    METHODS_PAIR(g_LizardMethods),
    kFF_MultiThread
  },
  {
    "LZ4",
    (1 << 1) | (1 << 3) | (1 << 6) | (1 << 9) | (1 << 12),
    METHODS_PAIR(g_Lz4Methods),
    kFF_MultiThread
  },
  {
    "LZ5",
    (1 << 1) | (1 << 3) | (1 << 7) | (1 << 11) | (1 << 15),
    METHODS_PAIR(g_Lz5Methods),
    kFF_MultiThread
  },
  // **************** 7-Zip ZS Modification End ****************
  {
    "Tar",
    (1 << 0),
    METHODS_PAIR(g_TarMethods),
    0
    // kFF_Time_Unix | kFF_Time_Win // | kFF_Time_1ns
  },
  {
    "wim",
    (1 << 0),
    0, NULL,
    0
    // | kFF_Time_Win
  },
  {
    "Hash",
    (0 << 0),
    METHODS_PAIR(g_HashMethods),
    0
  }
};

// **************** 7-Zip ZS Modification Start ****************
#define FL2_MAX_7Z_CLEVEL 9
#define MATCH_BUFFER_SHIFT 8
#define MATCH_BUFFER_ELBOW_BITS 17
#define MATCH_BUFFER_ELBOW (1UL << MATCH_BUFFER_ELBOW_BITS)
#define RMF_BUILDER_SIZE (8 * 0x40100U)

#define MB *(1U<<20)

struct FL2_compressionParameters
{
  UInt32   dictionarySize;   /* largest match distance : larger == more compression, more memory needed during decompression; > 64Mb == more memory per byte, slower */
  unsigned chainLog;         /* HC3 sliding window : larger == more compression, slower; hybrid mode only (ultra) */
  unsigned fastLength;       /* acceptable match size for parser : larger == more compression, slower; fast bytes parameter from 7-Zip */
  bool isUltra;
};

static const FL2_compressionParameters FL2_7zCParameters[FL2_MAX_7Z_CLEVEL + 1] = {
    { 0,       0,   0, false },
    { 1 MB,    7,  32, false },
    { 2 MB,    7,  32, false },
    { 2 MB,    7,  32, false },
    { 4 MB,    7,  32, false },
    { 16 MB,   9,  48, true },
    { 32 MB,  10,  64, true },
    { 64 MB,  11,  96, true },
    { 64 MB,  12, 273, true },
    { 128 MB, 14, 273, true },
};

#undef MB
// **************** 7-Zip ZS Modification End ****************

static bool IsMethodSupportedBySfx(int methodID)
{
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(g_7zSfxMethods); i++)
    if (methodID == g_7zSfxMethods[i])
      return true;
  return false;
}

static const char * const k_Auto_Prefix = "*  ";

static void Modify_Auto(AString &s)
{
  s.Insert(0, k_Auto_Prefix);
}

int GetExtDotPos(const UString &s)
{
  const int dotPos = s.ReverseFind_Dot();
  if (dotPos > s.ReverseFind_PathSepar() + 1)
    return dotPos;
  return -1;
}

static bool IsAsciiString(const UString &s)
{
  for (unsigned i = 0; i < s.Len(); i++)
  {
    const wchar_t c = s[i];
    if (c < 0x20 || c > 0x7F)
      return false;
  }
  return true;
}

static void AddSize_MB(UString &s, UInt64 size)
{
  s.Add_LF();
  const UInt64 v2 = size + ((UInt32)1 << 20) - 1;
  if (size < v2)
      size = v2;
  s.Add_UInt64(size >> 20);
  s += " MB : ";
}

static void AddSize_MB_id(UString &s, UInt64 size, UInt32 id)
{
  AddSize_MB(s, size);
  AddLangString(s, id);
}

void SetErrorMessage_MemUsage(UString &s, UInt64 reqSize, UInt64 ramSize, UInt64 ramLimit, const UString &usageString)
{
  AddLangString(s, IDS_MEM_OPERATION_BLOCKED);
  s.Add_LF();
  AddLangString(s, IDS_MEM_REQUIRES_BIG_MEM);
  s.Add_LF();
  AddSize_MB(s, reqSize);
  s += usageString;
  AddSize_MB_id(s, ramSize, IDS_MEM_RAM_SIZE);
  // if (ramLimit != 0)
  {
    AddSize_MB_id(s, ramLimit, IDS_MEM_USAGE_LIMIT_SET_BY_7ZIP);
  }
  s.Add_LF();
  s.Add_LF();
  AddLangString(s, IDS_MEM_ERROR);
}

static const size_t k_Auto_Dict = (size_t)0 - 1;

static void AddComboItem(CObjectVector<CCompressDialogCore::COptionItem> &items,
    const AString &display, UInt64 value)
{
  items.Add(CCompressDialogCore::COptionItem(GetUnicodeString(display), value));
}

static void AddComboItem(CObjectVector<CCompressDialogCore::COptionItem> &items,
    const UString &display, UInt64 value)
{
  items.Add(CCompressDialogCore::COptionItem(display, value));
}

static UInt64 Get_Lzma2_ChunkSize(UInt64 dict)
{
  // we use same default chunk sizes as defined in 7z encoder and lzma2 encoder
  UInt64 cs = (UInt64)dict << 2;
  const UInt32 kMinSize = (UInt32)1 << 20;
  const UInt32 kMaxSize = (UInt32)1 << 28;
  if (cs < kMinSize) cs = kMinSize;
  if (cs > kMaxSize) cs = kMaxSize;
  if (cs < dict) cs = dict;
  cs += (kMinSize - 1);
  cs &= ~(UInt64)(kMinSize - 1);
  return cs;
}

static void Add_Size(AString &s, UInt64 val)
{
  unsigned moveBits = 0;
  char c = 0;
       if ((val & 0x3FFFFFFF) == 0) { moveBits = 30; c = 'G'; }
  else if ((val &    0xFFFFF) == 0) { moveBits = 20; c = 'M'; }
  else if ((val &      0x3FF) == 0) { moveBits = 10; c = 'K'; }
  s.Add_UInt64(val >> moveBits);
  s.Add_Space();
  if (moveBits != 0)
    s.Add_Char(c);
  s.Add_Char('B');
}

static void AddMemSize(UString &res, UInt64 size)
{
  char c;
  unsigned moveBits = 0;
  if (size >= ((UInt64)1 << 31) && (size & 0x3FFFFFFF) == 0)
    { moveBits = 30; c = 'G'; }
  else // if (size >= ((UInt32)1 << 21) && (size & 0xFFFFF) == 0)
    { moveBits = 20; c = 'M'; }
  // else { moveBits = 10; c = 'K'; }
  res.Add_UInt64(size >> moveBits);
  res.Add_Space();
  if (moveBits != 0)
    res.Add_Char(c);
  res.Add_Char('B');
}

static void AddMemUsage(UString &s, UInt64 v)
{
  const char *post;
  if (v <= ((UInt64)16 << 30))
  {
    v = (v + (1 << 20) - 1) >> 20;
    post = "MB";
  }
  else if (v <= ((UInt64)64 << 40))
  {
    v = (v + (1 << 30) - 1) >> 30;
    post = "GB";
  }
  else
  {
    const UInt64 v2 = v + ((UInt64)1 << 40) - 1;
    if (v <= v2)
      v = v2;
    v >>= 40;
    post = "TB";
  }
  s.Add_UInt64(v);
  s.Add_Space();
  s += post;
}

static void Combine_Two_BoolPairs(const CBoolPair &b1, const CBoolPair &b2, CBool1 &res)
{
  if (!b1.Def && b2.Def)
    res.Val = b2.Val;
  else
    res.Val = b1.Val;
}

#define SET_GUI_BOOL(name) \
      Combine_Two_BoolPairs(Info. name, RegistryInfo. name, name)

static void Set_Final_BoolPairs(
    const CBool1 &gui,
    CBoolPair &cmd,
    CBoolPair &reg)
{
  if (!cmd.Def)
  {
    reg.Val = gui.Val;
    reg.Def = gui.Val;
  }
  if (gui.Supported)
  {
    cmd.Val = gui.Val;
    cmd.Def = gui.Val;
  }
  else
    cmd.Init();
}

#define SET_FINAL_BOOL_PAIRS(name) \
    Set_Final_BoolPairs(name, Info. name, RegistryInfo. name)


CCompressDialogCore::CCompressDialogCore()
{
  Clear();
}

void CCompressDialogCore::Clear()
{
  ArcFormats = NULL;
  ArcIndices.Clear();
  ExternalMethods.Clear();
  Info = NCompressDialog::CInfo();
  RegistryInfo = NCompression::CInfo();
  OriginalFileName.Empty();
  KeepName = false;

  SymLinks.Init();
  HardLinks.Init();
  AltStreams.Init();
  NtSecurity.Init();
  PreserveATime.Init();

  FormatIndex = -1;
  PrevFormat = -1;
  Level = (UInt32)(Int32)-1;
  MethodID = -1;
  Dict64 = (UInt64)(Int64)-1;
  Order = (UInt32)(Int32)-1;
  BlockLogSize = (UInt32)(Int32)-1;
  NumThreads = (UInt32)(Int32)-1;
  MemUseIndex = -1;
  EncryptionMethodIndex = -1;
  DefaultEncryptionMethodIndex = -1;
  ShowPassword = false;
  SfxChecked = false;
  Password.Empty();
  PasswordConfirmation.Empty();
  DirPrefix.Empty();
  StartDirPrefix.Empty();
  ArchiveName.Empty();

  AutoMethodId = -1;
  AutoDict = (UInt32)(Int32)-1;
  AutoOrder = (UInt32)(Int32)-1;
  AutoSolid = (UInt64)(Int64)-1;
  AutoNumThreads = (UInt32)(Int32)-1;

  RamSizeDefined = false;
  RamSize = 0;
  RamSizeReduced = 0;
  RamUsageAuto = 0;

  FormatItems.Clear();
  LevelItems.Clear();
  MethodItems.Clear();
  DictionaryItems.Clear();
  OrderItems.Clear();
  SolidItems.Clear();
  ThreadItems.Clear();
  MemUseItems.Clear();
  EncryptionMethodItems.Clear();
  MemUseStrings.Clear();

  SolidSupported = false;
  MultiThreadSupported = false;
  EncryptSupported = false;
  EncryptFileNamesSupported = false;
  MemUseSupported = false;
  SfxSupported = false;

  HardwareThreadsText.Empty();
  MemoryValueText.Empty();
  DecompressMemoryText.Empty();
  OptionsSummaryText.Empty();
  VolumeConfirmText.Empty();
  VolumeConfirmed = false;
}


void CCompressDialogCore::SetMethods(const CObjectVector<CCodecInfoUser> &userCodecs)
{
  ExternalMethods.Clear();
  {
    FOR_VECTOR (i, userCodecs)
    {
      const CCodecInfoUser &c = userCodecs[i];
      if (!c.EncoderIsAssigned
          || !c.IsFilter_Assigned
          || c.IsFilter
          || c.NumStreams != 1)
        continue;
      unsigned k;
      for (k = 0; k < Z7_ARRAY_SIZE(g_7zMethods); k++)
        if (c.Name.IsEqualTo_Ascii_NoCase(kMethodsNames[g_7zMethods[k]]))
          break;
      if (k != Z7_ARRAY_SIZE(g_7zMethods))
        continue;
      ExternalMethods.Add(c.Name);
    }
  }
}


void CCompressDialogCore::Initialize()
{
  {
    size_t size = (size_t)sizeof(size_t) << 29;
    RamSizeDefined = NSystem::GetRamSize(size);
    // size = (UInt64)3 << 62; // for debug only;
    {
      // we use reduced limit for 32-bit version:
      unsigned bits = sizeof(size_t) * 8;
      if (bits == 32)
      {
        const UInt32 limit2 = (UInt32)7 << 28;
        if (size > limit2)
            size = limit2;
      }
    }
    RamSize = size;
    const size_t kMinUseSize = 1 << 26;
    if (size < kMinUseSize)
        size = kMinUseSize;
    RamSizeReduced = size;

    // 80% - is auto usage limit in handlers
    RamUsageAuto = Calc_From_Val_Percents(size, 80);
  }

  RegistryInfo.Load();
}


void CCompressDialogCore::CalcFormats()
{
  FormatItems.Clear();
  const bool needSetMain = (Info.FormatIndex < 0);
  FOR_VECTOR(i, ArcIndices)
  {
    const unsigned arcIndex = ArcIndices[i];
    const CArcInfoEx &ai = (*ArcFormats)[arcIndex];
    const int index = (int)FormatItems.Add(COptionItem(ai.Name, arcIndex));
    if (!needSetMain)
    {
      if (Info.FormatIndex == (int)arcIndex)
        FormatIndex = (int)arcIndex;
      continue;
    }
    if (i == 0 || ai.Name.IsEqualTo_NoCase(RegistryInfo.ArcType))
    {
      FormatIndex = (int)arcIndex;
      Info.FormatIndex = (int)arcIndex;
    }
  }
}


int CCompressDialogCore::FindExactIndex(const CObjectVector<COptionItem> &items, UInt64 value) const
{
  for (int i = (int)items.Size() - 1; i >= 0; i--)
    if (items[i].Value == value)
      return i;
  return -1;
}

int CCompressDialogCore::FindNearestIndex(const CObjectVector<COptionItem> &items, UInt64 value) const
{
  // exact:
  for (int i = (int)items.Size() - 1; i >= 0; i--)
    if (items[i].Value == value)
      return i;
  // nearest:
  for (int i = (int)items.Size() - 1; i >= 0; i--)
    if (items[i].Value <= value)
      return i;
  // fallback:
  if (items.Size() > 0)
    return 0;
  return -1;
}


void CCompressDialogCore::CalcLevels()
{
  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  const CArcInfoEx &ai = Get_ArcInfoEx();
  UInt32 LevelsMask = fi.LevelsMask;
  Int32 LevelsStart = (LevelsMask & 1) ? 0 : 1;
  Int32 LevelsEnd = 9;
  bool LevelsEndByMask = true;
  int id = -1;
  if (ai.LevelsMask != 0xFFFFFFFF)
    LevelsMask = ai.LevelsMask;
  else
  {
    id = GetMethodID();
    if (id == kCopy) {
      LevelsStart = 0;
      LevelsEnd = 0;
      LevelsEndByMask = false;
      LevelsMask = 0;
    } else if (id >= kZSTD && id <= kLIZARD_M4) {
      auto& r = g_LevelRanges[id - kZSTD];
      LevelsStart = r[0];
      LevelsEnd = r[1];
      LevelsEndByMask = false;
      if (id == kZSTD) {
        LevelsMask = g_Formats[6].LevelsMask;
      } else if (id == kBROTLI)
        LevelsMask = g_Formats[7].LevelsMask;
      else if (id >= kLIZARD_M1 && id <= kLIZARD_M4)
        LevelsMask = g_Formats[8].LevelsMask;
      else if (id == kLZ4)
        LevelsMask = g_Formats[9].LevelsMask;
      else if (id == kLZ5)
        LevelsMask = g_Formats[10].LevelsMask;
    }
  }
  UInt32 level = LevelItems.Size() > 0 ? (UInt32)LevelItems[FindNearestIndex(LevelItems, Level)].Value : (LevelsEnd - LevelsStart + 1) / 2;
  LevelItems.Clear();
  {
    int index = FindRegistryFormat(ai.Name);
    if (index >= 0)
    {
      const NCompression::CFormatOptions &fo = RegistryInfo.Formats[index];
      if ( (fo.Level <= (UInt32)LevelsEnd) || (id != kCopy && fo.Level == Z7_ZSTD_ULTIMATE_LEV)
        || (id == kZSTD && fo.Level > Z7_ZSTD_FAST_LEV_INC && fo.Level <= Z7_ZSTD_FAST_LEV_INC + 64)
      ) {
        level = (Int32)fo.Level;
      } else {
        level = (Int32)(LevelsEnd - (LevelsStart > 0 ? LevelsStart : 0) + 1) / 2;
      }
    }
  }

  const WCHAR t[] = L"Level ";
  const WCHAR tf[] = L"Fast ";
  for (Int32 i = LevelsStart, ir, j = 0; i <= LevelsEnd; i++)
  {
    if (!i && id == kZSTD) continue;

    // lizard needs extra handling
    if (GetMethodID() >= kLIZARD_M1 && GetMethodID() <= kLIZARD_M4) {
      ir = i;
      if (ir % 10 == 0) j = 0;
      while (ir > 19) { ir -= 10; }
    } else {
      ir = i;
    }

    // max reached
    if (LevelsEndByMask && LevelsMask < (UInt32)(1 << ir))
      break;

    char buf[20+1];
    UString s = i >= 0 ? t : tf;
    ConvertInt64ToString(i, buf);
    s += buf;
    if (ir < 0 && id == kZSTD) {
      int lid = 0;
      switch (-ir) {
        case 64: lid = IDS_METHOD_ULTIMATEFAST; break;
        case  7: lid = IDS_METHOD_ULTRAFAST;    break;
        case  1: lid = IDS_METHOD_SUPERFAST;    break;
      }
      if (lid) {
        s += L" (";
        s += LangString(lid);
        s += L")";
      }
    }
    else
    if (ir >= 0 && (LevelsMask & (1 << ir)) && j < Z7_ARRAY_SIZE(g_Levels))
    {
      // skip level 0 (store) if not supported
      if (j == 0 && ir != 0) j = 1;
      s += L" (";
      s += LangString(g_Levels[j++]);
      s += L")";
    }
    LevelItems.Add(COptionItem(s, i >= 0 ? (UInt64)i : (UInt64)(Z7_ZSTD_FAST_LEV_INC - i)));
  }
  if (LevelItems.Size() > 1) { // ultimate level (max possible or zstd --max if allowed)
    UString s;
    if (id == kZSTD) {
      s = LangString(IDS_METHOD_ADV_MAX);
    } else {
      s = LangString(IDS_METHOD_HIGHEST);
    }
    LevelItems.Add(COptionItem(s, Z7_ZSTD_ULTIMATE_LEV));
  }
  const int sel = FindNearestIndex(LevelItems, level);
  if (sel >= 0)
    Level = (UInt32)LevelItems[sel].Value;
  else
    Level = (UInt32)(Int32)-1;
}


void CCompressDialogCore::CalcMethods(int keepMethodId)
{
  MethodItems.Clear();
  AutoMethodId = -1;
  MethodID = -1;
  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  const CArcInfoEx &ai = Get_ArcInfoEx();
  UString defaultMethod;
  // **************** 7-Zip ZS Modification Start ****************
  int defaultLevel = 5;
  // **************** 7-Zip ZS Modification End ****************
  {
    const int index = FindRegistryFormat(ai.Name);
    if (index >= 0)
    {
      const NCompression::CFormatOptions &fo = RegistryInfo.Formats[index];
      defaultMethod = fo.Method;
      // **************** 7-Zip ZS Modification Start ****************
      defaultLevel = fo.Level;
      // **************** 7-Zip ZS Modification End ****************
    }
  }
  const bool isSfx = IsSfx();
  bool weUseSameMethod = false;

  const bool is7z = ai.Is_7z();
  
  for (unsigned m = 0;; m++)
  {
    int methodID;
    // **************** 7-Zip ZS Modification Start ****************
    //const char *method;
    const char *method, *methodLong;
    // **************** 7-Zip ZS Modification End ****************
    if (m < fi.NumMethods)
    {
      methodID = fi.MethodIDs[m];
      method = kMethodsNames[methodID];
      // **************** 7-Zip ZS Modification Start ****************
      methodLong = kMethodsNamesLong[methodID];
      // **************** 7-Zip ZS Modification End ****************
      if (is7z)
      if (methodID == kCopy
          || methodID == kDeflate
          || methodID == kDeflate64
          )
        continue;
    }
    else
    {
      if (!is7z)
        break;
      const unsigned extIndex = m - fi.NumMethods;
      if (extIndex >= ExternalMethods.Size())
        break;
      methodID = (int)(Z7_ARRAY_SIZE(kMethodsNames) + extIndex);
      method = ExternalMethods[extIndex].Ptr();
      // **************** 7-Zip ZS Modification Start ****************
      methodLong = method;
      // **************** 7-Zip ZS Modification End ****************
    }
    if (isSfx)
      if (!IsMethodSupportedBySfx(methodID))
        continue;

    // **************** 7-Zip ZS Modification Start ****************
    //AString s(method);
    AString s(methodLong);
    // **************** 7-Zip ZS Modification End ****************
    int writtenMethodId = methodID;
    if (m == 0)
    {
      AutoMethodId = methodID;
      writtenMethodId = -1;
      Modify_Auto(s);
    }
    const int itemIndex = (int)MethodItems.Add(COptionItem(GetUnicodeString(s), (UInt64)(Int64)writtenMethodId));
    if (keepMethodId == methodID)
    {
      MethodID = writtenMethodId;
      weUseSameMethod = true;
      continue;
    }
    if ((defaultMethod.IsEqualTo_Ascii_NoCase(method) || m == 0) && !weUseSameMethod)
      MethodID = writtenMethodId;
  }

  // **************** 7-Zip ZS Modification Start ****************
  //if (!weUseSameMethod)
  //  MethodChanged();
  if (!weUseSameMethod) {
    // Lizard :/
    if (defaultMethod.IsEqualTo_Ascii_NoCase("lizard") && keepMethodId == -1) {
      if (defaultLevel >= 10 && defaultLevel <= 19) MethodID = kLIZARD_M1;
      else
      if (defaultLevel >= 20 && defaultLevel <= 29) MethodID = kLIZARD_M2;
      else
      if (defaultLevel >= 30 && defaultLevel <= 39) MethodID = kLIZARD_M3;
      else
      if (defaultLevel >= 40 && defaultLevel <= 49) MethodID = kLIZARD_M4;
    }
    ComprMethodChanged();
    CalcDictionary();
    CalcOrder();
  }
  // **************** 7-Zip ZS Modification End ****************
}


void CCompressDialogCore::ComprMethodChanged()
{
  const CArcInfoEx &ai = Get_ArcInfoEx();
  const int index = FindRegistryFormat(ai.Name);
  if (index >= 0)
  {
    UString compMeth;
    GetMethodSpec(compMeth);
    NCompression::CFormatOptions &fo = RegistryInfo.Formats[index];
    if (!compMeth.IsEqualTo_NoCase(fo.Method)) {
      fo.Method = compMeth;
      RegistryInfo.LoadAndUpdateFormatByMethod(fo);
    }
  }
}


bool CCompressDialogCore::IsZipFormat() const
{
  return Get_ArcInfoEx().Is_Zip();
}

bool CCompressDialogCore::IsXzFormat() const
{
  return Get_ArcInfoEx().Is_Xz();
}


void CCompressDialogCore::CalcEncryptionMethod()
{
  EncryptionMethodItems.Clear();
  DefaultEncryptionMethodIndex = -1;
  EncryptionMethodIndex = -1;
  const CArcInfoEx &ai = Get_ArcInfoEx();
  if (ai.Is_7z())
  {
    AddComboItem(EncryptionMethodItems, L"AES-256", 0);
    EncryptionMethodIndex = 0;
    DefaultEncryptionMethodIndex = 0;
  }
  else if (ai.Is_Zip())
  {
    const int index = FindRegistryFormat(ai.Name);
    UString encryptionMethod;
    if (index >= 0)
    {
      const NCompression::CFormatOptions &fo = RegistryInfo.Formats[index];
      encryptionMethod = fo.EncryptionMethod;
    }
    int sel = 0;
    // if (ZipCryptoIsAllowed)
    {
      AddComboItem(EncryptionMethodItems, L"ZipCrypto", 0);
      sel = (encryptionMethod.IsPrefixedBy_Ascii_NoCase("aes") ? 1 : 0);
      DefaultEncryptionMethodIndex = 0;
    }
    AddComboItem(EncryptionMethodItems, L"AES-256", 1);
    EncryptionMethodIndex = sel;
  }
}


UInt64 CCompressDialogCore::GetDict2() const
{
  UInt64 num = GetDictSpec();
  if (num == (UInt64)(Int64)-1)
  {
    if (AutoDict == (UInt32)(Int32)-1)
      return (UInt64)(Int64)-1; // unknown
    num = AutoDict;
  }
  return num;
}


UInt32 CCompressDialogCore::GetNumThreads2() const
{
  UInt32 num = GetNumThreadsSpec();
  if (num == (UInt32)(Int32)-1)
    num = AutoNumThreads;
  return num;
}


UString CCompressDialogCore::GetMethodSpec(UString &estimatedName) const
{
  estimatedName.Empty();
  if (MethodItems.Size() < 1)
    return estimatedName;
  int methodId = MethodID;
  if (methodId < 0)
    methodId = AutoMethodId;
  UString s;
  if (methodId >= 0)
  {
    if ((unsigned)methodId < Z7_ARRAY_SIZE(kMethodsNames))
      estimatedName = kMethodsNames[methodId];
    else
      estimatedName = ExternalMethods[(unsigned)methodId - (unsigned)Z7_ARRAY_SIZE(kMethodsNames)];
    if (MethodID >= 0)
      s = estimatedName;
  }
  return s;
}


UString CCompressDialogCore::GetMethodSpec() const
{
  UString estimatedName;
  UString s = GetMethodSpec(estimatedName);
  return s;
}

bool CCompressDialogCore::IsMethodEqualTo(const UString &s) const
{
  UString estimatedName;
  const UString shortName = GetMethodSpec(estimatedName);
  if (s.IsEmpty())
    return shortName.IsEmpty();
  return s.IsEqualTo_NoCase(estimatedName);
}


UString CCompressDialogCore::GetEncryptionMethodSpec() const
{
  UString s;
  if (EncryptionMethodItems.Size() > 0
      && EncryptionMethodIndex != DefaultEncryptionMethodIndex)
  {
    s = EncryptionMethodItems[EncryptionMethodIndex].Display;
    s.RemoveChar(L'-');
  }
  return s;
}


UString CCompressDialogCore::Get_MemUse_Spec() const
{
  if (MemUseItems.Size() < 1)
    return UString();
  return MemUseStrings[(unsigned)MemUseIndex];
}


UInt64 CCompressDialogCore::Get_MemUse_Bytes() const
{
  const UString mus = Get_MemUse_Spec();
  NCompression::CMemUse mu;
  if (!mus.IsEmpty())
  {
    mu.Parse(mus);
    if (mu.IsDefined)
      return mu.GetBytes(RamSizeReduced);
  }
  return RamUsageAuto; // RamSizeReduced; // RamSize;;
}


bool CCompressDialogCore::GetOrderMode() const
{
  switch (GetMethodID())
  {
    case kPPMd:
    case kPPMdZip:
      return true;
  }
  return false;
}


static void AddDictItem(CObjectVector<CCompressDialogCore::COptionItem> &items,
    size_t sizeReal, size_t sizeShow)
{
  char c = 0;
  unsigned moveBits = 0;
       if ((sizeShow & 0xFFFFF) == 0) { moveBits = 20; c = 'M'; }
  else if ((sizeShow &   0x3FF) == 0) { moveBits = 10; c = 'K'; }
  AString s;
  s.Add_UInt64(sizeShow >> moveBits);
  s.Add_Space();
  if (c != 0)
    s.Add_Char(c);
  s.Add_Char('B');
  if (sizeReal == k_Auto_Dict)
    Modify_Auto(s);
  AddComboItem(items, s, (UInt64)sizeReal);
}


void CCompressDialogCore::CalcDictionary()
{
  DictionaryItems.Clear();
  Dict64 = (UInt64)(Int64)-1;
  // m_Dictionary_Chain.ResetContent();
  
  // _auto_Dict = (UInt32)1 << 24; // we can use this dictSize to calculate _auto_Solid for unknown method for 7z
  AutoDict = (UInt32)(Int32)-1; // for debug
  // _auto_Dict_Chain = (UInt32)(Int32)-1; // for debug

  const CArcInfoEx &ai = Get_ArcInfoEx();
  UInt32 defaultDict = (UInt32)(Int32)-1;
  // UInt32 defaultDict_Chain = (UInt32)(Int32)-1;
  {
    const int index = FindRegistryFormat(ai.Name);
    if (index >= 0)
    {
      const NCompression::CFormatOptions &fo = RegistryInfo.Formats[index];
      if (IsMethodEqualTo(fo.Method))
      {
        defaultDict = fo.Dictionary;
        // defaultDict_Chain = fo.DictionaryChain;
      }
    }
  }
  
  const int methodID = GetMethodID();
  // **************** 7-Zip ZS Modification Start ****************
  //const UInt32 level = GetLevel2();
  UInt32 level = GetLevel2();
  // **************** 7-Zip ZS Modification End ****************

  if (methodID < 0)
    return;
  
  switch (methodID)
  {
    case kLZMA:
    case kLZMA2:
    {
      {
        AutoDict = level <= 4 ?
            (UInt32)1 << (level * 2 + 16) :
            level <= sizeof(size_t) / 2 + 4 ?
              (UInt32)1 << (level + 20) :
              (UInt32)1 << (sizeof(size_t) / 2 + 24);
      }

      // we use threshold 3.75 GiB to switch to kLzmaMaxDictSize.
      if (defaultDict != (UInt32)(Int32)-1
          && defaultDict >= ((UInt32)15 << 28))
        defaultDict = kLzmaMaxDictSize;
      
      const size_t kLzmaMaxDictSize_Up = (size_t)1 << (20 + sizeof(size_t) / 4 * 6);
      
      int curSel = (int)DictionaryItems.Size();
      AddDictItem(DictionaryItems, k_Auto_Dict, AutoDict);

      for (unsigned i = (16 - 1) * 2; i <= (32 - 1) * 2; i++)
      {
        if (i < (20 - 1) * 2
            && i != (16 - 1) * 2
            && i != (18 - 1) * 2)
          continue;
        if (i == (20 - 1) * 2 + 1)
          continue;
        const size_t dict_up = (size_t)(2 + (i & 1)) << (i / 2);
        size_t dict = dict_up;
        if (dict_up >= kLzmaMaxDictSize)
          dict = kLzmaMaxDictSize; // we reduce dictionary
        
        const int index = (int)DictionaryItems.Size();
        AddDictItem(DictionaryItems, dict, dict_up);

        // const UInt32 numThreads = 2;
        // const UInt64 memUsage = GetMemoryUsageComp_Threads_Dict(numThreads, dict);
        if (defaultDict != (UInt32)(Int32)-1)
          if (dict <= defaultDict || curSel <= 0)
          // if (!maxRamSize_Defined || memUsage <= maxRamSize)
            curSel = index;
        if (dict_up >= kLzmaMaxDictSize_Up)
          break;
      }
      
      Dict64 = DictionaryItems[curSel].Value;
      break;
    }

    case kPPMd:
    {
      AutoDict = (UInt32)1 << (level + 19);

      const UInt32 kPpmd_Default_4g = (UInt32)0 - ((UInt32)1 << 10);
      const size_t kPpmd_MaxDictSize_Up = (size_t)1 << (29 + sizeof(size_t) / 8);

      if (defaultDict != (UInt32)(Int32)-1
          && defaultDict >= ((UInt32)15 << 28)) // threshold
        defaultDict = kPpmd_Default_4g;

      int curSel = (int)DictionaryItems.Size();
      AddDictItem(DictionaryItems, k_Auto_Dict, AutoDict);

      for (unsigned i = (20 - 1) * 2; i <= (32 - 1) * 2; i++)
      {
        if (i == (20 - 1) * 2 + 1)
          continue;

        const size_t dict_up = (size_t)(2 + (i & 1)) << (i / 2);
        size_t dict = dict_up;
        if (dict_up >= kPpmd_Default_4g)
          dict = kPpmd_Default_4g;

        const int index = (int)DictionaryItems.Size();
        AddDictItem(DictionaryItems, dict, dict_up);
        // AddDict2((UInt32)((UInt32)0 - 2), dict_up); // for debug
        // AddDict(dict_up); // for debug
        // const UInt64 memUsage = GetMemoryUsageComp_Threads_Dict(1, dict);
        if (defaultDict != (UInt32)(Int32)-1)
          if (dict <= defaultDict || curSel <= 0)
            // if (!maxRamSize_Defined || memUsage <= maxRamSize)
            curSel = index;
        if (dict_up >= kPpmd_MaxDictSize_Up)
          break;
      }
      Dict64 = DictionaryItems[curSel].Value;
      break;
    }

    case kPPMdZip:
    {
      AutoDict = (UInt32)1 << (level + 19);
      
      int curSel = (int)DictionaryItems.Size();
      AddDictItem(DictionaryItems, k_Auto_Dict, AutoDict);

      for (unsigned i = 20; i <= 28; i++)
      {
        const UInt32 dict = (UInt32)1 << i;
        const int index = (int)DictionaryItems.Size();
        AddDictItem(DictionaryItems, dict, dict);
        // const UInt64 memUsage = GetMemoryUsageComp_Threads_Dict(1, dict);
        if (defaultDict != (UInt32)(Int32)-1)
          if (dict <= defaultDict || curSel <= 0)
            // if (!maxRamSize_Defined || memUsage <= maxRamSize)
            curSel = index;
      }
      Dict64 = DictionaryItems[curSel].Value;
      break;
    }

    case kDeflate:
    case kDeflate64:
    {
      const UInt32 dict = (methodID == kDeflate ? (UInt32)(1 << 15) : (UInt32)(1 << 16));
      AutoDict = dict;
      AddDictItem(DictionaryItems, k_Auto_Dict, AutoDict);
      Dict64 = DictionaryItems[0].Value;
      // EnableItem(IDC_COMPRESS_DICTIONARY, false);
      break;
    }
    
    case kBZip2:
    {
      {
             if (level >= 5) AutoDict = (900 << 10);
        else if (level >= 3) AutoDict = (500 << 10);
        else                 AutoDict = (100 << 10);
      }

      int curSel = (int)DictionaryItems.Size();
      AddDictItem(DictionaryItems, k_Auto_Dict, AutoDict);
      
      for (unsigned i = 1; i <= 9; i++)
      {
        const UInt32 dict = ((UInt32)i * 100) << 10;
        const int index = (int)DictionaryItems.Size();
        AddDictItem(DictionaryItems, dict, dict);
        // AddDict2(i * 100000, dict);
        if (defaultDict != (UInt32)(Int32)-1)
          if (i <= defaultDict / 100000 || curSel <= 0)
            curSel = index;
      }
      Dict64 = DictionaryItems[curSel].Value;
      break;
    }

    case kCopy:
    {
      AutoDict = 0;
      AddDictItem(DictionaryItems, 0, 0);
      Dict64 = DictionaryItems[0].Value;
      break;
    }
    // **************** 7-Zip ZS Modification Start ****************
    case kFLZMA2:
    {
      static const UInt32 kMinDicSize = (1 << 20);
      level += !level;
      if (level > FL2_MAX_7Z_CLEVEL)
        level = FL2_MAX_7Z_CLEVEL;
      if (defaultDict == (UInt32)(Int32)-1)
        defaultDict = FL2_7zCParameters[level].dictionarySize;

      int curSel = 0;

      for (unsigned i = 20; i <= 31; i++) {
        UInt32 dict = (UInt32)1 << i;

        if (dict >
          #ifdef MY_CPU_64BIT
            (1 << 30)
          #else
            (1 << 27)
          #endif
          )
          continue;

        const int index = (int)DictionaryItems.Size();
        AddDictItem(DictionaryItems, dict, dict);
        //const UInt64 memUsage = GetMemoryUsageComp_Threads_Dict(dict);
        if (dict <= defaultDict /*&& (!maxRamSize_Defined || memUsage <= maxRamSize)*/)
          curSel = index;
      }

      Dict64 = DictionaryItems.Size() > 0 ? DictionaryItems[curSel].Value : (UInt64)(Int64)-1;
      break;
    }
    // **************** 7-Zip ZS Modification End ****************
  }
}


static void AddOrderItem(CObjectVector<CCompressDialogCore::COptionItem> &items, UInt32 size)
{
  char s[32];
  ConvertUInt32ToString(size, s);
  AddComboItem(items, GetUnicodeString(s), size);
}

static void AddOrderItem_Auto(CObjectVector<CCompressDialogCore::COptionItem> &items, UInt32 autoOrder)
{
  AString s;
  s.Add_UInt32(autoOrder);
  Modify_Auto(s);
  AddComboItem(items, s, (UInt64)(Int64)(Int32)-1);
}


void CCompressDialogCore::CalcOrder()
{
  OrderItems.Clear();
  Order = (UInt32)(Int32)-1;
  
  AutoOrder = 1;

  const CArcInfoEx &ai = Get_ArcInfoEx();
  UInt32 defaultOrder = (UInt32)(Int32)-1;

  {
    const int index = FindRegistryFormat(ai.Name);
    if (index >= 0)
    {
      const NCompression::CFormatOptions &fo = RegistryInfo.Formats[index];
      if (IsMethodEqualTo(fo.Method))
        defaultOrder = fo.Order;
    }
  }

  const int methodID = GetMethodID();
  const UInt32 level = GetLevel2();
  if (methodID < 0)
    return;
  
  switch (methodID)
  {
    case kLZMA:
    case kLZMA2:
    // **************** 7-Zip ZS Modification Start ****************
    case kFLZMA2:
    // **************** 7-Zip ZS Modification End ****************
    {
      // **************** 7-Zip ZS Modification Start ****************
      //_auto_Order = (level < 7 ? 32 : 64);
      if (methodID == kFLZMA2)
        AutoOrder = FL2_7zCParameters[level].fastLength;
      else
        AutoOrder = (level < 7 ? 32 : 64);
      // **************** 7-Zip ZS Modification End ****************
      int curSel = (int)OrderItems.Size();
      AddOrderItem_Auto(OrderItems, AutoOrder);
      for (unsigned i = 2 * 2; i < 8 * 2; i++)
      {
        UInt32 order = ((UInt32)(2 + (i & 1)) << (i / 2));
        if (order > 256)
          order = 273;
        const int index = (int)OrderItems.Size();
        AddOrderItem(OrderItems, order);
        if (defaultOrder != (UInt32)(Int32)-1)
          if (order <= defaultOrder || curSel <= 0)
            curSel = index;
      }
      Order = (UInt32)OrderItems[curSel].Value;
      break;
    }

    case kDeflate:
    case kDeflate64:
    {
      {
             if (level >= 9) AutoOrder = 128;
        else if (level >= 7) AutoOrder = 64;
        else                 AutoOrder = 32;
      }
      int curSel = (int)OrderItems.Size();
      AddOrderItem_Auto(OrderItems, AutoOrder);
      for (unsigned i = 2 * 2; i < 8 * 2; i++)
      {
        UInt32 order = ((UInt32)(2 + (i & 1)) << (i / 2));
        if (order > 256)
          order = (methodID == kDeflate64 ? 257 : 258);
        const int index = (int)OrderItems.Size();
        AddOrderItem(OrderItems, order);
        if (defaultOrder != (UInt32)(Int32)-1)
          if (order <= defaultOrder || curSel <= 0)
            curSel = index;
      }
      
      Order = (UInt32)OrderItems[curSel].Value;
      break;
    }
   
    case kPPMd:
    {
      {
             if (level >= 9) AutoOrder = 32;
        else if (level >= 7) AutoOrder = 16;
        else if (level >= 5) AutoOrder = 6;
        else                 AutoOrder = 4;
      }
      
      int curSel = (int)OrderItems.Size();
      AddOrderItem_Auto(OrderItems, AutoOrder);

      for (unsigned i = 0;; i++)
      {
        UInt32 order = i + 2;
        if (i >= 2)
          order = (4 + ((i - 2) & 3)) << ((i - 2) / 4);
        const int index = (int)OrderItems.Size();
        AddOrderItem(OrderItems, order);
        if (defaultOrder != (UInt32)(Int32)-1)
          if (order <= defaultOrder || curSel <= 0)
            curSel = index;
        if (order >= 32)
          break;
      }
      Order = (UInt32)OrderItems[curSel].Value;
      break;
    }

    case kPPMdZip:
    {
      AutoOrder = level + 3;
      int curSel = (int)OrderItems.Size();
      AddOrderItem_Auto(OrderItems, AutoOrder);
      for (unsigned i = 2; i <= 16; i++)
      {
        const int index = (int)OrderItems.Size();
        AddOrderItem(OrderItems, i);
        if (defaultOrder != (UInt32)(Int32)-1)
          if (i <= defaultOrder || curSel <= 0)
            curSel = index;
      }
      Order = (UInt32)OrderItems[curSel].Value;
      break;
    }
    
    // case kBZip2:
    default:
      break;
  }
}


void CCompressDialogCore::CalcSolidBlockSize()
{
  SolidItems.Clear();
  BlockLogSize = (UInt32)(Int32)-1;
  AutoSolid = 1 << 20;

  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  if (!fi.Solid_())
    return;

  const UInt32 level = GetLevel2();
  if (level == 0)
    return;

  UInt64 dict = GetDict2();
  if (dict == (UInt64)(Int64)-1)
  {
    dict = 1 << 25; // default dict for unknown methods
    // return;
  }


  UInt32 defaultBlockSize = (UInt32)(Int32)-1;

  const CArcInfoEx &ai = Get_ArcInfoEx();

  {
    const int index = FindRegistryFormat(ai.Name);
    if (index >= 0)
    {
      const NCompression::CFormatOptions &fo = RegistryInfo.Formats[index];
      if (IsMethodEqualTo(fo.Method))
        defaultBlockSize = fo.BlockLogSize;
    }
  }

  const bool is7z = ai.Is_7z();

  const UInt64 cs = Get_Lzma2_ChunkSize(dict);

  // Solid Block Size
  UInt64 blockSize = cs; // for xz

  if (is7z)
  {
    // we use same default block sizes as defined in 7z encoder
    UInt64 kMaxSize = (UInt64)1 << 32;
    const int methodId = GetMethodID();
    if (methodId == kLZMA2)
    {
      blockSize = cs << 6;
      kMaxSize = (UInt64)1 << 34;
    }
    else
    {
      UInt64 dict2 = dict;
      if (methodId == kBZip2)
      {
        dict2 /= 100000;
        if (dict2 < 1)
          dict2 = 1;
        dict2 *= 100000;
      }
      blockSize = dict2 << 7;
    }

    const UInt32 kMinSize = (UInt32)1 << 24;
    if (blockSize < kMinSize) blockSize = kMinSize;
    if (blockSize > kMaxSize) blockSize = kMaxSize;
  }

  AutoSolid = blockSize;

  int curSel;
  {
    AString s;
    Add_Size(s, AutoSolid);
    Modify_Auto(s);
    curSel = (int)SolidItems.Size();
    AddComboItem(SolidItems, s, (UInt64)(UInt32)(Int32)-1);
  }

  if (is7z)
  {
    UString s ('-');
    // kSolidLog_NoSolid = 0 for xz means default blockSize
    if (is7z)
      LangString(IDS_COMPRESS_NON_SOLID, s);
    const int index = (int)SolidItems.Size();
    AddComboItem(SolidItems, s, (UInt64)kSolidLog_NoSolid);
    if (defaultBlockSize == kSolidLog_NoSolid)
      curSel = index;
  }
  
  for (unsigned i = 20; i <= 36; i++)
  {
    AString s;
    Add_Size(s, (UInt64)1 << i);
    const int index = (int)SolidItems.Size();
    AddComboItem(SolidItems, s, (UInt64)i);
    if (defaultBlockSize != (UInt32)(Int32)-1)
      if (i <= defaultBlockSize || index <= 1)
        curSel = index;
  }
  
  {
    UString s;
    LangString(IDS_COMPRESS_SOLID, s);
    const int index = (int)SolidItems.Size();
    AddComboItem(SolidItems, s, (UInt64)kSolidLog_FullSolid);
    if (defaultBlockSize == kSolidLog_FullSolid)
      curSel = index;
  }

  BlockLogSize = (UInt32)SolidItems[curSel].Value;
}


static const char * const k_ST_Threads = " (ST)";


void CCompressDialogCore::CalcThreads()
{
  AutoNumThreads = 1;
  NumThreads = (UInt32)(Int32)-1;

  ThreadItems.Clear();
  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  if (!fi.MultiThread_())
    return;

  UInt32 numCPUs = 1;            // process threads
  UInt32 numHardwareThreads = 1; // system threads
  NSystem::CProcessAffinity threadsInfo;
  threadsInfo.InitST();
#ifndef Z7_ST
  threadsInfo.Get_and_return_NumProcessThreads_and_SysThreads(numCPUs, numHardwareThreads);
#endif

  AString s ("/ ");
  {
    s.Add_UInt32(numCPUs);
    if (numCPUs != numHardwareThreads)
    {
      s += " / ";
      s.Add_UInt32(numHardwareThreads);
    }
    HardwareThreadsText = GetUnicodeString(s);
  }

  UInt32 defaultValue = numCPUs;
  bool useAutoThreads = true;

  {
    const CArcInfoEx &ai = Get_ArcInfoEx();
    const int index = FindRegistryFormat(ai.Name);
    if (index >= 0)
    {
      const NCompression::CFormatOptions &fo = RegistryInfo.Formats[index];
      if (IsMethodEqualTo(fo.Method) && fo.NumThreads != (UInt32)(Int32)-1)
      {
        defaultValue = fo.NumThreads;
        useAutoThreads = false;
      }
    }
  }

  const int methodID = GetMethodID();
  const bool isZip = IsZipFormat();

  UInt32 numAlgoThreadsMax = numHardwareThreads * 2; // for unknow methods
  if (isZip)
    numAlgoThreadsMax =
        8 << (sizeof(size_t) / 2); // 32 threads for 32-bit : 128 threads for 64-bit
  else if (IsXzFormat())
    numAlgoThreadsMax = 256 * 2; // MTCODER_THREADS_MAX * 2
  else switch (methodID)
  {
    // **************** 7-Zip ZS Modification Start ****************
    case kZSTD: numAlgoThreadsMax = 128; break;
    case kBROTLI: numAlgoThreadsMax = 128; break;
    case kLZ4: numAlgoThreadsMax = 128; break;
    case kLZ5: numAlgoThreadsMax = 128; break;
    case kLIZARD_M1: numAlgoThreadsMax = 128; break;
    case kLIZARD_M2: numAlgoThreadsMax = 128; break;
    case kLIZARD_M3: numAlgoThreadsMax = 128; break;
    case kLIZARD_M4: numAlgoThreadsMax = 128; break;
    case kFLZMA2: numAlgoThreadsMax = 128; break;
    // **************** 7-Zip ZS Modification End ****************
    case kLZMA: numAlgoThreadsMax = 2; break;
    case kLZMA2: numAlgoThreadsMax = 256 * 2; break; // MTCODER_THREADS_MAX * 2
    case kBZip2: numAlgoThreadsMax = 64; break;
    // case kZSTD: numAlgoThreadsMax = num_ZSTD_threads_MAX; break;
    case kCopy:
    case kPPMd:
    case kDeflate:
    case kDeflate64:
    case kPPMdZip:
      numAlgoThreadsMax = 1;
  }
  UInt32 autoThreads = numCPUs;
  if (autoThreads > numAlgoThreadsMax)
    autoThreads = numAlgoThreadsMax;

  const UInt64 memUse_Limit = Get_MemUse_Bytes();

  if (RamSizeDefined)
  if (autoThreads > 1
      // || (autoThreads == 0 && methodID == kZSTD)
      )
  {
    if (isZip)
    {
      for (; autoThreads > 1; autoThreads--)
      {
        const UInt64 dict64 = GetDict2();
        UInt64 decompressMemory;
        const UInt64 usage = GetMemoryUsage_Threads_Dict_DecompMem(autoThreads, dict64, decompressMemory);
        if (usage <= memUse_Limit)
          break;
      }
    }
    else if (methodID == kLZMA2)
    {
      const UInt64 dict64 = GetDict2();
      const UInt32 numThreads1 = (GetLevel2() >= 5 ? 2 : 1);
      UInt32 numBlockThreads = autoThreads / numThreads1;
      for (; numBlockThreads > 1; numBlockThreads--)
      {
        autoThreads = numBlockThreads * numThreads1;
        UInt64 decompressMemory;
        const UInt64 usage = GetMemoryUsage_Threads_Dict_DecompMem(autoThreads, dict64, decompressMemory);
        if (usage <= memUse_Limit)
          break;
      }
      autoThreads = numBlockThreads * numThreads1;
    }
  }

  AutoNumThreads = autoThreads;

  int curSel = -1;
  {
    s.Empty();
    s.Add_UInt32(autoThreads);
    if (autoThreads == 0) s += k_ST_Threads;
    Modify_Auto(s);
    const int index = (int)ThreadItems.Size();
    AddComboItem(ThreadItems, s, (UInt64)(Int64)(Int32)-1);
    if (useAutoThreads)
      curSel = index;
  }

  if (numAlgoThreadsMax != autoThreads || autoThreads != 1)
  for (UInt32 i =
      1;
      i <= numHardwareThreads * 2 && i <= numAlgoThreadsMax; i++)
  {
    s.Empty();
    s.Add_UInt32(i);
    if (i == 0) s += k_ST_Threads;
    const int index = (int)ThreadItems.Size();
    AddComboItem(ThreadItems, s, (UInt64)i);
    if (!useAutoThreads && i == defaultValue)
      curSel = index;
  }
 
  if (curSel >= 0)
    NumThreads = (UInt32)ThreadItems[curSel].Value;
  else
    NumThreads = (UInt32)(Int32)-1;
}


static int AddMemComboItemTo(CCompressDialogCore &core, UInt64 val, bool isPercent, bool isDefault)
{
  UString sUser;
  UString sRegistry;
  if (isPercent)
  {
    UString s;
    s.Add_UInt64(val);
    s.Add_Char('%');
    if (isDefault)
      sUser = k_Auto_Prefix;
    else
      sRegistry = s;
    sUser += s;
  }
  else
  {
    AddMemSize(sUser, val);
    sRegistry = sUser;
    for (;;)
    {
      const int pos = sRegistry.Find(L' ');
      if (pos < 0)
        break;
      sRegistry.Delete(pos);
    }
    if (!sRegistry.IsEmpty())
      if (sRegistry.Back() == 'B')
        sRegistry.DeleteBack();
  }
  const unsigned dataIndex = core.MemUseStrings.Add(sRegistry);
  core.MemUseItems.Add(CCompressDialogCore::COptionItem(sUser, dataIndex));
  return (int)dataIndex;
}


void CCompressDialogCore::CalcMemUse()
{
  MemUseStrings.Clear();
  MemUseItems.Clear();
  MemUseIndex = -1;
  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  MemUseSupported = fi.MemUse_();

  if (!MemUseSupported)
    return;

  UInt64 curMem_Bytes = 0;
  UInt64 curMem_Percents = 0;
  bool needSetCur_Bytes = false;
  bool needSetCur_Percents = false;
  {
    const NCompression::CFormatOptions &fo = Get_FormatOptions();
    if (!fo.MemUse.IsEmpty())
    {
      NCompression::CMemUse mu;
      mu.Parse(fo.MemUse);
      if (mu.IsDefined)
      {
        if (mu.IsPercent)
        {
          curMem_Percents = mu.Val;
          needSetCur_Percents = true;
        }
        else
        {
          curMem_Bytes = mu.GetBytes(RamSizeReduced);
          needSetCur_Bytes = true;
        }
      }
    }
  }

  
  // 80% - is auto usage limit in handlers
  AddMemComboItemTo(*this, 80, true, true);
  MemUseIndex = 0;

  {
    for (unsigned i = 10;; i += 10)
    {
      UInt64 size = i;
      if (i > 100)
        size = (UInt64)(Int64)-1;
      if (needSetCur_Percents && size >= curMem_Percents)
      {
        const int index = AddMemComboItemTo(*this, curMem_Percents, true, false);
        MemUseIndex = index;
        needSetCur_Percents = false;
        if (size == curMem_Percents)
          continue;
      }
      if (size == (UInt64)(Int64)-1)
        break;
      AddMemComboItemTo(*this, size, true, false);
    }
  }
  {
    for (unsigned i = (27) * 2;; i++)
    {
      UInt64 size = (UInt64)(2 + (i & 1)) << (i / 2);
      if (i > (20 + sizeof(size_t) * 3 - 1) * 2)
        size = (UInt64)(Int64)-1;
      if (needSetCur_Bytes && size >= curMem_Bytes)
      {
        const int index = AddMemComboItemTo(*this, curMem_Bytes, false, false);
        MemUseIndex = index;
        needSetCur_Bytes = false;
        if (size == curMem_Bytes)
          continue;
      }
      if (size == (UInt64)(Int64)-1)
        break;
      AddMemComboItemTo(*this, size, false, false);
    }
  }
}


UInt64 CCompressDialogCore::GetMemoryUsage_DecompMem(UInt64 &decompressMemory) const
{
  return GetMemoryUsage_Dict_DecompMem(GetDict2(), decompressMemory);
}


UInt64 CCompressDialogCore::GetMemoryUsage_Dict_DecompMem(UInt64 dict64, UInt64 &decompressMemory) const
{
  return GetMemoryUsage_Threads_Dict_DecompMem(GetNumThreads2(), dict64, decompressMemory);
}

UInt64 CCompressDialogCore::GetMemoryUsage_Threads_Dict_DecompMem(UInt32 numThreads, UInt64 dict64, UInt64 &decompressMemory) const
{
  decompressMemory = (UInt64)(Int64)-1;

  // **************** 7-Zip ZS Modification Start ****************
  //const UInt32 level = GetLevel2();
  UInt32 level = GetLevel2();
  // **************** 7-Zip ZS Modification End ****************
  if (level == 0 && !Get_ArcInfoEx().Is_Zstd())
  {
    decompressMemory = (1 << 20);
    return decompressMemory;
  }
  UInt64 size = 0;

  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  if (fi.Filter_() && level >= 9)
    size += (12 << 20) * 2 + (5 << 20);
  
  UInt32 numMainZipThreads = 1;

  if (IsZipFormat())
  {
    UInt32 numSubThreads = 1;
    if (GetMethodID() == kLZMA && numThreads > 1 && level >= 5)
      numSubThreads = 2;
    numMainZipThreads = numThreads / numSubThreads;
    if (numMainZipThreads > 1)
      size += (UInt64)numMainZipThreads * ((size_t)sizeof(size_t) << 23);
    else
      numMainZipThreads = 1;
  }
  
  const int methodId = GetMethodID();

  if (dict64 == (UInt64)(Int64)-1
      // && methodId != kZSTD
      )
    return (UInt64)(Int64)-1;

  
  switch (methodId)
  {
    case kLZMA:
    case kLZMA2:
    {
      const UInt32 dict = (dict64 >= kLzmaMaxDictSize ? kLzmaMaxDictSize : (UInt32)dict64);
      UInt32 hs = dict - 1;
      hs |= (hs >> 1);
      hs |= (hs >> 2);
      hs |= (hs >> 4);
      hs |= (hs >> 8);
      hs >>= 1;
      if (hs >= (1 << 24))
        hs >>= 1;
      hs |= (1 << 16) - 1;
      // if (numHashBytes >= 5)
      if (level < 5)
        hs |= (256 << 10) - 1;
      hs++;
      UInt64 size1 = (UInt64)hs * 4;
      size1 += (UInt64)dict * 4;
      if (level >= 5)
        size1 += (UInt64)dict * 4;
      size1 += (2 << 20);

      UInt32 numThreads1 = 1;
      if (numThreads > 1 && level >= 5)
      {
        size1 += (2 << 20) + (4 << 20);
        numThreads1 = 2;
      }
      
      UInt32 numBlockThreads = numThreads / numThreads1;
    
      UInt64 chunkSize = 0; // it's solid chunk

      if (methodId != kLZMA && numBlockThreads != 1)
      {
        chunkSize = Get_Lzma2_ChunkSize(dict);

        if (IsXzFormat())
        {
          UInt32 blockSizeLog = GetBlockSizeSpec();
          if (blockSizeLog != (UInt32)(Int32)-1)
          {
            if (blockSizeLog == kSolidLog_FullSolid)
            {
              numBlockThreads = 1;
              chunkSize = 0;
            }
            else if (blockSizeLog != kSolidLog_NoSolid)
              chunkSize = (UInt64)1 << blockSizeLog;
          }
        }
      }

      if (chunkSize == 0)
      {
        const UInt32 kBlockSizeMax = (UInt32)0 - (UInt32)(1 << 16);
        UInt64 blockSize = (UInt64)dict + (1 << 16)
          + (numThreads1 > 1 ? (1 << 20) : 0);
        blockSize += (blockSize >> (blockSize < ((UInt32)1 << 30) ? 1 : 2));
        if (blockSize >= kBlockSizeMax)
          blockSize = kBlockSizeMax;
        size += numBlockThreads * (size1 + blockSize);
      }
      else
      {
        size += numBlockThreads * (size1 + chunkSize);
        const UInt32 numPackChunks = numBlockThreads + (numBlockThreads / 8) + 1;
        if (chunkSize < ((UInt32)1 << 26)) numBlockThreads++;
        if (chunkSize < ((UInt32)1 << 24)) numBlockThreads++;
        if (chunkSize < ((UInt32)1 << 22)) numBlockThreads++;
        size += numPackChunks * chunkSize;
      }

      decompressMemory = dict + (2 << 20);
      return size;
    }

    // **************** 7-Zip ZS Modification Start ****************
    case kFLZMA2:
    {
      const UInt32 dict = (dict64 >= kLzmaMaxDictSize ? kLzmaMaxDictSize : (UInt32)dict64);
      if (level > FL2_MAX_7Z_CLEVEL)
        level = FL2_MAX_7Z_CLEVEL;
      /* dual buffer is enabled in Lzma2Encoder.cpp so size is dict * 6 */
      size += dict * 6 + (1UL << 18) * numThreads;
      UInt32 bufSize = dict >> MATCH_BUFFER_SHIFT;
      if (bufSize > MATCH_BUFFER_ELBOW) {
        UInt32 extra = 0;
        unsigned n = MATCH_BUFFER_ELBOW_BITS - 1;
        for (; (4UL << n) <= bufSize; ++n)
          extra += MATCH_BUFFER_ELBOW >> 4;
        if ((3UL << n) <= bufSize)
          extra += MATCH_BUFFER_ELBOW >> 5;
        bufSize = MATCH_BUFFER_ELBOW + extra;
      }
      size += (bufSize * 12 + RMF_BUILDER_SIZE) * numThreads;
      if (dict > (UInt32(1) << 26))
        size += dict;
      if (FL2_7zCParameters[level].isUltra)
        size += (UInt32(4) << 14) + (UInt32(4) << FL2_7zCParameters[level].chainLog);
      decompressMemory = dict + (2 << 20);
      return size;
    }
    // **************** 7-Zip ZS Modification End ****************
    
    case kPPMd:
    {
      decompressMemory = dict64 + (2 << 20);
      return size + decompressMemory;
    }
    
    case kDeflate:
    case kDeflate64:
    {
      UInt64 size1 = 3 << 20;
      // if (level >= 7)
        size1 += (1 << 20);
      size += size1 * numMainZipThreads;
      decompressMemory = (2 << 20);
      return size;
    }
    
    case kBZip2:
    {
      decompressMemory = (7 << 20);
      UInt64 memForOneThread = (10 << 20);
      return size + memForOneThread * numThreads;
    }
    
    case kPPMdZip:
    {
      decompressMemory = dict64 + (2 << 20);
      return size + (UInt64)decompressMemory * numThreads;
    }
  }
  
  return (UInt64)(Int64)-1;
}


unsigned CCompressDialogCore::GetStaticFormatIndex() const
{
  const CArcInfoEx &ai = Get_ArcInfoEx();
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(g_Formats); i++)
    if (ai.Name.IsEqualTo_Ascii_NoCase(g_Formats[i].Name))
      return i;
  return 0; // -1;
}


int CCompressDialogCore::FindRegistryFormat(const UString &name) const
{
  FOR_VECTOR (i, RegistryInfo.Formats)
  {
    const NCompression::CFormatOptions &fo = RegistryInfo.Formats[i];
    if (name.IsEqualTo_NoCase(GetUnicodeString(fo.FormatID)))
      return (int)i;
  }
  return -1;
}


unsigned CCompressDialogCore::FindRegistryFormat_Always(const UString &name)
{
  const int index = FindRegistryFormat(name);
  if (index >= 0)
    return (unsigned)index;
  {
    NCompression::CFormatOptions fo;
    fo.FormatID = GetSystemString(name);
    return RegistryInfo.Formats.Add(fo);
  }
}


NCompression::CFormatOptions &CCompressDialogCore::Get_FormatOptions()
{
  const CArcInfoEx &ai = Get_ArcInfoEx();
  return RegistryInfo.Formats[FindRegistryFormat_Always(ai.Name)];
}


void CCompressDialogCore::SaveOptionsInMem()
{
  /* these options are for (Info.FormatIndex).
     If it's called just after format changing,
     then it's format that was selected before format changing
     So we store previous format properties */

  Info.Options.Trim();

  const CArcInfoEx &ai = (*ArcFormats)[Info.FormatIndex];
  const unsigned index = FindRegistryFormat_Always(ai.Name);
  NCompression::CFormatOptions &fo = RegistryInfo.Formats[index];
  fo.Options = Info.Options;
  fo.Level = GetLevelSpec();
  {
    const UInt64 dict64 = GetDictSpec();
    UInt32 dict32;
    if (dict64 == (UInt64)(Int64)-1)
      dict32 = (UInt32)(Int32)-1;
    else
    {
      dict32 = (UInt32)dict64;
      if (dict64 != dict32)
      {
        /* here we must write 32-bit value for registry that indicates big_value
           (UInt32)(Int32)-1  : is used as marker for default size
           (UInt32)(Int32)-2  : it can be used to indicate big value (4 GiB)
           the value must be larger than threshold
        */
        dict32 = (UInt32)(Int32)-2;
        // dict32 = kLzmaMaxDictSize; // it must be larger than threshold
      }
    }
    fo.Dictionary = dict32;
  }

  fo.Order = GetOrderSpec();
  // **************** 7-Zip ZS Modification Start ****************
  // fo.Method = GetMethodSpec();
  GetMethodSpec(fo.Method);
  // **************** 7-Zip ZS Modification End ****************
  fo.EncryptionMethod = GetEncryptionMethodSpec();
  fo.NumThreads = GetNumThreadsSpec();
  fo.BlockLogSize = GetBlockSizeSpec();
  fo.MemUse = Get_MemUse_Spec();
}


bool CCompressDialogCore::SetArcPathFields(const UString &path, UString &name, bool always)
{
  FString resDirPrefix;
  FString resFileName;
  const bool res = GetFullPathAndSplit(us2fs(path), resDirPrefix, resFileName);
  if (res)
  {
    DirPrefix = fs2us(resDirPrefix);
    name = fs2us(resFileName);
  }
  else
  {
    if (!always)
      return false;
    DirPrefix.Empty();
    name = path;
  }
  ArchiveName = name;
  return res;
}


bool CCompressDialogCore::GetFinalPath_Smart(UString &resPath) const
{
  resPath.Empty();
  UString name = ArchiveName;
  name.Trim();
  FString fullPath;
  UString dirPrefx = DirPrefix;
  if (dirPrefx.IsEmpty())
    dirPrefx = StartDirPrefix;
  const bool res = !dirPrefx.IsEmpty() ?
      NName::GetFullPath(us2fs(dirPrefx), us2fs(name), fullPath):
      NName::GetFullPath(                 us2fs(name), fullPath);
  if (res)
    resPath = fs2us(fullPath);
  return res;
}


void CCompressDialogCore::SetArchiveName(const UString &name)
{
  UString fileName = name;
  Info.FormatIndex = (int)GetFormatIndex();
  const CArcInfoEx &ai = (*ArcFormats)[Info.FormatIndex];
  PrevFormat = Info.FormatIndex;
  if (ai.Flags_KeepName())
  {
    fileName = OriginalFileName;
  }
  else
  {
    if (!Info.KeepName)
    {
      int dotPos = GetExtDotPos(fileName);
      if (dotPos >= 0)
        fileName.DeleteFrom(dotPos);
    }
  }

  if (IsSfx())
    fileName += kExeExt;
  else
  {
    fileName.Add_Dot();
    UString ext = ai.GetMainExt();
    if (ai.Flags_HashHandler())
    {
      UString estimatedName;
      GetMethodSpec(estimatedName);
      if (!estimatedName.IsEmpty())
      {
        ext = estimatedName;
        ext.MakeLower_Ascii();
      }
    }
    fileName += ext;
  }
  ArchiveName = fileName;
}


void CCompressDialogCore::SetArchiveName2(bool prevWasSFX)
{
  UString fileName = ArchiveName;
  const CArcInfoEx &prevArchiverInfo = (*ArcFormats)[PrevFormat];
  if (prevArchiverInfo.Flags_KeepName() || Info.KeepName)
  {
    UString prevExtension;
    if (prevWasSFX)
      prevExtension = kExeExt;
    else
    {
      prevExtension.Add_Dot();
      prevExtension += prevArchiverInfo.GetMainExt();
    }
    const unsigned prevExtensionLen = prevExtension.Len();
    if (fileName.Len() >= prevExtensionLen)
      if (StringsAreEqualNoCase(fileName.RightPtr(prevExtensionLen), prevExtension))
        fileName.DeleteFrom(fileName.Len() - prevExtensionLen);
  }
  SetArchiveName(fileName);
}


// Appends the current format's main extension when the archive name has
// none (the user replaced the whole selected file name). Existing
// extensions are left alone; SFX mode appends .exe.
void CCompressDialogCore::EnsureArchiveExtension()
{
  UString name = ArchiveName;
  name.Trim();
  if (name.IsEmpty())
    return;
  if (GetExtDotPos(name) >= 0)
    return;
  if (IsSfx())
  {
    name += kExeExt;
  }
  else
  {
    name.Add_Dot();
    name += Get_ArcInfoEx().GetMainExt();
  }
  ArchiveName = name;
}


bool CCompressDialogCore::ArcPathChanged(const UString &path)
{
  const int dotPos = GetExtDotPos(path);
  if (dotPos < 0)
    return false;
  const UString ext = path.Ptr(dotPos + 1);
  {
    const CArcInfoEx &ai = Get_ArcInfoEx();
    if (ai.FindExtension(ext) >= 0)
      return false;
  }

  const unsigned count = (unsigned)FormatItems.Size();
  for (unsigned i = 0; i < count; i++)
  {
    const CArcInfoEx &ai = (*ArcFormats)[(unsigned)FormatItems[i].Value];
    if (ai.FindExtension(ext) >= 0)
    {
      FormatIndex = (int)FormatItems[i].Value;
      SaveOptionsInMem();
      FormatChanged(true); // isChanged
      return true;
    }
  }
  return false;
}


void CCompressDialogCore::CheckSFX()
{
  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  bool enable = fi.SFX_();
  if (enable)
  {
    const int methodID = GetMethodID();
    enable = (methodID == -1 || IsMethodSupportedBySfx(methodID));
  }
  SfxSupported = enable;
  if (!enable)
    SfxChecked = false;
}


void CCompressDialogCore::CheckSFXNameChange()
{
  const bool isSFX = IsSfx();
  CheckSFX();
  if (isSFX != IsSfx())
    SetArchiveName2(isSFX);
}


void CCompressDialogCore::FormatChanged(bool isChanged)
{
  // **************** 7-Zip ZS Modification Start ****************
  CalcMethods();
  // **************** 7-Zip ZS Modification End ****************
  CalcLevels();
  CalcMethods();
  CalcSolidBlockSize();
  CalcMemUse();
  CalcThreads();

  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  Info.SolidIsSpecified = fi.Solid_();
  Info.EncryptHeadersIsAllowed = fi.EncryptFileNames_();

  CheckSFX();

  {
    if (!isChanged)
    {
      SET_GUI_BOOL (SymLinks);
      SET_GUI_BOOL (HardLinks);
      SET_GUI_BOOL (AltStreams);
      SET_GUI_BOOL (NtSecurity);
      SET_GUI_BOOL (PreserveATime);
    }

    PreserveATime.Supported = true;

    {
      const CArcInfoEx &ai = Get_ArcInfoEx();
      SymLinks.Supported   = ai.Flags_SymLinks();
      HardLinks.Supported  = ai.Flags_HardLinks();
      AltStreams.Supported = ai.Flags_AltStreams();
      NtSecurity.Supported = ai.Flags_NtSecurity();
    }

    UpdateOptionsSummary();
  }

  EncryptSupported = fi.Encrypt_();
  EncryptFileNamesSupported = fi.EncryptFileNames_();

  CalcEncryptionMethod();
}


static void AddText_from_BoolPair(AString &s, const char *name, const CBoolPair &bp)
{
  if (bp.Def)
  {
    s.Add_OptSpaced(name);
    if (!bp.Val)
      s += "-";
  }
}


static void AddText_from_Bool1(AString &s, const char *name, const CBool1 &b)
{
  if (b.Supported && b.Val)
    s.Add_OptSpaced(name);
}


void CCompressDialogCore::UpdateOptionsSummary()
{
  NCompression::CFormatOptions &fo = Get_FormatOptions();

  AString s;
  if (fo.IsSet_TimePrec())
  {
    s.Add_OptSpaced("tp");
    s.Add_UInt32(fo.TimePrec);
  }
  AddText_from_BoolPair(s, "tm", fo.MTime);
  AddText_from_BoolPair(s, "tc", fo.CTime);
  AddText_from_BoolPair(s, "ta", fo.ATime);
  AddText_from_BoolPair(s, "-stl", fo.SetArcMTime);

  AddText_from_Bool1(s, "SL",  SymLinks);
  AddText_from_Bool1(s, "HL",  HardLinks);
  AddText_from_Bool1(s, "AS",  AltStreams);
  AddText_from_Bool1(s, "Sec", NtSecurity);

  OptionsSummaryText = GetUnicodeString(s);
}


void CCompressDialogCore::UpdateMemoryTexts()
{
  UInt64 decompressMem;
  const UInt64 memUsage = GetMemoryUsage_DecompMem(decompressMem);

  if (decompressMem == (UInt64)(Int64)-1)
    DecompressMemoryText = L"?";
  else
  {
    UString s;
    AddMemUsage(s, decompressMem);
    DecompressMemoryText = s;
  }

  if (memUsage == (UInt64)(Int64)-1)
    MemoryValueText = L"?";
  else
  {
    UString s;
    AddMemUsage(s, memUsage);
    const UString mus = Get_MemUse_Spec();
    NCompression::CMemUse mu;
    if (!mus.IsEmpty())
      mu.Parse(mus);
    if (mu.IsDefined)
    {
      s += " / ";
      AddMemUsage(s, mu.GetBytes(RamSizeReduced));
    }
    else if (RamSizeDefined)
    {
      s += " / ";
      AddMemUsage(s, RamUsageAuto);
    }

    if (RamSizeDefined)
    {
      s += " / ";
      AddMemUsage(s, RamSize);
    }
    MemoryValueText = s;
  }
}


CCompressDialogCore::ECommitResult CCompressDialogCore::ValidateAndCommit(UString &errorMessage)
{
  errorMessage.Empty();
  VolumeConfirmText.Empty();

  Info.Password = Password;
  if (IsZipFormat())
  {
    if (!IsAsciiString(Info.Password))
    {
      errorMessage = LangString(IDS_PASSWORD_USE_ASCII);
      return kCommitBlocked;
    }
    UString method = GetEncryptionMethodSpec();
    if (method.IsPrefixedBy_Ascii_NoCase("aes"))
    {
      if (Info.Password.Len() > 99)
      {
        errorMessage = LangString(IDS_PASSWORD_TOO_LONG);
        return kCommitBlocked;
      }
    }
  }
  if (!ShowPassword)
  {
    if (PasswordConfirmation != Info.Password)
    {
      errorMessage = LangString(IDS_PASSWORD_NOT_MATCH);
      return kCommitBlocked;
    }
  }

  {
    UInt64 decompressMem;
    const UInt64 memUsage = GetMemoryUsage_DecompMem(decompressMem);
    if (memUsage != (UInt64)(Int64)-1)
    {
      const UInt64 limit = Get_MemUse_Bytes();
      if (memUsage > limit)
      {
        UString s2;
        LangString_OnlyFromLangFile(IDS_MEM_REQUIRED_MEM_SIZE, s2);
        if (s2.IsEmpty())
        {
          s2 = LangString(IDT_COMPRESS_MEMORY);
          if (s2.IsEmpty())
            s2 = L"Memory"; // RC 默认文本（非 Z7_LANG 构建）
          s2.RemoveChar(L':');
        }
        UString s;
        SetErrorMessage_MemUsage(s, memUsage, RamSize, limit, s2);
        errorMessage = s;
        return kCommitBlocked;
      }
    }
  }

  SaveOptionsInMem();

  UStringVector arcPaths;
  {
    UString s;
    if (!GetFinalPath_Smart(s))
    {
      errorMessage = k_IncorrectPathMessage;
      return kCommitBlocked;
    }
    Info.ArcPath = s;
    AddUniqueString(arcPaths, s);
  }
  
  Info.Level = GetLevelSpec();
  Info.Dict64 = GetDictSpec();
  // Info.Dict64_Chain = GetDictChainSpec();
  Info.Order = GetOrderSpec();
  Info.OrderMode = GetOrderMode();
  Info.NumThreads = GetNumThreadsSpec();

  Info.MemUsage.Clear();
  {
    const UString mus = Get_MemUse_Spec();
    if (!mus.IsEmpty())
    {
      NCompression::CMemUse mu;
      mu.Parse(mus);
      if (mu.IsDefined)
        Info.MemUsage = mu;
    }
  }

  {
    // Info.SolidIsSpecified = g_Formats[GetStaticFormatIndex()].Solid;
    const UInt32 solidLogSize = GetBlockSizeSpec();
    Info.SolidBlockSize = 0;
    if (solidLogSize == (UInt32)(Int32)-1)
      Info.SolidIsSpecified = false;
    else if (solidLogSize > 0)
      Info.SolidBlockSize = (solidLogSize >= 64) ?
          (UInt64)(Int64)-1 :
          ((UInt64)1 << solidLogSize);
  }

  Info.Method = GetMethodSpec();
  Info.EncryptionMethod = GetEncryptionMethodSpec();
  Info.FormatIndex = (int)GetFormatIndex();
  Info.SFXMode = IsSfx();
  Info.OpenShareForWrite = OpenShareForWrite;
  Info.DeleteAfterCompressing = DeleteAfterCompressing;

  RegistryInfo.EncryptHeaders =
    Info.EncryptHeaders = EncryptHeadersChecked;

  {
    /* Info properties could be for another archive types.
       so we disable unsupported properties in Info */

    SET_FINAL_BOOL_PAIRS (SymLinks);
    SET_FINAL_BOOL_PAIRS (HardLinks);
    SET_FINAL_BOOL_PAIRS (AltStreams);
    SET_FINAL_BOOL_PAIRS (NtSecurity);

    SET_FINAL_BOOL_PAIRS (PreserveATime);
  }

  {
    const NCompression::CFormatOptions &fo = Get_FormatOptions();

    Info.TimePrec = fo.TimePrec;
    Info.MTime = fo.MTime;
    Info.CTime = fo.CTime;
    Info.ATime = fo.ATime;
    Info.SetArcMTime = fo.SetArcMTime;
  }

  UString volumeString = VolumeText;
  volumeString.Trim();
  Info.VolumeSizes.Clear();
  
  if (!volumeString.IsEmpty())
  {
    if (!ParseVolumeSizes(volumeString, Info.VolumeSizes))
    {
      errorMessage = LangString(IDS_INCORRECT_VOLUME_SIZE);
      return kCommitBlocked;
    }
    if (!Info.VolumeSizes.IsEmpty())
    {
      const UInt64 volumeSize = Info.VolumeSizes.Back();
      if (volumeSize < (100 << 10) && !VolumeConfirmed)
      {
        wchar_t s[32];
        ConvertUInt64ToString(volumeSize, s);
        VolumeConfirmText = MyFormatNew(IDS_SPLIT_CONFIRM, s);
        return kCommitNeedVolumeConfirm;
      }
    }
  }

  if (Info.FormatIndex >= 0)
    RegistryInfo.ArcType = (*ArcFormats)[Info.FormatIndex].Name;
  RegistryInfo.ShowPassword = ShowPassword;

  FOR_VECTOR (i, RegistryInfo.ArcPaths)
  {
    if (arcPaths.Size() >= kHistorySize)
      break;
    AddUniqueString(arcPaths, RegistryInfo.ArcPaths[i]);
  }
  RegistryInfo.ArcPaths = arcPaths;

  RegistryInfo.Save();

  return kCommitOk;
}


void CCompressDialogCore::OnFormatSelected(int arcIndex)
{
  const bool isSFX = IsSfx();
  SaveOptionsInMem();
  FormatIndex = arcIndex;
  FormatChanged(true); // isChanged
  SetArchiveName2(isSFX);
}


void CCompressDialogCore::OnLevelSelected(UInt32 value)
{
  Level = value;
  Get_FormatOptions().ResetForLevelChange();
  CalcDictionary();
  CalcOrder();
  CalcSolidBlockSize();
  CalcThreads();
  CheckSFXNameChange();
}


void CCompressDialogCore::OnMethodSelected(int methodId)
{
  MethodID = methodId;
  ComprMethodChanged();
  CalcDictionary();
  CalcOrder();
  CalcLevels();
  CalcSolidBlockSize();
  CalcThreads();
  CheckSFXNameChange();
  if (Get_ArcInfoEx().Flags_HashHandler())
    SetArchiveName2(false);
}


void CCompressDialogCore::OnDictionarySelected(UInt64 value)
{
  Dict64 = value;
  SaveOptionsInMem();
  const UInt32 blockSizeLog = GetBlockSizeSpec();
  if (// blockSizeLog != (UInt32)(Int32)-1 &&
         blockSizeLog != kSolidLog_NoSolid
      && blockSizeLog != kSolidLog_FullSolid)
  {
    Get_FormatOptions().Reset_BlockLogSize();
    // SetSolidBlockSize(true);
  }
  CalcDictionary();
  CalcSolidBlockSize();
  CalcThreads(); // we want to change the reported threads for Auto line only
}


void CCompressDialogCore::OnOrderSelected(UInt32 value)
{
  Order = value;
}


void CCompressDialogCore::OnSolidSelected(UInt32 value)
{
  BlockLogSize = value;
}


void CCompressDialogCore::OnThreadsSelected(UInt32 value)
{
  NumThreads = value;
}


void CCompressDialogCore::OnMemUseSelected(int memUseStringsIndex)
{
  MemUseIndex = memUseStringsIndex;
  SaveOptionsInMem();
  CalcThreads(); // we want to change the reported threads for Auto line only
}


void CCompressDialogCore::OnEncryptionMethodSelected(int itemIndex)
{
  EncryptionMethodIndex = itemIndex;
}


void CCompressDialogCore::OnSfxChecked(bool checked)
{
  SfxChecked = checked;
  CalcMethods(GetMethodID());
  UString fileName = ArchiveName;
  const int dotPos = GetExtDotPos(fileName);
  if (SfxChecked)
  {
    if (dotPos >= 0)
      fileName.DeleteFrom(dotPos);
    fileName += kExeExt;
    ArchiveName = fileName;
  }
  else
  {
    if (dotPos >= 0)
    {
      const UString ext = fileName.Ptr(dotPos);
      if (ext.IsEqualTo_Ascii_NoCase(kExeExt))
      {
        fileName.DeleteFrom(dotPos);
        ArchiveName = fileName;
      }
    }
    SetArchiveName2(false); // it's for OnInit
  }
}


