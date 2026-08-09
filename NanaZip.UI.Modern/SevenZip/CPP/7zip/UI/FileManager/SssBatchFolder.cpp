// SssBatchFolder.cpp

#include "StdAfx.h"

#include "SssBatchFolder.h"

#include "../../../Common/IntToString.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileIO.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariant.h"

#include "../../Common/FileStreams.h"

#include "../Agent/Agent.h"

#include "SysIconUtils.h"

using namespace NWindows;

namespace
{
  enum
  {
    kSssStatePending = 0,
    kSssStateExtracting = 1,
    kSssStateDone = 2,
    kSssStateFailed = 3
  };

  static const wchar_t *GetStateString(Byte state)
  {
    switch (state)
    {
      case kSssStateExtracting: return L"解压中…";
      case kSssStateDone: return L"已完成";
      case kSssStateFailed: return L"失败";
      default: return L"待处理";
    }
  }

  static const PROPID kProps[] =
  {
    kpidName,
    kpidSize,
    kpidSSSStatus
  };
}

void CSssBatchFolder::Init(const UStringVector &paths)
{
  _paths.Clear();
  _names.Clear();
  _dirs.Clear();
  _sizes.Clear();
  _iconIndices.Clear();
  _states.Clear();

  FOR_VECTOR(i, paths)
  {
    const UString &path = paths[i];
    NFile::NFind::CFileInfo fi;
    if (!fi.Find(us2fs(path)))
      continue;
    _paths.Add(path);
    _names.Add(fs2us(fi.Name));
    UString dir;
    {
      FString fullPath;
      FString dirPrefix;
      if (NFile::NName::GetFullPath(us2fs(path), fullPath) &&
          NFile::NDir::GetOnlyDirPrefix(fullPath, dirPrefix))
        dir = fs2us(dirPrefix);
    }
    _dirs.Add(dir);
    _sizes.Add(fi.Size);
    int iconIndex = -1;
    GetRealIconIndex(us2fs(path), 0, iconIndex);
    _iconIndices.Add(iconIndex);
    _states.Add(kSssStatePending);
  }
}

void CSssBatchFolder::SetState(unsigned index, Byte state)
{
  if (index < _states.Size())
    _states[index] = state;
}

STDMETHODIMP CSssBatchFolder::LoadItems()
{
  return S_OK;
}

STDMETHODIMP CSssBatchFolder::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = _paths.Size();
  return S_OK;
}

STDMETHODIMP CSssBatchFolder::GetProperty(UInt32 itemIndex, PROPID propID, PROPVARIANT *value)
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidIsDir:  prop = true; break;
    case kpidName:  prop = _names[itemIndex]; break;
    case kpidSize:  prop = _sizes[itemIndex]; break;
    case kpidSSSStatus:  prop = GetStateString(_states[itemIndex]); break;
  }
  prop.Detach(value);
  return S_OK;
}

STDMETHODIMP CSssBatchFolder::BindToFolder(UInt32 index, IFolderFolder **resultFolder)
{
  *resultFolder = NULL;
  if (index >= _paths.Size())
    return E_INVALIDARG;
  CMyComPtr<IInStream> inStream;
  CInFileStream *inStreamSpec = new CInFileStream;
  inStream = inStreamSpec;
  if (!inStreamSpec->Open(us2fs(_paths[index])))
    return E_FAIL;
  CArchiveFolderManager manager;
  CMyComPtr<IFolderFolder> folder;
  HRESULT res = manager.OpenFolderFile(inStream, _paths[index], L"", &folder, NULL);
  if (res != S_OK)
    return res;
  *resultFolder = folder.Detach();
  return S_OK;
}

STDMETHODIMP CSssBatchFolder::BindToFolder(const wchar_t *name, IFolderFolder **resultFolder)
{
  *resultFolder = NULL;
  const UString name2 = name;
  FOR_VECTOR(i, _names)
    if (_names[i].IsEqualTo_NoCase(name2))
      return BindToFolder(i, resultFolder);
  return E_INVALIDARG;
}

STDMETHODIMP CSssBatchFolder::BindToParentFolder(IFolderFolder **resultFolder)
{
  *resultFolder = NULL;
  return S_OK;
}

STDMETHODIMP CSssBatchFolder::GetNumberOfProperties(UInt32 *numProperties)
{
  *numProperties = ARRAY_SIZE(kProps);
  return S_OK;
}

STDMETHODIMP CSssBatchFolder::GetPropertyInfo(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType)
{
  if (index >= ARRAY_SIZE(kProps))
    return E_INVALIDARG;
  *propID = kProps[index];
  // Custom property: the standard VARTYPE table does not cover it.
  *varType = (*propID == kpidSSSStatus) ? VT_BSTR : k7z_PROPID_To_VARTYPE[(unsigned)*propID];
  *name = 0;
  return S_OK;
}

STDMETHODIMP CSssBatchFolder::GetFolderProperty(PROPID propID, PROPVARIANT *value)
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidType: prop = GetTypeString(); break;
    case kpidPath:
    {
      UString s = L"批量归档";
      if (_paths.Size() > 0)
      {
        s += L"（";
        s.Add_UInt32(_paths.Size());
        s += L" 个）";
      }
      prop = s;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
}

STDMETHODIMP CSssBatchFolder::GetSystemIconIndex(UInt32 index, Int32 *iconIndex)
{
  *iconIndex = _iconIndices[index];
  return S_OK;
}

STDMETHODIMP_(Int32) CSssBatchFolder::CompareItems(UInt32 index1, UInt32 index2, PROPID propID, Int32 /* propIsRaw */)
{
  switch (propID)
  {
    case kpidName:
      return CompareFileNames_ForFolderList(_names[index1], _names[index2]);
    case kpidSize:
    {
      const UInt64 v1 = _sizes[index1];
      const UInt64 v2 = _sizes[index2];
      return (v1 < v2) ? -1 : ((v1 > v2) ? 1 : 0);
    }
    case kpidSSSStatus:
    {
      const Byte s1 = _states[index1];
      const Byte s2 = _states[index2];
      return (s1 < s2) ? -1 : ((s1 > s2) ? 1 : 0);
    }
  }
  return 0;
}

bool IsSssBatchFolder(IFolderFolder *folder)
{
  if (!folder)
    return false;
  NCOM::CPropVariant prop;
  if (folder->GetFolderProperty(kpidType, &prop) != S_OK || prop.vt != VT_BSTR)
    return false;
  return (UString(prop.bstrVal) == CSssBatchFolder::GetTypeString());
}

// **************** SSS Modification Start ****************
void SssLog(const wchar_t *msg)
{
  ::OutputDebugStringW(msg);
  ::OutputDebugStringW(L"\n");
  wchar_t temp[MAX_PATH];
  if (::GetTempPathW(MAX_PATH, temp) == 0)
    return;
  UString full(temp);
  full += L"sss_batch_debug.log";
  HANDLE h = ::CreateFileW(full, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
      OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return;
  wchar_t buf[512];
  const DWORD tick = ::GetTickCount();
  wsprintfW(buf, L"[%u] %s\r\n", tick, msg);
  DWORD written = 0;
  ::WriteFile(h, buf, (DWORD)(MyStringLen(buf) * sizeof(wchar_t)), &written, NULL);
  ::CloseHandle(h);
}
// **************** SSS Modification End ****************
