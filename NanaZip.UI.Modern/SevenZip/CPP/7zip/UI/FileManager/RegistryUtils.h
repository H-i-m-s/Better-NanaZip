// RegistryUtils.h

#ifndef __REGISTRY_UTILS_H
#define __REGISTRY_UTILS_H

#include "../../../Common/MyTypes.h"
#include "../../../Common/MyString.h"

void SaveRegLang(const UString &path);
void ReadRegLang(UString &path);

void SaveRegEditor(bool useEditor, const UString &path);
void ReadRegEditor(bool useEditor, UString &path);

void SaveRegDiff(const UString &path);
void ReadRegDiff(UString &path);

void ReadReg_VerCtrlPath(UString &path);

struct CFmSettings
{
  bool ShowDots;
  bool ShowRealFileIcons;
  bool FullRow;
  bool ShowGrid;
  bool SingleClick;
  bool AlternativeSelection;
  bool ArcHistory;
  bool PathHistory;
  bool CopyHistory;
  bool FolderHistory;
  bool LowercaseHashes;
  bool SizeFormat;   // show human-readable sizes (K/M/G) in the file list
  bool DeleteAfterExtract;   // delete archives after successful extraction
  bool DeletePermanently;    // delete without moving to the Recycle Bin
  // **************** SSS Modification Start (extraction settings) ****************
  bool AutoQueryCloud;       // auto query cloud password API at extract time
  bool AutoMatchLocal;       // auto try local password book entries
  UInt32 MatchPriority;      // 0 = local first (default), 1 = cloud first
  bool AutoShowPassword;     // default checked state of "show password" in extract dialog
  bool AutoSharePassword;    // default checked state of "share password" in extract/password dialogs
  // **************** SSS Modification End ****************
  // bool Underline;

  bool ShowSystemMenu;

  void Save() const;
  void Load();
};

// void SaveLockMemoryAdd(bool enable);
// bool ReadLockMemoryAdd();

bool ReadLockMemoryEnable();
void SaveLockMemoryEnable(bool enable);

bool WantArcHistory();
bool WantPathHistory();
bool WantCopyHistory();
bool WantFolderHistory();
bool WantLowercaseHashes();
bool WantSizeFormat();
bool WantDeleteAfterExtract();
bool WantDeletePermanently();
// **************** SSS Modification Start (extraction settings) ****************
bool WantAutoQueryCloud();
bool WantAutoMatchLocal();
UInt32 ReadMatchPriority();
bool WantAutoShowPassword();
bool WantAutoSharePassword();
// **************** SSS Modification End ****************

UInt32 ReadFileContextMenuFlags();
void SaveFileContextMenuFlags(UInt32 flags);

void SaveFlatView(UInt32 panelIndex, bool enable);
bool ReadFlatView(UInt32 panelIndex);

/*
void Save_ShowDeleted(bool enable);
bool Read_ShowDeleted();
*/

#endif
