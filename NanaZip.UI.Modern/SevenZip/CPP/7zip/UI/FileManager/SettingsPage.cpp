// SettingsPage.cpp

#include "StdAfx.h"

// #include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/StringToInt.h"

#ifndef UNDER_CE
#include "../../../Windows/MemoryLock.h"
// #include "../../../Windows/System.h"
#endif

// #include "../Common/ZipRegistry.h"

#include "../Common/ZipRegistry.h"
#include "FontUtils.h"

#include "LangUtils.h"
#include "RegistryUtils.h"
#include "App.h"
#include "SettingsPage.h"
#include "SettingsPageRes.h"

using namespace NWindows;

static const UInt32 kLangIDs[] =
{
  IDX_SETTINGS_SHOW_DOTS,
  IDX_SETTINGS_SHOW_REAL_FILE_ICONS,
  IDX_SETTINGS_SHOW_SYSTEM_MENU,
  IDX_SETTINGS_FULL_ROW,
  IDX_SETTINGS_SHOW_GRID,
  IDX_SETTINGS_SINGLE_CLICK,
  IDX_SETTINGS_ALTERNATIVE_SELECTION,
  IDX_SETTINGS_LARGE_PAGES,
  IDX_SETTINGS_WANT_ARC_HISTORY,
  IDX_SETTINGS_WANT_PATH_HISTORY,
  IDX_SETTINGS_WANT_COPY_HISTORY,
  IDX_SETTINGS_WANT_FOLDER_HISTORY,
  IDX_SETTINGS_LOWERCASE_HASHES,
  // **************** SSS Modification Start ****************
  IDT_SETTINGS_FONT_GROUP,
  IDT_SETTINGS_FONT_ADDRESSBAR,
  IDT_SETTINGS_FONT_LIST,
  IDT_SETTINGS_FONT_STATUSBAR,
  IDT_SETTINGS_FONT_DIALOG
  // **************** SSS Modification End ****************
  // , IDT_COMPRESS_MEMORY
};

extern bool IsLargePageSupported();

/*
static void AddMemSize(UString &res, UInt64 size, bool needRound = false)
{
  char c;
  unsigned moveBits = 0;
  if (needRound)
  {
    UInt64 rn = 0;
    if (size >= (1 << 31))
      rn = (1 << 28) - 1;
    UInt32 kRound = (1 << 20) - 1;
    if (rn < kRound)
      rn = kRound;
    size += rn;
    size &= ~rn;
  }
  if (size >= ((UInt64)1 << 31) && (size & 0x3FFFFFFF) == 0)
    { moveBits = 30; c = 'G'; }
  else
    { moveBits = 20; c = 'M'; }
  res.Add_UInt64(size >> moveBits);
  res.Add_Space();
  if (moveBits != 0)
    res += c;
  res += 'B';
}


int CSettingsPage::AddMemComboItem(UInt64 size, UInt64 percents, bool isDefault)
{
  UString sUser;
  UString sRegistry;
  if (size == 0)
  {
    UString s;
    s.Add_UInt64(percents);
    s += '%';
    if (isDefault)
      sUser = "* ";
    else
      sRegistry = s;
    sUser += s;
  }
  else
  {
    AddMemSize(sUser, size);
    sRegistry = sUser;
    for (;;)
    {
      int pos = sRegistry.Find(L' ');
      if (pos < 0)
        break;
      sRegistry.Delete(pos);
    }
    if (!sRegistry.IsEmpty())
      if (sRegistry.Back() == 'B')
        sRegistry.DeleteBack();
  }
  const int index = (int)_memCombo.AddString(sUser);
  _memCombo.SetItemData(index, _memLimitStrings.Size());
  _memLimitStrings.Add(sRegistry);
  return index;
}
*/

