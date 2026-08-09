// Windows/Control/PropertyPage.cpp

#include "StdAfx.h"

#ifndef _UNICODE
#include "../../Common/StringConvert.h"
#endif

#include "PropertyPage.h"

extern HINSTANCE g_hInstance;
#ifndef _UNICODE
extern bool g_IsNT;
#endif

namespace NWindows {
namespace NControl {


// **************** SSS Modification Start ****************
// Property-page font support: reads the registered dialog font size, applies
// the font and recomputes a compact layout (row heights only, horizontal
// positions untouched) so the page adapts without stretching the gaps. The
// parent sheet (tab control, buttons, window height) follows the page
// height. The original template layout is captured once and remembered in
// the SSS_OrigLayout property, so repeated calls are idempotent.

static HFONT CreateSssDialogFont(unsigned pt, unsigned dpi)
{
  LOGFONTW lf = {};
  lf.lfHeight = -::MulDiv((int)pt, (int)dpi, 72);
  lf.lfCharSet = DEFAULT_CHARSET;
  lf.lfQuality = CLEARTYPE_QUALITY;

  NONCLIENTMETRICSW ncm = {};
  ncm.cbSize = sizeof(ncm);
  if (::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0) &&
      ncm.lfMessageFont.lfFaceName[0])
    wcscpy_s(lf.lfFaceName, ncm.lfMessageFont.lfFaceName);
  else
    wcscpy_s(lf.lfFaceName, L"Segoe UI");

