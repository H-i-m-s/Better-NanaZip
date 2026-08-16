// LinkDialog.cpp

#include "StdAfx.h"

#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileIO.h"
#include "../../../Windows/FileName.h"

#ifdef LANG
#include "LangUtils.h"
#endif

#include "BrowseDialog.h"
#include "CopyDialogRes.h"
#include "LinkDialog.h"
#include "resourceGui.h"

// **************** NanaZip Modification Start ****************
#include <NanaZip.Modern.h>
// **************** NanaZip Modification End ****************

#include "App.h"

#include "resource.h"

extern bool g_SymLink_Supported;

using namespace NWindows;
using namespace NFile;

#ifdef LANG
static const UInt32 kLangIDs[] =
{
  IDB_LINK_LINK,
  IDT_LINK_PATH_FROM,
  IDT_LINK_PATH_TO,
  IDG_LINK_TYPE,
  IDR_LINK_TYPE_HARD,
  IDR_LINK_TYPE_SYM_FILE,
  IDR_LINK_TYPE_SYM_DIR,
  IDR_LINK_TYPE_JUNCTION,
  IDR_LINK_TYPE_WSL
};
#endif


// **************** NanaZip Modification Start ****************
// Backported from 25.00.
static bool GetSymLink(CFSTR path, CReparseAttr &attr, UString &errorMessage)
{
  CByteBuffer buf;
  if (!NIO::GetReparseData(path, buf, NULL))
    return false;
  if (!attr.Parse(buf, buf.Size()))
  {
    SetLastError(attr.ErrorCode);
    return false;
  }
  CByteBuffer data2;
  FillLinkData(data2, attr.GetPath(),
      !attr.IsMountPoint(), attr.IsSymLink_WSL());
  if (data2.Size() == 0)
  {
    errorMessage = "Cannot reproduce reparse point";
    return false;
  }
  if (data2 != buf)
  {
    errorMessage = "mismatch for reproduced reparse point";
    return false;
  }
  return true;
}
// **************** NanaZip Modification End ****************


static const int k_LinkType_Buttons[] =
{
  IDR_LINK_TYPE_HARD,
  IDR_LINK_TYPE_SYM_FILE,
  IDR_LINK_TYPE_SYM_DIR,
  IDR_LINK_TYPE_JUNCTION,
  IDR_LINK_TYPE_WSL
};

void CLinkDialog::Set_LinkType_Radio(int idb)
{
  CheckRadioButton(k_LinkType_Buttons[0], k_LinkType_Buttons[ARRAY_SIZE(k_LinkType_Buttons) - 1], idb);
}

bool CLinkDialog::OnInit()
{
  #ifdef LANG
  LangSetWindowText(*this, IDD_LINK);
  LangSetDlgItems(*this, kLangIDs, ARRAY_SIZE(kLangIDs));
  #endif

  _pathFromCombo.Attach(GetItem(IDC_LINK_PATH_FROM));
  _pathToCombo.Attach(GetItem(IDC_LINK_PATH_TO));

  if (!FilePath.IsEmpty())
  {
    NFind::CFileInfo fi;
    int linkType = 0;
    if (!fi.Find(us2fs(FilePath)))
      linkType = IDR_LINK_TYPE_SYM_FILE;
    else
    {
      if (fi.HasReparsePoint())
      {
        CReparseAttr attr;
        UString error;
        bool res = GetSymLink(us2fs(FilePath), attr, error);
        if (!res && error.IsEmpty())
        {
          DWORD lastError = GetLastError();
          if (lastError != 0)
            error = NError::MyFormatMessage(lastError);
        }

        UString s = attr.GetPath();
        if (!attr.IsSymLink_WSL())
        if (!attr.IsOkNamePair())
        {
          s += " : ";
          s += attr.PrintName;
        }

        if (!res)
        {
          s.Insert(0, L"ERROR: ");
          if (!error.IsEmpty())
          {
            s += " : ";
            s += error;
          }
        }


        SetItemText(IDT_LINK_PATH_TO_CUR, s);

        UString destPath = attr.GetPath();
        _pathFromCombo.SetText(FilePath);
        _pathToCombo.SetText(destPath);

        // if (res)
        {
          if (attr.IsMountPoint())
            linkType = IDR_LINK_TYPE_JUNCTION;
          else if (attr.IsSymLink_WSL())
            linkType = IDR_LINK_TYPE_WSL;
          else if (attr.IsSymLink_Win())
          {
            linkType =
              fi.IsDir() ?
                IDR_LINK_TYPE_SYM_DIR :
                IDR_LINK_TYPE_SYM_FILE;
            // if (attr.IsRelative()) linkType = IDR_LINK_TYPE_SYM_RELATIVE;
          }

          if (linkType != 0)
            Set_LinkType_Radio(linkType);
        }
      }
      else
      {
        // no ReparsePoint
        _pathFromCombo.SetText(AnotherPath);
        _pathToCombo.SetText(FilePath);
        if (fi.IsDir())
          linkType = g_SymLink_Supported ?
              IDR_LINK_TYPE_SYM_DIR :
              IDR_LINK_TYPE_JUNCTION;
        else
          linkType = IDR_LINK_TYPE_HARD;
      }
    }
    if (linkType != 0)
      Set_LinkType_Radio(linkType);
  }

  NormalizeSize();
  return CModalDialog::OnInit();
}