bool CSettingsPage::OnInit()
{
  _wasChanged = false;
  _largePages_wasChanged = false;
  // **************** SSS Modification Start ****************
  _fontSizes_wasChanged = false;
  _fontInitGuard = true;
  // **************** SSS Modification End ****************
  /*
  _wasChanged_MemLimit = false;
  _memLimitStrings.Clear();
  _memCombo.Attach(GetItem(IDC_SETTINGS_MEM));
  */

  LangSetDlgItems(*this, kLangIDs, ARRAY_SIZE(kLangIDs));

  CFmSettings st;
  st.Load();

  CheckButton(IDX_SETTINGS_SHOW_DOTS, st.ShowDots);
  CheckButton(IDX_SETTINGS_SHOW_REAL_FILE_ICONS, st.ShowRealFileIcons);
  CheckButton(IDX_SETTINGS_FULL_ROW, st.FullRow);
  CheckButton(IDX_SETTINGS_SHOW_GRID, st.ShowGrid);
  CheckButton(IDX_SETTINGS_SINGLE_CLICK, st.SingleClick);
  CheckButton(IDX_SETTINGS_ALTERNATIVE_SELECTION, st.AlternativeSelection);
  // CheckButton(IDX_SETTINGS_UNDERLINE, st.Underline);

  CheckButton(IDX_SETTINGS_SHOW_SYSTEM_MENU, st.ShowSystemMenu);

  if (IsLargePageSupported())
    CheckButton(IDX_SETTINGS_LARGE_PAGES, ReadLockMemoryEnable());
  else
    EnableItem(IDX_SETTINGS_LARGE_PAGES, false);

  CheckButton(IDX_SETTINGS_WANT_ARC_HISTORY, st.ArcHistory);
  CheckButton(IDX_SETTINGS_WANT_PATH_HISTORY, st.PathHistory);
  CheckButton(IDX_SETTINGS_WANT_COPY_HISTORY, st.CopyHistory);
  CheckButton(IDX_SETTINGS_WANT_FOLDER_HISTORY, st.FolderHistory);
  CheckButton(IDX_SETTINGS_LOWERCASE_HASHES, st.LowercaseHashes);

  // **************** SSS Modification Start ****************
  {
    static const UINT kFontComboIDs[4] =
    {
      IDC_SETTINGS_FONT_ADDRESSBAR,
      IDC_SETTINGS_FONT_LIST,
      IDC_SETTINGS_FONT_STATUSBAR,
      IDC_SETTINGS_FONT_DIALOG
    };

    CFontSizeInfo fs;
    fs.Load();
    const UInt32 kFontValues[4] = { fs.AddressBar, fs.List, fs.StatusBar, fs.Dialog };
    for (unsigned i = 0; i < 4; i++)
    {
      _fontCombo[i].Attach(GetItem(kFontComboIDs[i]));
      InitFontCombo(i, kFontValues[i]);
    }
    // Clear the edit selection so the values do not appear highlighted.
    for (unsigned i = 0; i < 4; i++)
    {
      _fontCombo[i].SendMsg(CB_SETEDITSEL, 0, MAKELPARAM(0, 0));
      COMBOBOXINFO info = { sizeof(info) };
      if (::GetComboBoxInfo(_fontCombo[i], &info) && info.hwndItem)
        ::SendMessageW(info.hwndItem, EM_SETSEL, 0, 0);
    }
    _fontInitGuard = false;

    // Apply the dialog font size to this page.
    ApplyFontToDialog(*this, fs.Dialog);
  }
  // **************** SSS Modification End ****************

  /*
  NCompression::CMemUse mu;
  bool needSetCur = NCompression::MemLimit_Load(mu);
  UInt64 curMemLimit;
  {
    AddMemComboItem(0, 90, true);
    _memCombo.SetCurSel(0);
  }
  if (mu.IsPercent)
  {
    const int index = AddMemComboItem(0, mu.Val);
    _memCombo.SetCurSel(index);
    needSetCur = false;
  }
  {
    _ramSize = (UInt64)(sizeof(size_t)) << 29;
    _ramSize_Defined = NSystem::GetRamSize(_ramSize);
    UString s;
    if (_ramSize_Defined)
    {
      s += "/ ";
      AddMemSize(s, _ramSize, true);
    }
    SetItemText(IDT_SETTINGS_MEM_RAM, s);

    curMemLimit = mu.GetBytes(_ramSize);

    // size = 100 << 20; // for debug only;
    for (unsigned i = (27) * 2;; i++)
    {
      UInt64 size = (UInt64)(2 + (i & 1)) << (i / 2);
      if (i > (20 + sizeof(size_t) * 3 * 1 - 1) * 2)
        size = (UInt64)(Int64)-1;
      if (needSetCur && (size >= curMemLimit))
      {
        const int index = AddMemComboItem(curMemLimit);
        _memCombo.SetCurSel(index);
        needSetCur = false;
        if (size == curMemLimit)
          continue;
      }
      if (size == (UInt64)(Int64)-1)
        break;
      AddMemComboItem(size);
    }
  }
  */

  // EnableSubItems();

  return CPropertyPage::OnInit();
}