  return ::CreateFontIndirectW(&lf);
}

static BOOL CALLBACK SssSetChildFontProc(HWND hwnd, LPARAM lParam)
{
  ::SendMessageW(hwnd, WM_SETFONT, (WPARAM)lParam, TRUE);
  return TRUE;
}

static void SssApplyFontToTree(HWND hwnd, unsigned pt)
{
  HDC dc = ::GetDC(hwnd);
  const UINT dpi = dc ? static_cast<UINT>(::GetDeviceCaps(dc, LOGPIXELSY)) : 96;
  if (dc)
    ::ReleaseDC(hwnd, dc);
  HFONT font = CreateSssDialogFont(pt, dpi);
  if (!font)
    return;

  ::SendMessageW(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
  ::EnumChildWindows(hwnd, SssSetChildFontProc, (LPARAM)font);
}

struct SssLayoutItem
{
  HWND Hwnd;
  int X, Y, Width, Height;
};

struct SssLayoutHeader
{
  UINT Count;
  SssLayoutItem Items[1]; // actually Count entries
};

struct SssCollectContext
{
  HWND Parent;
  CRecordVector<SssLayoutItem> Items;
};

static BOOL CALLBACK SssCollectProc(HWND hwnd, LPARAM lParam)
{
  SssCollectContext *ctx = (SssCollectContext *)lParam;
  RECT r = {};
  if (::GetWindowRect(hwnd, &r))
  {
    POINT p = { r.left, r.top };
    ::ScreenToClient(ctx->Parent, &p);
    SssLayoutItem item;
    item.Hwnd = hwnd;
    item.X = p.x;
    item.Y = p.y;
    item.Width = r.right - r.left;
    item.Height = r.bottom - r.top;
    ctx->Items.Add(item);
  }
  return TRUE;
}

static void SssSaveLayout(HWND dialog)
{
  if (::GetPropW(dialog, L"SSS_OrigLayout"))
    return; // already captured

  SssCollectContext ctx;
  ctx.Parent = dialog;
  ::EnumChildWindows(dialog, SssCollectProc, (LPARAM)&ctx);

  const SIZE_T total = sizeof(SssLayoutHeader) +
      (ctx.Items.Size() > 1 ? (ctx.Items.Size() - 1) * sizeof(SssLayoutItem) : 0);
  HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, total);
  if (!hMem)
    return;
  SssLayoutHeader *header = (SssLayoutHeader *)::GlobalLock(hMem);
  header->Count = ctx.Items.Size();
  for (UINT i = 0; i < header->Count; i++)
    header->Items[i] = ctx.Items[i];
  ::GlobalUnlock(hMem);
  ::SetPropW(dialog, L"SSS_OrigLayout", (HANDLE)hMem);
}

static void SssFreeLayout(HWND dialog)
{
  HANDLE h = ::GetPropW(dialog, L"SSS_OrigLayout");
  if (h)
  {
    ::GlobalFree((HGLOBAL)h);
    ::RemovePropW(dialog, L"SSS_OrigLayout");
  }
}

// Returns the new client height needed to contain all rows (incl. padding).
static int SssRelayoutCompact(HWND dialog, unsigned pt)
{
  SssSaveLayout(dialog);
  HANDLE h = ::GetPropW(dialog, L"SSS_OrigLayout");
  if (!h)
    return 0;
  SssLayoutHeader *header = (SssLayoutHeader *)::GlobalLock(h);
  if (!header || header->Count == 0)
  {
    ::GlobalUnlock(h);
    return 0;
  }

  HDC dc = ::GetDC(dialog);
  const UINT dpi = dc ? static_cast<UINT>(::GetDeviceCaps(dc, LOGPIXELSY)) : 96;
  if (dc)
    ::ReleaseDC(dialog, dc);
  const int fontPx = ::MulDiv((int)pt, (int)dpi, 72);
  const int rowH = fontPx * 7 / 5;   // compact 1.4x line height
  const int pad = fontPx / 2 + 4;
  const int minH = fontPx + 8;

  CRecordVector<unsigned> order;
  for (UINT i = 0; i < header->Count; i++)
    order.Add(i);
  for (UINT i = 1; i < order.Size(); i++)
  {
    unsigned v = order[i];
    UINT j = i;
    while (j > 0)
    {
      const SssLayoutItem &a = header->Items[order[j - 1]];
      const SssLayoutItem &b = header->Items[v];
      if (a.Y < b.Y || (a.Y == b.Y && a.X <= b.X))
        break;
      order[j] = order[j - 1];
      j--;
    }
    order[j] = v;
  }

  int rowIndex = -1;
  int rowBaseY = 0;
  int bottom = pad;
  for (UINT k = 0; k < order.Size(); k++)
  {
    const SssLayoutItem &item = header->Items[order[k]];
    if (rowIndex < 0 || item.Y - rowBaseY > 6)
    {
      rowIndex++;
      rowBaseY = item.Y;
    }
    int newH = item.Height;
    if (newH < minH)
      newH = minH;
    const int newY = pad + rowIndex * rowH;
    ::MoveWindow(item.Hwnd, item.X, newY, item.Width, newH, TRUE);
    // The window only needs to contain the visible part of each control: a
    // combo box shows just its edit field while folded (its drop-down list
    // is a popup window), so do not count the full template height here.
    int visH = newH;
    wchar_t cls[32] = {};
    ::GetClassNameW(item.Hwnd, cls, 32);
    if (::lstrcmpiW(cls, L"ComboBox") == 0)
      visH = minH;
    const int itemBottom = newY + visH + pad;
    if (itemBottom > bottom)
      bottom = itemBottom;
  }

  ::GlobalUnlock(h);
  return bottom;
}

struct SssSheetAdjustContext
{
  int Delta;
  HWND Page;
};

static BOOL CALLBACK SssAdjustSheetChildProc(HWND hwnd, LPARAM lParam)
{
  const SssSheetAdjustContext *ctx = (const SssSheetAdjustContext *)lParam;
  if (hwnd == ctx->Page)
    return TRUE; // page already resized
  HWND sheet = ::GetParent(hwnd);
  if (!sheet)
    return TRUE;
  RECT r = {};
  if (!::GetWindowRect(hwnd, &r))
    return TRUE;
  POINT p = { r.left, r.top };
  ::ScreenToClient(sheet, &p);
  const int w = r.right - r.left;
  const int h = r.bottom - r.top;
  wchar_t cls[64] = {};
  ::GetClassNameW(hwnd, cls, 64);
  if (::lstrcmpiW(cls, L"SysTabControl32") == 0)
    ::MoveWindow(hwnd, p.x, p.y, w, h + ctx->Delta, TRUE);
  else
    ::MoveWindow(hwnd, p.x, p.y + ctx->Delta, w, h, TRUE);
  return TRUE;
}

static BOOL CALLBACK SssClearComboSelectionProc(HWND hwnd, LPARAM /* lParam */)
{
  wchar_t cls[64] = {};
  ::GetClassNameW(hwnd, cls, 64);
  if (::lstrcmpiW(cls, L"ComboBox") == 0)
  {
    ::SendMessageW(hwnd, CB_SETEDITSEL, 0, MAKELPARAM(0, 0));
    COMBOBOXINFO info = { sizeof(info) };
    if (::GetComboBoxInfo(hwnd, &info) && info.hwndItem)
      ::SendMessageW(info.hwndItem, EM_SETSEL, 0, 0);
  }
  return TRUE;
}

// Re-layout the page contents (row heights, control heights) only. The page
// window position and the property-sheet layout are left to the sheet; the
// sheet is sized later, when the page becomes active (PSN_SETACTIVE), so it
// is fully laid out by then.
static void SssApplyRegisteredPageSettings(HWND page)
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
  if (pt == 0)
    return;

