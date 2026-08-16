// CopyDialog.cpp

#include "StdAfx.h"

#include "../../../Windows/FileName.h"

#include "../../../Windows/Control/Static.h"

#include "CopyDialog.h"

#ifdef LANG
#include "LangUtils.h"
#endif

// **************** NanaZip Modification Start ****************
#include <NanaZip.Modern.h>
// **************** NanaZip Modification End ****************

using namespace NWindows;

// **************** NanaZip Modification Start ****************
INT_PTR CCopyDialog::Create(HWND parentWindow)
{
    K7_COPY_DIALOG_CONTEXT Context = {};

    // Dialog font size from the registry (mirrors the ExtractDialog font).
    {
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
        // Fixed-size ABI buffers: truncate before writing so an over-long
        // string can never trigger wcscpy_s failure (which would clear the
        // buffer silently).
        UString title = Title;
        if (title.Len() >= K7_COPY_MAX_TEXT_LENGTH)
            title.DeleteFrom(K7_COPY_MAX_TEXT_LENGTH - 1);
        UString staticText = Static;
        if (staticText.Len() >= K7_COPY_MAX_TEXT_LENGTH)
            staticText.DeleteFrom(K7_COPY_MAX_TEXT_LENGTH - 1);
        UString value = Value;
        if (value.Len() >= K7_COPY_MAX_TEXT_LENGTH)
            value.DeleteFrom(K7_COPY_MAX_TEXT_LENGTH - 1);
        UString info = Info;
        if (info.Len() >= K7_COPY_MAX_TEXT_LENGTH)
            info.DeleteFrom(K7_COPY_MAX_TEXT_LENGTH - 1);

        wcscpy_s(Context.Title, title.Ptr());
        wcscpy_s(Context.Static, staticText.Ptr());
        wcscpy_s(Context.Value, value.Ptr());
        wcscpy_s(Context.Info, info.Ptr());

        // Drop-down history, bounded by the fixed ABI array.
        Context.HistoryCount = 0;
        FOR_VECTOR (i, Strings)
        {
            if (Context.HistoryCount >= K7_COPY_MAX_HISTORY_ITEMS)
                break;
            UString item = Strings[i];
            if (item.Len() >= K7_COPY_MAX_TEXT_LENGTH)
                item.DeleteFrom(K7_COPY_MAX_TEXT_LENGTH - 1);
            wcscpy_s(Context.History[Context.HistoryCount], item.Ptr());
            Context.HistoryCount++;
        }
    }

    if (::K7ModernShowCopyLocationDialog(parentWindow, &Context) < 0 ||
        !Context.OK)
    {
        return IDCLOSE;
    }

    Value = Context.Value;
    return IDOK;
}

#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
bool CCopyDialog::OnInit()
{
  #ifdef LANG
  LangSetDlgItems(*this, NULL, 0);
  #endif
  _path.Attach(GetItem(IDC_COPY));
  SetText(Title);

  NControl::CStatic staticContol;
  staticContol.Attach(GetItem(IDT_COPY));
  staticContol.SetText(Static);
  #ifdef UNDER_CE
  // we do it, since WinCE selects Value\something instead of Value !!!!
  _path.AddString(Value);
  #endif
  FOR_VECTOR (i, Strings)
    _path.AddString(Strings[i]);
  _path.SetText(Value);
  SetItemText(IDT_COPY_INFO, Info);
  NormalizeSize(true);
  return CModalDialog::OnInit();
}

bool CCopyDialog::OnSize(WPARAM /* wParam */, int xSize, int ySize)
{
  int mx, my;
  GetMargins(8, mx, my);
  int bx1, bx2, by;
  GetItemSizes(IDCANCEL, bx1, by);
  GetItemSizes(IDOK, bx2, by);
  int y = ySize - my - by;
  int x = xSize - mx - bx1;

  InvalidateRect(NULL);

  {
    RECT r;
    GetClientRectOfItem(IDB_COPY_SET_PATH, r);
    int bx = RECT_SIZE_X(r);
    MoveItem(IDB_COPY_SET_PATH, xSize - mx - bx, r.top, bx, RECT_SIZE_Y(r));
    ChangeSubWindowSizeX(_path, xSize - mx - mx - bx - mx);
  }

  {
    RECT r;
    GetClientRectOfItem(IDT_COPY_INFO, r);
    NControl::CStatic staticContol;
    staticContol.Attach(GetItem(IDT_COPY_INFO));
    int yPos = r.top;
    staticContol.Move(mx, yPos, xSize - mx * 2, y - 2 - yPos);
  }

  MoveItem(IDCANCEL, x, y, bx1, by);
  MoveItem(IDOK, x - mx - bx2, y, bx2, by);

  return false;
}

bool CCopyDialog::OnButtonClicked(int buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDB_COPY_SET_PATH:
      OnButtonSetPath();
      return true;
  }
  return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
}

void CCopyDialog::OnButtonSetPath()
{
  UString currentPath;
  _path.GetText(currentPath);

  const UString title = LangString(IDS_SET_FOLDER);

  UString resultPath;
  if (!MyBrowseForFolder(*this, title, currentPath, resultPath))
    return;
  NFile::NName::NormalizeDirPathPrefix(resultPath);
  _path.SetCurSel(-1);
  _path.SetText(resultPath);
}

void CCopyDialog::OnOK()
{
  _path.GetText(Value);
  CModalDialog::OnOK();
}
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
// **************** NanaZip Modification End ****************