// **************** SSS Modification Start ****************
static UInt32 ParseFontPt(NWindows::NControl::CComboBox &combo)
{
  UString s;
  combo.GetText(s);
  s.Trim();
  if (s.IsEmpty())
    return 0;

  const UString defaultStr = LangString(IDT_SETTINGS_FONT_DEFAULT);
  if (!defaultStr.IsEmpty() && s.IsEqualTo_NoCase(defaultStr))
    return 0;
  if (s.IsEqualTo_NoCase(L"Default"))
    return 0;

  const wchar_t *end;
  const UInt64 v = ConvertStringToUInt64(s, &end);
  if (end && *end == 0 && v >= 1 && v <= 100)
    return (UInt32)v;
  return 0;
}
// **************** SSS Modification End ****************

LONG CSettingsPage::OnApply()
{
  if (_wasChanged)
  {
    CFmSettings st;
    st.ShowDots = IsButtonCheckedBool(IDX_SETTINGS_SHOW_DOTS);
    st.ShowRealFileIcons = IsButtonCheckedBool(IDX_SETTINGS_SHOW_REAL_FILE_ICONS);
    st.FullRow = IsButtonCheckedBool(IDX_SETTINGS_FULL_ROW);
    st.ShowGrid = IsButtonCheckedBool(IDX_SETTINGS_SHOW_GRID);
    st.SingleClick = IsButtonCheckedBool(IDX_SETTINGS_SINGLE_CLICK);
    st.AlternativeSelection = IsButtonCheckedBool(IDX_SETTINGS_ALTERNATIVE_SELECTION);
    st.ArcHistory = IsButtonCheckedBool(IDX_SETTINGS_WANT_ARC_HISTORY);
    st.PathHistory = IsButtonCheckedBool(IDX_SETTINGS_WANT_PATH_HISTORY);
    st.CopyHistory = IsButtonCheckedBool(IDX_SETTINGS_WANT_COPY_HISTORY);
    st.FolderHistory = IsButtonCheckedBool(IDX_SETTINGS_WANT_FOLDER_HISTORY);
    st.LowercaseHashes = IsButtonCheckedBool(IDX_SETTINGS_LOWERCASE_HASHES);
    // st.Underline = IsButtonCheckedBool(IDX_SETTINGS_UNDERLINE);

    st.ShowSystemMenu = IsButtonCheckedBool(IDX_SETTINGS_SHOW_SYSTEM_MENU);

    st.Save();
    _wasChanged = false;
  }

  // **************** SSS Modification Start ****************
  if (_fontSizes_wasChanged)
  {
    CFontSizeInfo fs;
    fs.AddressBar = ParseFontPt(_fontCombo[0]);
    fs.List = ParseFontPt(_fontCombo[1]);
    fs.StatusBar = ParseFontPt(_fontCombo[2]);
    fs.Dialog = ParseFontPt(_fontCombo[3]);
    fs.Save();
    _fontSizes_wasChanged = false;

    // Apply the dialog font size to this page with a compact re-layout
    // (the parent property sheet is adjusted automatically).
    HWND propertySheet = CPropertyPage::GetParent();
    ApplyFontToDialogCompact(*this, fs.Dialog, true);

    // The main window owns the panels. Ask it to re-read the saved settings
    // and apply them to the live controls.
    HWND owner = propertySheet ? ::GetParent(propertySheet) : nullptr;
    if (!owner)
      owner = g_HWND;
    if (owner)
      ::PostMessageW(owner, kApplyFontSettingsMessage, 0, 0);
  }
  // **************** SSS Modification End ****************

  #ifndef UNDER_CE
  if (_largePages_wasChanged)
  {
    if (IsLargePageSupported())
    {
      bool enable = IsButtonCheckedBool(IDX_SETTINGS_LARGE_PAGES);
      NSecurity::EnablePrivilege_LockMemory(enable);
      SaveLockMemoryEnable(enable);
    }
    _largePages_wasChanged = false;
  }
  #endif

  /*
  if (_wasChanged_MemLimit)
  {
    const unsigned index = (int)_memCombo.GetItemData_of_CurSel();
    const UString str = _memLimitStrings[index];

    bool needSave = true;

    NCompression::CMemUse mu;

    if (_ramSize_Defined)
      mu.Parse(str);
    if (mu.IsDefined)
    {
      const UInt64 usage64 = mu.GetBytes(_ramSize);
      if (_ramSize <= usage64)
      {
        UString s2 = LangString(IDT_COMPRESS_MEMORY);
        if (s2.IsEmpty())
          GetItemText(IDT_COMPRESS_MEMORY, s2);
        UString s;

        s += "The selected value is not safe for system performance.";
        s.Add_LF();
        s += "The memory consumption for compression operation will exceed RAM size.";
        s.Add_LF();
        s.Add_LF();
        AddSize_MB(s, usage64);

        if (!s2.IsEmpty())
        {
          s += " : ";
          s += s2;
        }

        s.Add_LF();
        AddSize_MB(s, _ramSize);
        s += " : RAM";

        s.Add_LF();
        s.Add_LF();
        s += "Are you sure you want set that unsafe value for memory usage?";

        int res = MessageBoxW(*this, s, L"NanaZip", MB_YESNOCANCEL | MB_ICONQUESTION);
        if (res != IDYES)
          needSave = false;
      }
    }

    if (needSave)
    {
      NCompression::MemLimit_Save(str);
      _wasChanged_MemLimit = false;
    }
    else
      return PSNRET_INVALID_NOCHANGEPAGE;
  }
  */

  return PSNRET_NOERROR;
}

