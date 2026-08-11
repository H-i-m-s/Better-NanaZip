// OptionsDialog.cpp

#include "StdAfx.h"

#include "../../../Windows/Control/Dialog.h"
#include "../../../Windows/Control/PropertyPage.h"
#include "../../../Windows/MemoryLock.h"
#include "../../../Common/StringConvert.h"

#include "DialogSize.h"

#include "EditPage.h"
#include "EditPageRes.h"
#include "FoldersPage.h"
#include "FoldersPageRes.h"
#include "MenuPage.h"
#include "MenuPageRes.h"
#include "SettingsPage.h"
#include "SettingsPageRes.h"
// **************** SSS Modification Start ****************
#include "ExtractSettingsPage.h"
// **************** SSS Modification End ****************

#include "../Common/ZipRegistry.h"
#include "../Explorer/ContextMenuFlags.h"
#include "../Explorer/resource.h"
#include "../GUI/ExtractDialogRes.h"
#include "PropertyNameRes.h"
#include "FormatUtils.h"
#include "RegistryUtils.h"
#include "SssPasswordFile.h"
#include "FontUtils.h"

#include <NanaZip.Modern.h>

#include "App.h"
#include "LangUtils.h"
#include "MyLoadMenu.h"

#include "resource.h"

using namespace NWindows;
using namespace NContextMenuFlags;

// ============================================================
// XAML settings dialog support (SSS modification)
//
// The options dialog is shown with the XAML settings dialog when
// NanaZip.Modern is available. All values are exchanged through a
// K7_SETTINGS_DIALOG_CONTEXT snapshot so that the Win32 side keeps
// the full ownership of the registry access and application logic.
// The legacy property sheet path is kept as a fallback.
// ============================================================

extern bool IsLargePageSupported();

struct CContextMenuItem
{
  int ControlID;
  UInt32 Flag;
};

// Position/size of the XAML options dialog, persisted across runs so that
// the dialog reopens at the same place and size.
static const wchar_t *kOptionsDialogRegKey = L"Software\\NanaZip\\OptionsDialog";