  SssRelayoutCompact(page, pt);
  SssApplyFontToTree(page, pt);
  ::EnumChildWindows(page, SssClearComboSelectionProc, 0);
}

// Called when the page becomes active: grow the page window and the parent
// sheet so the re-laid-out content is fully visible. Idempotent: the needed
// height is recomputed from the template layout each time and the page is
// only grown when it is shorter than the content.
static void SssFitPageToContent(HWND page)
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
  if (pt == 0)
    return;

  const int contentBottom = SssRelayoutCompact(page, pt);
  if (contentBottom <= 0)
    return;

  RECT pr = {};
  RECT pcr = {};
  if (!::GetWindowRect(page, &pr) || !::GetClientRect(page, &pcr))
    return;
  const int border = (pr.bottom - pr.top) - pcr.bottom;
  const int needH = contentBottom + border;
  if (needH <= (int)(pr.bottom - pr.top))
    return;

  ::SetWindowPos(page, nullptr, 0, 0, pr.right - pr.left, needH,
      SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

  HWND sheet = ::GetParent(page);
  if (!sheet)
    return;
  const int dh = needH - (int)(pr.bottom - pr.top);
  RECT sr = {};
  if (::GetWindowRect(sheet, &sr))
    ::SetWindowPos(sheet, nullptr, sr.left, sr.top,
        sr.right - sr.left, sr.bottom - sr.top + dh,
        SWP_NOZORDER | SWP_NOACTIVATE);
  SssSheetAdjustContext ctx = { dh, page };
  ::EnumChildWindows(sheet, SssAdjustSheetChildProc, (LPARAM)&ctx);
}
// **************** SSS Modification End ****************

static INT_PTR APIENTRY MyProperyPageProcedure(HWND dialogHWND, UINT message, WPARAM wParam, LPARAM lParam)
{
  CWindow tempDialog(dialogHWND);
  if (message == WM_INITDIALOG)
    tempDialog.SetUserDataLongPtr(((PROPSHEETPAGE *)lParam)->lParam);
  CDialog *dialog = (CDialog *)(tempDialog.GetUserDataLongPtr());
  if (dialog == NULL)
    return FALSE;
  if (message == WM_INITDIALOG)
  {
    dialog->Attach(dialogHWND);
    // **************** SSS Modification Start ****************
    SssApplyRegisteredPageSettings(dialogHWND);
    // **************** SSS Modification End ****************
  }
  // **************** SSS Modification Start ****************
  if (message == WM_NOTIFY)
  {
    const NMHDR *nmhdr = (const NMHDR *)lParam;
    if (nmhdr && nmhdr->code == PSN_SETACTIVE)
      SssFitPageToContent(dialogHWND);
  }
  else if (message == WM_DESTROY)
    SssFreeLayout(dialogHWND);
  // **************** SSS Modification End ****************
  try { return BoolToBOOL(dialog->OnMessage(message, wParam, lParam)); }
  catch(...) { return TRUE; }
}