bool CLinkDialog::OnSize(WPARAM /* wParam */, int xSize, int ySize)
{
  int mx, my;
  GetMargins(8, mx, my);
  int bx1, bx2, by;
  GetItemSizes(IDCANCEL, bx1, by);
  GetItemSizes(IDB_LINK_LINK, bx2, by);
  int yPos = ySize - my - by;
  int xPos = xSize - mx - bx1;

  InvalidateRect(NULL);

  {
    RECT r, r2;
    GetClientRectOfItem(IDB_LINK_PATH_FROM, r);
    GetClientRectOfItem(IDB_LINK_PATH_TO, r2);
    int bx = RECT_SIZE_X(r);
    int newButtonXpos = xSize - mx - bx;

    MoveItem(IDB_LINK_PATH_FROM, newButtonXpos, r.top, bx, RECT_SIZE_Y(r));
    MoveItem(IDB_LINK_PATH_TO, newButtonXpos, r2.top, bx, RECT_SIZE_Y(r2));

    int newComboXsize = newButtonXpos - mx - mx;
    ChangeSubWindowSizeX(_pathFromCombo, newComboXsize);
    ChangeSubWindowSizeX(_pathToCombo, newComboXsize);
  }

  MoveItem(IDCANCEL, xPos, yPos, bx1, by);
  MoveItem(IDB_LINK_LINK, xPos - mx - bx2, yPos, bx2, by);

  return false;
}

bool CLinkDialog::OnButtonClicked(int buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDB_LINK_PATH_FROM:
      OnButton_SetPath(false);
      return true;
    case IDB_LINK_PATH_TO:
      OnButton_SetPath(true);
      return true;
    case IDB_LINK_LINK:
      OnButton_Link();
      return true;
  }
  return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
}

void CLinkDialog::OnButton_SetPath(bool to)
{
  UString currentPath;
  NWindows::NControl::CComboBox &combo = to ?
    _pathToCombo :
    _pathFromCombo;
  combo.GetText(currentPath);
  // UString title = "Specify a location for output folder";
  UString title = LangString(IDS_SET_FOLDER);

  UString resultPath;
  if (!MyBrowseForFolder(*this, title, currentPath, resultPath))
    return;
  NName::NormalizeDirPathPrefix(resultPath);
  combo.SetCurSel(-1);
  combo.SetText(resultPath);
}

void CLinkDialog::ShowError(const wchar_t *s)
{
  ::MessageBoxW(*this, s, L"NanaZip", MB_ICONERROR);
}

void CLinkDialog::ShowLastErrorMessage()
{
  ShowError(NError::MyFormatMessage(GetLastError()));
}