static void LoadOptionsDialogRect(RECT &rc)
{
  rc.left = rc.top = rc.right = rc.bottom = 0;

  HKEY key = nullptr;
  if (::RegOpenKeyExW(HKEY_CURRENT_USER, kOptionsDialogRegKey, 0,
      KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
    return;

  DWORD x = 0, y = 0, w = 0, h = 0;
  DWORD size = sizeof(DWORD);
  DWORD type = REG_DWORD;
  ::RegQueryValueExW(key, L"X", nullptr, &type,
      (LPBYTE)&x, &size);
  ::RegQueryValueExW(key, L"Y", nullptr, &type,
      (LPBYTE)&y, &size);
  ::RegQueryValueExW(key, L"W", nullptr, &type,
      (LPBYTE)&w, &size);
  ::RegQueryValueExW(key, L"H", nullptr, &type,
      (LPBYTE)&h, &size);
  ::RegCloseKey(key);

  if (w == 0 || h == 0)
    return;

  RECT candidate = { (LONG)x, (LONG)y, (LONG)(x + w), (LONG)(y + h) };
  // If the remembered position is outside every display (for example the
  // monitor was disconnected), fall back to the centered default.
  if (!::MonitorFromRect(&candidate, MONITOR_DEFAULTTONULL))
    return;

  rc = candidate;
}

static void SaveOptionsDialogRect(const RECT &rc)
{
  if (rc.right <= rc.left || rc.bottom <= rc.top)
    return;

  HKEY key = nullptr;
  DWORD disp = 0;
  if (::RegCreateKeyExW(HKEY_CURRENT_USER, kOptionsDialogRegKey, 0,
      nullptr, 0, KEY_SET_VALUE, nullptr, &key, &disp) != ERROR_SUCCESS)
    return;

  DWORD x = (DWORD)rc.left;
  DWORD y = (DWORD)rc.top;
  DWORD w = (DWORD)(rc.right - rc.left);
  DWORD h = (DWORD)(rc.bottom - rc.top);
  ::RegSetValueExW(key, L"X", 0, REG_DWORD, (const BYTE *)&x, sizeof(x));
  ::RegSetValueExW(key, L"Y", 0, REG_DWORD, (const BYTE *)&y, sizeof(y));
  ::RegSetValueExW(key, L"W", 0, REG_DWORD, (const BYTE *)&w, sizeof(w));
  ::RegSetValueExW(key, L"H", 0, REG_DWORD, (const BYTE *)&h, sizeof(h));
  ::RegCloseKey(key);
}

// The "Apply" button saves the settings immediately without closing the
// dialog. It runs on the UI thread while the dialog is open.
static void SaveSettingsDialogContext(
    const K7_SETTINGS_DIALOG_CONTEXT &ctx,
    HWND hwndOwner);

struct CApplySettingsContext
{
  K7_SETTINGS_DIALOG_CONTEXT *Ctx;
  HWND Owner;
};

static void WINAPI ApplySettingsCallback(LPVOID param)
{
  CApplySettingsContext *applyContext =
      (CApplySettingsContext *)param;
  if (!applyContext || !applyContext->Ctx)
    return;
  SaveSettingsDialogContext(*applyContext->Ctx, applyContext->Owner);
  g_App.SetListSettings();
  g_App.RefreshAllPanels();
}

static const CContextMenuItem kMenuItems[] =
{
  { IDS_CONTEXT_OPEN, kOpen },
  { IDS_CONTEXT_TEST, kTest },

  { IDS_CONTEXT_EXTRACT, kExtract },
  { IDS_CONTEXT_EXTRACT_HERE, kExtractHere },
  { IDS_CONTEXT_EXTRACT_HERE_SMART, kExtractHereSmart },
  { IDS_CONTEXT_EXTRACT_TO, kExtractTo },

  { IDS_CONTEXT_COMPRESS, kCompress },
  { IDS_CONTEXT_COMPRESS_TO, kCompressTo7z },
  { IDS_CONTEXT_COMPRESS_TO, kCompressToZip },

  #ifndef UNDER_CE
  { IDS_CONTEXT_COMPRESS_EMAIL, kCompressEmail },
  { IDS_CONTEXT_COMPRESS_TO_EMAIL, kCompressTo7zEmail },
  { IDS_CONTEXT_COMPRESS_TO_EMAIL, kCompressToZipEmail },
  #endif

  { IDS_PROP_CHECKSUM, kCRC }
};

static void LoadLang_Spec(UString &s, UInt32 id, const char *eng)
{
  LangString(id, s);
  if (s.IsEmpty())
    s = eng;
  s.RemoveChar(L'&');
}

static UInt32 GetDefaultUiFontPt(HWND hwnd)
{
  HFONT font = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
  LOGFONTW lf = {};
  if (::GetObjectW(font, sizeof(lf), &lf) == 0 || lf.lfHeight >= 0)
    return 0;
  UINT dpi = hwnd ? ::GetDpiForWindow(hwnd) : USER_DEFAULT_SCREEN_DPI;
  return (UInt32)(((double)-lf.lfHeight * 72.0 / dpi) + 0.5);
}

static void FillSettingsDialogContext(
    K7_SETTINGS_DIALOG_CONTEXT &ctx,
    HWND hwndOwner)
{
  ctx.OK = FALSE;
  ctx.DirtyApi = FALSE;

  // --- Settings page ---
  CFmSettings st;
  st.Load();
  ctx.ShowDots = st.ShowDots;
  ctx.ShowRealFileIcons = st.ShowRealFileIcons;
  ctx.FullRow = st.FullRow;
  ctx.ShowGrid = st.ShowGrid;
  ctx.SingleClick = st.SingleClick;
  ctx.AlternativeSelection = st.AlternativeSelection;
  ctx.ShowSystemMenu = st.ShowSystemMenu;
  ctx.ArcHistory = st.ArcHistory;
  ctx.PathHistory = st.PathHistory;
  ctx.CopyHistory = st.CopyHistory;
  ctx.FolderHistory = st.FolderHistory;
  ctx.LowercaseHashes = st.LowercaseHashes;
  ctx.SizeFormat = st.SizeFormat;

  ctx.LargePagesSupported = IsLargePageSupported();
  ctx.LargePages = ctx.LargePagesSupported ? ReadLockMemoryEnable() : FALSE;

  CFontSizeInfo fs;
  fs.Load();
  ctx.FontSizeAddressBar = fs.AddressBar;
  ctx.FontSizeList = fs.List;
  ctx.FontSizeStatusBar = fs.StatusBar;
  ctx.FontSizeDialog = fs.Dialog;
  ctx.DefaultUiFontPt = GetDefaultUiFontPt(hwndOwner);

  // --- Integration (menu) page ---
  CContextMenuInfo ci;
  ci.Load();
  ctx.ElimDup = ci.ElimDup.Val;
  ctx.ExtractOnOpen = ci.ExtractOnOpen.Val;

  {
    unsigned wz = ci.WriteZone;
    if (wz == (UInt32)(Int32)-1)
      wz = NExtract::NZoneIdMode::Default;
    unsigned zoneSel = 0;
    for (unsigned i = 0; i <= 3; i++)
    {
      UString s;
      unsigned val = i;
      if (i == 3)
      {
        if (wz < 3)
          break;
        val = wz;
      }
      else
      {
        #define MY_IDYES  406
        #define MY_IDNO   407
        if (i == 0)
          LoadLang_Spec(s, MY_IDNO, "No");
        else if (i == 1)
          LoadLang_Spec(s, MY_IDYES, "Yes");
        else
          LangString(IDT_ZONE_FOR_OFFICE, s);
      }
      if (s.IsEmpty())
        s.Add_UInt32(val);
      if (i == NExtract::NZoneIdMode::Default)
        s.Insert(0, L"* ");
      wcsncpy_s(ctx.ZoneItems[i], s, _TRUNCATE);
      if (val == wz)
        zoneSel = i;
    }
    ctx.ZoneSel = zoneSel;
    ctx.WriteZone = zoneSel;
  }

  for (unsigned i = 0; i < ARRAY_SIZE(kMenuItems); i++)
  {
    const CContextMenuItem &menuItem = kMenuItems[i];
    ctx.ContextFlags[i] = ((ci.Flags & menuItem.Flag) != 0);

    UString s = LangString(menuItem.ControlID);
    if (menuItem.Flag == kCRC)
      s = "HASH";

    switch (menuItem.ControlID)
    {
      case IDS_CONTEXT_EXTRACT_TO:
      {
        s = MyFormatNew(s, LangString(IDS_CONTEXT_FOLDER));
        break;
      }
      case IDS_CONTEXT_COMPRESS_TO:
      case IDS_CONTEXT_COMPRESS_TO_EMAIL:
      {
        UString s2 = LangString(IDS_CONTEXT_ARCHIVE);
        switch (menuItem.Flag)
        {
          case kCompressTo7z:
          case kCompressTo7zEmail:
            s2 += (".7z");
            break;
          case kCompressToZip:
          case kCompressToZipEmail:
            s2 += (".zip");
            break;
        }
        s = MyFormatNew(s, s2);
        break;
      }
    }
    wcsncpy_s(ctx.ContextNames[i], s, _TRUNCATE);
  }

  // --- Folders page ---
  NWorkDir::CInfo wd;
  wd.Load();
  ctx.WorkMode = wd.Mode;
  ctx.ForRemovableOnly = wd.ForRemovableOnly;
  wcsncpy_s(ctx.WorkPath, fs2us(wd.Path), _TRUNCATE);

  // --- Editor page ---
  {
    UString path;
    ReadRegEditor(false, path);
    wcsncpy_s(ctx.EditorPaths[0], path, _TRUNCATE);
    ReadRegEditor(true, path);
    wcsncpy_s(ctx.EditorPaths[1], path, _TRUNCATE);
    ReadRegDiff(path);
    wcsncpy_s(ctx.EditorPaths[2], path, _TRUNCATE);
  }

  // --- Extract settings page ---
  ctx.DeleteAfterExtract = st.DeleteAfterExtract;
  ctx.DeletePermanently = st.DeletePermanently;
  ctx.AutoQueryCloud = st.AutoQueryCloud;
  ctx.AutoMatchLocal = st.AutoMatchLocal;
  ctx.AutoShowPassword = st.AutoShowPassword;
  ctx.MatchPriority = st.MatchPriority;

  {
    SssPasswordBook book;
    UString text;
    if (SssLoadPasswordBook(book))
    {
      for (unsigned i = 0; i < book.lines.Size(); i++)
      {
        if (i != 0)
          text += L"\r\n";
        text += book.lines[i];
      }
    }
    wcsncpy_s(ctx.PasswordBook, text, _TRUNCATE);
  }

  {
    SssApiConfig cfg;
    SssLoadApiConfig(cfg);
    wcsncpy_s(ctx.ApiUrl, cfg.Url, _TRUNCATE);
    wcsncpy_s(ctx.ApiAppId, cfg.AppId, _TRUNCATE);
    wcsncpy_s(ctx.ApiAesKey, cfg.AesKey, _TRUNCATE);
    wcsncpy_s(ctx.ApiSigningKey, cfg.SigningKey, _TRUNCATE);
    wcsncpy_s(ctx.ApiPackageName, cfg.PackageName, _TRUNCATE);
    wcsncpy_s(ctx.ApiFingerprint, cfg.Fingerprint, _TRUNCATE);
  }
}

static void SaveSettingsDialogContext(
    const K7_SETTINGS_DIALOG_CONTEXT &ctx,
    HWND hwndOwner)
{
  // --- Settings page ---
  CFmSettings st;
  st.Load();
  st.ShowDots = ctx.ShowDots != FALSE;
  st.ShowRealFileIcons = ctx.ShowRealFileIcons != FALSE;
  st.FullRow = ctx.FullRow != FALSE;
  st.ShowGrid = ctx.ShowGrid != FALSE;
  st.SingleClick = ctx.SingleClick != FALSE;
  st.AlternativeSelection = ctx.AlternativeSelection != FALSE;
  st.ShowSystemMenu = ctx.ShowSystemMenu != FALSE;
  st.ArcHistory = ctx.ArcHistory != FALSE;
  st.PathHistory = ctx.PathHistory != FALSE;
  st.CopyHistory = ctx.CopyHistory != FALSE;
  st.FolderHistory = ctx.FolderHistory != FALSE;
  st.LowercaseHashes = ctx.LowercaseHashes != FALSE;
  st.SizeFormat = ctx.SizeFormat != FALSE;
  st.DeleteAfterExtract = ctx.DeleteAfterExtract != FALSE;
  st.DeletePermanently = ctx.DeletePermanently != FALSE;
  st.AutoQueryCloud = ctx.AutoQueryCloud != FALSE;
  st.AutoMatchLocal = ctx.AutoMatchLocal != FALSE;
  st.AutoShowPassword = ctx.AutoShowPassword != FALSE;
  st.MatchPriority = ctx.MatchPriority;
  st.Save();

  // --- Font sizes ---
  CFontSizeInfo fs;
  fs.Load();
  fs.AddressBar = ctx.FontSizeAddressBar;
  fs.List = ctx.FontSizeList;
  fs.StatusBar = ctx.FontSizeStatusBar;
  fs.Dialog = ctx.FontSizeDialog;
  fs.Save();

  // Ask the main window to re-apply the font settings to the live controls.
  if (hwndOwner)
    ::PostMessageW(hwndOwner, kApplyFontSettingsMessage, 0, 0);

  #ifndef UNDER_CE
  if (ctx.LargePagesSupported)
  {
    NSecurity::EnablePrivilege_LockMemory(ctx.LargePages != FALSE);
    SaveLockMemoryEnable(ctx.LargePages != FALSE);
  }
  #endif

  // --- Integration (menu) page ---
  CContextMenuInfo ci;
  ci.Load();
  ci.ElimDup.Val = ctx.ElimDup != FALSE;
  ci.ElimDup.Def = true;
  ci.ExtractOnOpen.Val = ctx.ExtractOnOpen != FALSE;
  ci.ExtractOnOpen.Def = true;

  {
    UInt32 zoneIndex = ctx.WriteZone;
    if (zoneIndex > NExtract::NZoneIdMode::kOffice ||
        zoneIndex == NExtract::NZoneIdMode::Default)
      zoneIndex = (UInt32)(Int32)-1;
    ci.WriteZone = zoneIndex;
  }

  ci.Flags = 0;
  for (unsigned i = 0; i < ARRAY_SIZE(kMenuItems); i++)
    if (ctx.ContextFlags[i])
      ci.Flags |= kMenuItems[i].Flag;
  ci.Flags_Def = true;
  ci.Save();

  // --- Folders page ---
  NWorkDir::CInfo wd;
  wd.Load();
  wd.Mode = NWorkDir::NMode::EEnum(ctx.WorkMode);
  wd.ForRemovableOnly = ctx.ForRemovableOnly != FALSE;
  wd.Path = us2fs(ctx.WorkPath);
  wd.Save();

  // --- Editor page ---
  SaveRegEditor(false, ctx.EditorPaths[0]);
  SaveRegEditor(true, ctx.EditorPaths[1]);
  SaveRegDiff(ctx.EditorPaths[2]);

  // --- Extract settings page ---
  {
    UString text = ctx.PasswordBook;
    UStringVector lines;
    SssSplitTextToLines(text, lines);
    UStringVector clean;
    for (unsigned i = 0; i < lines.Size(); i++)
      if (!lines[i].IsEmpty())
        clean.Add(lines[i]);
    SssSavePasswordBook(clean);
  }

  if (ctx.DirtyApi)
  {
    SssApiConfig cfg;
    cfg.Url = ctx.ApiUrl;
    cfg.AppId = ctx.ApiAppId;
    cfg.AesKey = ctx.ApiAesKey;
    cfg.SigningKey = ctx.ApiSigningKey;
    cfg.PackageName = ctx.ApiPackageName;
    cfg.Fingerprint = ctx.ApiFingerprint;
    SssSaveApiConfig(cfg);
  }
}

void OptionsDialog(HWND hwndOwner, HINSTANCE /* hInstance */)
{
  if (::K7ModernAvailable())
  {
    K7_SETTINGS_DIALOG_CONTEXT ctx = {};
    FillSettingsDialogContext(ctx, hwndOwner);

    // Restore the remembered dialog position and size (if any).
    LoadOptionsDialogRect(ctx.WindowRect);

    // "Apply" button support: save immediately without closing.
    CApplySettingsContext applyContext = { &ctx, hwndOwner };
    ctx.ApplyCallback = ApplySettingsCallback;
    ctx.ApplyContext = &applyContext;

    ::K7ModernShowSettingsDialog(hwndOwner, &ctx);

    // Remember the final position and size for the next open.
    SaveOptionsDialogRect(ctx.WindowRect);

    if (ctx.OK)
    {
      SaveSettingsDialogContext(ctx, hwndOwner);
      g_App.SetListSettings();
      g_App.RefreshAllPanels();
    }
    return;
  }

  // Legacy property sheet path (fallback when NanaZip.Modern is not
  // available).
  CMenuPage menuPage;
  CFoldersPage foldersPage;
  CEditPage editPage;
  CSettingsPage settingsPage;
  // **************** SSS Modification Start ****************
  CExtractSettingsPage extractSettingsPage;
  // **************** SSS Modification End ****************

  CObjectVector<NControl::CPageInfo> pages;
  BIG_DIALOG_SIZE(200, 200);

  const UINT pageIDs[] = {
      SIZED_DIALOG(IDD_MENU),
      SIZED_DIALOG(IDD_FOLDERS),
      SIZED_DIALOG(IDD_EDIT),
      SIZED_DIALOG(IDD_SETTINGS),
      // **************** SSS Modification Start ****************
      SIZED_DIALOG(IDD_SETTINGS_EXTRACT)
      // **************** SSS Modification End ****************
  };

  NControl::CPropertyPage *pagePointers[] = { &menuPage, &foldersPage, &editPage, &settingsPage,
      // **************** SSS Modification Start ****************
      &extractSettingsPage
      // **************** SSS Modification End ****************
  };

  for (unsigned i = 0; i < ARRAY_SIZE(pageIDs); i++)
  {
    NControl::CPageInfo &page = pages.AddNew();
    page.ID = pageIDs[i];
    LangString_OnlyFromLangFile(page.ID, page.Title);
    page.Page = pagePointers[i];
  }

  INT_PTR res = NControl::MyPropertySheet(pages, hwndOwner, LangString(IDS_OPTIONS));

  if (res != -1 && res != 0)
  {
    g_App.SetListSettings();
    g_App.RefreshAllPanels();
    // ::PostMessage(hwndOwner, kLangWasChangedMessage, 0 , 0);
  }
}