/*
bool CSettingsPage::OnCommand(int code, int itemID, LPARAM param)
{
  if (code == CBN_SELCHANGE)
  {
    switch (itemID)
    {
      case IDC_SETTINGS_MEM:
      {
        _wasChanged_MemLimit = true;
        Changed();
        break;
      }
    }
  }
  return CPropertyPage::OnCommand(code, itemID, param);
}
*/

bool CSettingsPage::OnCommand(int code, int itemID, LPARAM param)
{
  if (_fontInitGuard)
    return CPropertyPage::OnCommand(code, itemID, param);
  if (code == CBN_SELCHANGE || code == CBN_EDITCHANGE)
  {
    switch (itemID)
    {
      case IDC_SETTINGS_FONT_ADDRESSBAR:
      case IDC_SETTINGS_FONT_LIST:
      case IDC_SETTINGS_FONT_STATUSBAR:
      case IDC_SETTINGS_FONT_DIALOG:
        _fontSizes_wasChanged = true;
        Changed();
        break;
    }
  }
  // **************** SSS Modification Start ****************
  // When a combo box gains focus, Windows auto-selects its whole edit text
  // (shown as a blue selection). Clear the selection right away so the
  // values never appear highlighted. Also clear after picking an item.
  if (code == CBN_SETFOCUS || code == CBN_SELCHANGE)
  {
    const HWND itemHwnd = ::GetDlgItem(*this, itemID);
    if (itemHwnd)
    {
      for (unsigned i = 0; i < 4; i++)
      {
        if ((HWND)_fontCombo[i] == itemHwnd)
        {
          _fontCombo[i].SendMsg(CB_SETEDITSEL, 0, MAKELPARAM(0, 0));
          COMBOBOXINFO info = { sizeof(info) };
          if (::GetComboBoxInfo(_fontCombo[i], &info) && info.hwndItem)
            ::SendMessageW(info.hwndItem, EM_SETSEL, 0, 0);
          break;
        }
      }
    }
  }
  // **************** SSS Modification End ****************
  return CPropertyPage::OnCommand(code, itemID, param);
}