// **************** NanaZip Modification Start ****************
// Shared link-creation logic used by both the Win32 CLinkDialog and the
// XAML dialog command callback (NANAZIP_MODERN). Returns false and fills
// error with a user-facing message on failure.
static bool CreateLinkShared(
    const UString &from,
    const UString &to,
    int linkTypeId,
    UString &error)
{
  NFind::CFileInfo info1, info2;
  const bool finded1 = info1.Find(us2fs(from));
  const bool finded2 = info2.Find(us2fs(to));

  const bool isDirLink = (
      linkTypeId == IDR_LINK_TYPE_SYM_DIR ||
      linkTypeId == IDR_LINK_TYPE_JUNCTION);

  const bool isWSL = (linkTypeId == IDR_LINK_TYPE_WSL);

  if (!isWSL)
  if ((finded1 && info1.IsDir() != isDirLink) ||
      (finded2 && info2.IsDir() != isDirLink))
  {
    error = L"Incorrect link type";
    return false;
  }

  if (linkTypeId == IDR_LINK_TYPE_HARD)
  {
    if (!NDir::MyCreateHardLink(us2fs(from), us2fs(to)))
    {
      error = NError::MyFormatMessage(GetLastError());
      return false;
    }
  }
  else
  {
    if (finded1 && !info1.IsDir() && !info1.HasReparsePoint() && info1.Size != 0)
    {
      UString s (L"WARNING: reparse point will hide the data of existing file");
      s.Add_LF();
      s += from;
      error = s;
      return false;
    }

    // Backported from 25.00.
    CByteBuffer data;
    const bool isSymLink = (linkTypeId != IDR_LINK_TYPE_JUNCTION);
    FillLinkData(data, to, isSymLink, isWSL);
    if (data.Size() == 0)
    {
      error = L"Incorrect link";
      return false;
    }

    CReparseAttr attr;
    if (!attr.Parse(data, data.Size()))
    {
      error = L"Internal conversion error";
      return false;
    }

    bool res;
    if (to.IsEmpty())
    {
      // res = NIO::SetReparseData(us2fs(from), isDirLink, NULL, 0);
      res = NIO::DeleteReparseData(us2fs(from));
    }
    else
      res = NIO::SetReparseData(us2fs(from), isDirLink, data, (DWORD)data.Size());

    if (!res)
    {
      error = NError::MyFormatMessage(GetLastError());
      return false;
    }
  }
  return true;
}
// **************** NanaZip Modification End ****************

void CLinkDialog::OnButton_Link()
{
  UString from, to;
  _pathFromCombo.GetText(from);
  _pathToCombo.GetText(to);

  if (from.IsEmpty())
    return;
  if (!NName::IsAbsolutePath(from))
    from.Insert(0, CurDirPrefix);

  int idb = -1;
  for (unsigned i = 0;; i++)
  {
    if (i >= ARRAY_SIZE(k_LinkType_Buttons))
      return;
    idb = k_LinkType_Buttons[i];
    if (IsButtonCheckedBool(idb))
      break;
  }

  UString error;
  if (!CreateLinkShared(from, to, idb, error))
  {
    ShowError(error.Ptr());
    return;
  }

  End(IDOK);
}

