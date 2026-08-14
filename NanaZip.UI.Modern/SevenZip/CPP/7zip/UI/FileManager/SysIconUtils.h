// SysIconUtils.h

#ifndef __SYS_ICON_UTILS_H
#define __SYS_ICON_UTILS_H

#include "../../../Common/MyWindows.h"

#include <CommCtrl.h>

#include "../../../Common/MyString.h"

struct CExtIconPair
{
  UString Ext;
  int IconIndex;
  // UString TypeName;

  // int Compare(const CExtIconPair &a) const { return MyStringCompareNoCase(Ext, a.Ext); }
};

struct CAttribIconPair
{
  DWORD Attrib;
  int IconIndex;
  // UString TypeName;

  // int Compare(const CAttribIconPair &a) const { return Ext.Compare(a.Ext); }
};

class CExtToIconMap
{
public:
  CRecordVector<CAttribIconPair> _attribMap;
  CObjectVector<CExtIconPair> _extMap;
  int SplitIconIndex;
  int SplitIconIndex_Defined;
  
  CExtToIconMap(): SplitIconIndex_Defined(false) {}

  void Clear()
  {
    SplitIconIndex_Defined = false;
    _extMap.Clear();
    _attribMap.Clear();
  }
  int GetIconIndex(DWORD attrib, const wchar_t *fileName /* , UString *typeName */);
  // int GetIconIndex(DWORD attrib, const UString &fileName);
};

DWORD_PTR GetRealIconIndex(CFSTR path, DWORD attrib, int &iconIndex);
int GetIconIndexForCSIDL(int csidl);

// File-type description from the shell (e.g. "Text Document"), localized by
// the system. Works on the file name plus attributes (SHGFI_USEFILEATTRIBUTES),
// so it never touches the disk. Returns an empty string when the shell has no
// description for the extension (caller falls back to the extension itself).
UString GetFileTypeName(const UString &fileName, DWORD attrib);

inline HIMAGELIST GetSysImageList(bool smallIcons)
{
  SHFILEINFO shellInfo;
  return (HIMAGELIST)SHGetFileInfo(TEXT(""),
      FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_DIRECTORY,
      &shellInfo, sizeof(shellInfo),
      SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX | (smallIcons ? SHGFI_SMALLICON : SHGFI_ICON));
}

#endif
