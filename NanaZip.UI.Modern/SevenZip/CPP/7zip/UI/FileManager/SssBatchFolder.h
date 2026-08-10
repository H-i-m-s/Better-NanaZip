// SssBatchFolder.h
// In-memory virtual folder that presents a set of selected archives as a
// normal folder listing inside the file-manager window. No persistence:
// the folder lives only while the panel holds it.

#ifndef __SSS_BATCH_FOLDER_H
#define __SSS_BATCH_FOLDER_H

#include "../../../Common/MyCom.h"
#include "../../../Common/MyString.h"
#include "../../../Common/MyVector.h"

#include "../../PropID.h"

#include "IFolder.h"

// Custom property used for the per-item extraction state column.
const PROPID kpidSSSStatus = kpidUserDefined + 1;

class CSssBatchFolder:
  public IFolderFolder,
  public IFolderGetSystemIconIndex,
  public IFolderCompare,
  public CMyUnknownImp
{
  UStringVector _paths;       // full paths of the archives
  UStringVector _names;       // file names only
  UStringVector _dirs;        // parent directories (with tail slash)
  CRecordVector<UInt64> _sizes;
  CRecordVector<int> _iconIndices;
  CRecordVector<Byte> _states; // 0=pending 1=extracting 2=done 3=failed

public:
  MY_UNKNOWN_IMP2(IFolderGetSystemIconIndex, IFolderCompare)
  INTERFACE_FolderFolder(;)
  STDMETHOD(GetSystemIconIndex)(UInt32 index, Int32 *iconIndex);
  STDMETHOD_(Int32, CompareItems)(UInt32 index1, UInt32 index2, PROPID propID, Int32 propIsRaw);

  void Init(const UStringVector &paths);
  // Append late-arriving archives (Explorer launches the multi-select
  // open command in chunks); duplicates are skipped.
  void Append(const UStringVector &paths);
  void SetState(unsigned index, Byte state);

  const UStringVector &GetPaths() const { return _paths; }
  unsigned GetNumItems() const { return (unsigned)_paths.Size(); }
  static const wchar_t *GetTypeString() { return L"SssBatchFolder"; }
};

// True if the folder is the batch view (checked via kpidType).
bool IsSssBatchFolder(IFolderFolder *folder);


#endif