#ifdef NANAZIP_MODERN
// **************** NanaZip Modification Start ****************
// Truncate-copy into a fixed-size ABI buffer (the same rule as the
// other XAML dialog contexts).
static void CopyTruncated(
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

// Link-creation callback invoked by the XAML LinkPage when the user
// presses the Link button. The business rules stay on the 7-Zip side;
// failure messages are returned so the dialog can show them inline.
static BOOL WINAPI K7LinkCommandCallback(
    _In_ LPCWSTR From,
    _In_ LPCWSTR To,
    _In_ UINT32 LinkType,
    _In_opt_ LPVOID CallbackContext,
    _Out_writes_z_(K7_LINK_MAX_ERROR_LENGTH) LPWSTR ErrorBuffer,
    _In_ UINT32 ErrorBufferSize)
{
  // Map the XAML link-type index (0-4) to the Win32 radio-button IDs.
  static const int kTypeToButtonId[K7_LINK_MAX_TYPE_COUNT] =
  {
    IDR_LINK_TYPE_HARD,
    IDR_LINK_TYPE_SYM_FILE,
    IDR_LINK_TYPE_SYM_DIR,
    IDR_LINK_TYPE_JUNCTION,
    IDR_LINK_TYPE_WSL,
  };
  const int idb = (LinkType < K7_LINK_MAX_TYPE_COUNT) ?
      kTypeToButtonId[LinkType] : IDR_LINK_TYPE_HARD;

  // Resolve a relative source path against the current directory prefix
  // (the same rule as CLinkDialog::OnButton_Link).
  UString from = From ? From : L"";
  if (!from.IsEmpty() && !NName::IsAbsolutePath(from) && CallbackContext)
  {
    PK7_LINK_DIALOG_CONTEXT Context =
        static_cast<PK7_LINK_DIALOG_CONTEXT>(CallbackContext);
    from.Insert(0, Context->CurDirPrefix);
  }

  UString to = To ? To : L"";
  UString error;
  if (CreateLinkShared(from, to, idb, error))
  {
    if (ErrorBuffer && ErrorBufferSize > 0)
    {
      ErrorBuffer[0] = 0;
    }
    return TRUE;
  }

  if (ErrorBuffer && ErrorBufferSize > 0)
  {
    if (error.Len() >= (int)ErrorBufferSize)
    {
      error.DeleteFrom(ErrorBufferSize - 1);
    }
    wcscpy_s(ErrorBuffer, ErrorBufferSize, error.Ptr());
  }
  return FALSE;
}
// **************** NanaZip Modification End ****************
#endif

void CApp::Link()
{
  unsigned srcPanelIndex = GetFocusedPanelIndex();
  CPanel &srcPanel = Panels[srcPanelIndex];
  if (!srcPanel.IsFSFolder())
  {
    srcPanel.MessageBox_Error_UnsupportOperation();
    return;
  }
  CRecordVector<UInt32> indices;
  srcPanel.GetOperatedItemIndices(indices);
  if (indices.IsEmpty())
    return;
  if (indices.Size() != 1)
  {
    srcPanel.MessageBox_Error_LangID(IDS_SELECT_ONE_FILE);
    return;
  }
  int index = indices[0];
  const UString itemName = srcPanel.GetItemName(index);

  const UString fsPrefix = srcPanel.GetFsPath();
  const UString srcPath = fsPrefix + srcPanel.GetItemPrefix(index);
  UString path = srcPath;
  {
    unsigned destPanelIndex = (NumPanels <= 1) ? srcPanelIndex : (1 - srcPanelIndex);
    CPanel &destPanel = Panels[destPanelIndex];
    if (NumPanels > 1)
      if (destPanel.IsFSFolder())
        path = destPanel.GetFsPath();
  }

  // **************** NanaZip Modification Start ****************
  // Backported from 25.00.
  CSelectedState srcSelState;
  srcPanel.SaveSelectedState(srcSelState);
  // **************** NanaZip Modification End ****************

#ifdef NANAZIP_MODERN
  // **************** NanaZip Modification Start ****************
  K7_LINK_DIALOG_CONTEXT Context = {};
  {
    // Dialog font size from the registry (mirrors the ExtractDialog font).
    DWORD pt = 0;
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\NanaZip\\Options", 0,
        KEY_READ, &key) == ERROR_SUCCESS)
    {
      DWORD size = sizeof(pt);
      ::RegQueryValueExW(key, L"FontSizeDialog", nullptr, nullptr,
          reinterpret_cast<LPBYTE>(&pt), &size);
      ::RegCloseKey(key);
    }
    Context.FontSizeDialog = pt;
  }
  {
    // Localized texts (the same IDs the Win32 dialog translates).
    // NanaZip ships without a .lang file, so LangString falls back to
    // empty; use the dialog-template English texts in that case (the
    // same strings the Win32 dialog would show untranslated).
    static const wchar_t *kTypeFallback[K7_LINK_MAX_TYPE_COUNT] =
    {
      L"Hard Link",
      L"File Symbolic Link",
      L"Directory Symbolic Link",
      L"Directory Junction",
      L"WSL",
    };
    static const wchar_t *kFromFallback = L"Link from:";
    static const wchar_t *kToFallback = L"Link to:";
    static const wchar_t *kGroupFallback = L"Link Type";
    static const wchar_t *kLinkFallback = L"Link";

    UString title = LangString(IDD_LINK);
    if (title.IsEmpty())
    {
      title = L"Link";
    }
    CopyTruncated(Context.Title, K7_LINK_TYPE_NAME_LENGTH, title);
    {
      UString fromLabel = LangString(IDT_LINK_PATH_FROM);
      if (fromLabel.IsEmpty())
      {
        fromLabel = kFromFallback;
      }
      CopyTruncated(Context.FromLabel, K7_LINK_TYPE_NAME_LENGTH, fromLabel);
    }
    {
      UString toLabel = LangString(IDT_LINK_PATH_TO);
      if (toLabel.IsEmpty())
      {
        toLabel = kToFallback;
      }
      CopyTruncated(Context.ToLabel, K7_LINK_TYPE_NAME_LENGTH, toLabel);
    }
    {
      UString groupLabel = LangString(IDG_LINK_TYPE);
      if (groupLabel.IsEmpty())
      {
        groupLabel = kGroupFallback;
      }
      CopyTruncated(Context.TypeGroupLabel, K7_LINK_TYPE_NAME_LENGTH, groupLabel);
    }
    {
      UString linkText = LangString(IDB_LINK_LINK);
      if (linkText.IsEmpty())
      {
        linkText = kLinkFallback;
      }
      CopyTruncated(Context.LinkButtonText, K7_LINK_TYPE_NAME_LENGTH, linkText);
    }
    for (UINT32 i = 0; i < K7_LINK_MAX_TYPE_COUNT; i++)
    {
      UString typeName = LangString(k_LinkType_Buttons[i]);
      if (typeName.IsEmpty())
      {
        typeName = kTypeFallback[i];
      }
      CopyTruncated(Context.TypeNames[i], K7_LINK_TYPE_NAME_LENGTH,
          typeName);
    }
    CopyTruncated(Context.CurDirPrefix, K7_LINK_MAX_TEXT_LENGTH, fsPrefix);
  }
  {
    // Initial paths and link type, mirroring CLinkDialog::OnInit.
    const UString filePath = srcPath + itemName;
    NFind::CFileInfo fi;
    int linkType = 0;
    if (!fi.Find(us2fs(filePath)))
    {
      linkType = 1; // file symbolic link
    }
    else if (fi.HasReparsePoint())
    {
      CReparseAttr attr;
      UString error;
      bool res = GetSymLink(us2fs(filePath), attr, error);
      if (!res && error.IsEmpty())
      {
        DWORD lastError = GetLastError();
        if (lastError != 0)
          error = NError::MyFormatMessage(lastError);
      }

      UString s = attr.GetPath();
      if (!attr.IsSymLink_WSL())
      if (!attr.IsOkNamePair())
      {
        s += " : ";
        s += attr.PrintName;
      }

      if (!res)
      {
        s.Insert(0, L"ERROR: ");
        if (!error.IsEmpty())
        {
          s += " : ";
          s += error;
        }
      }

      CopyTruncated(Context.Hint, K7_LINK_MAX_TEXT_LENGTH, s);
      CopyTruncated(Context.From, K7_LINK_MAX_TEXT_LENGTH, filePath);
      CopyTruncated(Context.To, K7_LINK_MAX_TEXT_LENGTH, attr.GetPath());

      if (attr.IsMountPoint())
        linkType = 3; // junction
      else if (attr.IsSymLink_WSL())
        linkType = 4; // WSL
      else if (attr.IsSymLink_Win())
        linkType = fi.IsDir() ? 2 : 1; // dir / file symlink
    }
    else
    {
      CopyTruncated(Context.From, K7_LINK_MAX_TEXT_LENGTH, path);
      CopyTruncated(Context.To, K7_LINK_MAX_TEXT_LENGTH, filePath);
      if (fi.IsDir())
        linkType = g_SymLink_Supported ? 2 : 3;
      else
        linkType = 0; // hard link
    }
    Context.InitialLinkType = linkType;
  }
  Context.CommandCallback = K7LinkCommandCallback;
  Context.CallbackContext = &Context;

  if (::K7ModernShowLinkDialog(srcPanel.GetParent(), &Context) < 0 ||
      !Context.OK)
    return;
  // **************** NanaZip Modification End ****************
#else
  CLinkDialog dlg;
  dlg.CurDirPrefix = fsPrefix;
  dlg.FilePath = srcPath + itemName;
  dlg.AnotherPath = path;

  if (dlg.Create(srcPanel.GetParent()) != IDOK)
    return;
#endif

  // **************** NanaZip Modification Start ****************
  // Backported from 25.00.
  // we refresh srcPanel to show changes in "Link" (kpidNtReparse) column.
  // maybe we should refresh another panel also?
  if (srcPanel._visibleColumns.FindItem_for_PropID(kpidNtReparse) >= 0)
    srcPanel.RefreshListCtrl(srcSelState);
  // **************** NanaZip Modification End ****************

  RefreshTitleAlways();
}