// **************** SSS Modification Start ****************
static UInt32 GetDefaultUiFontPt(HWND hwnd)
{
  HFONT font = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
  LOGFONTW lf = {};
  if (::GetObjectW(font, sizeof(lf), &lf) == 0 || lf.lfHeight >= 0)
    return 0;
  UINT dpi = hwnd ? ::GetDpiForWindow(hwnd) : USER_DEFAULT_SCREEN_DPI;
  return (UInt32)(((double)-lf.lfHeight * 72.0 / dpi) + 0.5);
}

void CSettingsPage::InitFontCombo(unsigned index, UInt32 pt)
{
  NWindows::NControl::CComboBox &combo = _fontCombo[index];

  combo.ResetContent();

  static const UInt32 kValues[] = { 8, 9, 10, 11, 12, 14, 16, 18, 20, 24 };
  for (unsigned i = 0; i < ARRAY_SIZE(kValues); i++)
  {
    UString s;
    s.Add_UInt32(kValues[i]);
    combo.AddString(s);
  }

  // Show the effective font size: if the setting is 0 (follow system),
  // show the actual system UI size instead of a vague "Default" label.
  if (pt == 0)
    pt = GetDefaultUiFontPt(*this);

  for (unsigned i = 0; i < ARRAY_SIZE(kValues); i++)
  {
    if (kValues[i] == pt)
    {
      combo.SetCurSel((int)i);
      return;
    }
  }

  UString s;
  s.Add_UInt32(pt);
  combo.SetText(s);
}
// **************** SSS Modification End ****************

bool CSettingsPage::OnButtonClicked(int buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDX_SETTINGS_SINGLE_CLICK:
    /*
      EnableSubItems();
      break;
    */
    case IDX_SETTINGS_SHOW_DOTS:
    case IDX_SETTINGS_SHOW_SYSTEM_MENU:
    case IDX_SETTINGS_SHOW_REAL_FILE_ICONS:
    case IDX_SETTINGS_FULL_ROW:
    case IDX_SETTINGS_SHOW_GRID:
    case IDX_SETTINGS_ALTERNATIVE_SELECTION:
    case IDX_SETTINGS_WANT_ARC_HISTORY:
    case IDX_SETTINGS_WANT_PATH_HISTORY:
    case IDX_SETTINGS_WANT_COPY_HISTORY:
    case IDX_SETTINGS_WANT_FOLDER_HISTORY:
    case IDX_SETTINGS_LOWERCASE_HASHES:
      _wasChanged = true;
      break;

    case IDX_SETTINGS_LARGE_PAGES:
      _largePages_wasChanged = true;
      break;

    default:
      return CPropertyPage::OnButtonClicked(buttonID, buttonHWND);
  }

  Changed();
  return true;
}