bool CPropertyPage::OnNotify(UINT /* controlID */, LPNMHDR lParam)
{
  switch (lParam->code)
  {
    case PSN_APPLY: SetMsgResult(OnApply(LPPSHNOTIFY(lParam))); break;
    case PSN_KILLACTIVE: SetMsgResult(BoolToBOOL(OnKillActive(LPPSHNOTIFY(lParam)))); break;
    case PSN_SETACTIVE: SetMsgResult(OnSetActive(LPPSHNOTIFY(lParam))); break;
    case PSN_RESET: OnReset(LPPSHNOTIFY(lParam)); break;
    default: return false;
  }
  return true;
}

INT_PTR MyPropertySheet(const CObjectVector<CPageInfo> &pagesInfo, HWND hwndParent, const UString &title)
{
  #ifndef _UNICODE
  AStringVector titles;
  #endif
  #ifndef _UNICODE
  CRecordVector<PROPSHEETPAGEA> pagesA;
  #endif
  CRecordVector<PROPSHEETPAGEW> pagesW;

  unsigned i;
  #ifndef _UNICODE
  for (i = 0; i < pagesInfo.Size(); i++)
    titles.Add(GetSystemString(pagesInfo[i].Title));
  #endif

  for (i = 0; i < pagesInfo.Size(); i++)
  {
    const CPageInfo &pageInfo = pagesInfo[i];
    #ifndef _UNICODE
    {
      PROPSHEETPAGE page;
      page.dwSize = sizeof(page);
      page.dwFlags = 0;
      page.hInstance = g_hInstance;
      page.pszTemplate = MAKEINTRESOURCE(pageInfo.ID);
      page.pszIcon = NULL;
      page.pfnDlgProc = NWindows::NControl::MyProperyPageProcedure;

      if (titles[i].IsEmpty())
        page.pszTitle = NULL;
      else
      {
        page.dwFlags |= PSP_USETITLE;
        page.pszTitle = titles[i];
      }
      page.lParam = (LPARAM)pageInfo.Page;
      page.pfnCallback = NULL;
      pagesA.Add(page);
    }
    #endif
    {
      PROPSHEETPAGEW page;
      page.dwSize = sizeof(page);
      page.dwFlags = 0;
      page.hInstance = g_hInstance;
      page.pszTemplate = MAKEINTRESOURCEW(pageInfo.ID);
      page.pszIcon = NULL;
      page.pfnDlgProc = NWindows::NControl::MyProperyPageProcedure;

      if (pageInfo.Title.IsEmpty())
        page.pszTitle = NULL;
      else
      {
        page.dwFlags |= PSP_USETITLE;
        page.pszTitle = pageInfo.Title;
      }
      page.lParam = (LPARAM)pageInfo.Page;
      page.pfnCallback = NULL;
      pagesW.Add(page);
    }
  }

  #ifndef _UNICODE
  if (!g_IsNT)
  {
    PROPSHEETHEADER sheet;
    sheet.dwSize = sizeof(sheet);
    sheet.dwFlags = PSH_PROPSHEETPAGE | PSH_NOCONTEXTHELP;
    sheet.hwndParent = hwndParent;
    sheet.hInstance = g_hInstance;
    AString titleA (GetSystemString(title));
    sheet.pszCaption = titleA;
    sheet.nPages = pagesInfo.Size();
    sheet.nStartPage = 0;
    sheet.ppsp = &pagesA.Front();
    sheet.pfnCallback = NULL;
    return ::PropertySheetA(&sheet);
  }
  else
  #endif
  {
    PROPSHEETHEADERW sheet;
    sheet.dwSize = sizeof(sheet);
    sheet.dwFlags = PSH_PROPSHEETPAGE | PSH_NOCONTEXTHELP;
    sheet.hwndParent = hwndParent;
    sheet.hInstance = g_hInstance;
    sheet.pszCaption = title;
    sheet.nPages = pagesInfo.Size();
    sheet.nStartPage = 0;
    sheet.ppsp = &pagesW.Front();
    sheet.pfnCallback = NULL;
    return ::PropertySheetW(&sheet);
  }
}

}}
