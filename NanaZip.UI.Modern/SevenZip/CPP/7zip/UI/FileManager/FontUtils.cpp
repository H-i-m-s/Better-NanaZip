// FontUtils.cpp
//
// SSS custom font-size support implementation.

#include "StdAfx.h"

#include "FontUtils.h"

#include <map>
#include <vector>

// winrt headers pull in Windows.UI.Xaml.Media.Animation whose
// GetCurrentTime member collides with the mmsystem.h macro.
#undef GetCurrentTime

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.h>

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Hosting;
using namespace winrt::Windows::UI::Xaml::Media;

// **************** SSS Modification Start ****************

// ---- Win32 font helpers ----

struct CFontKey
{
  unsigned Pt;
  unsigned Dpi;

  bool operator<(const CFontKey &other) const
  {
    if (Pt != other.Pt)
      return Pt < other.Pt;
    return Dpi < other.Dpi;
  }
};

static std::map<CFontKey, HFONT> g_FontCache;

static HFONT CreateAppFont(unsigned pt, unsigned dpi)
{
  const int height = -::MulDiv((int)pt, (int)dpi, 72);

  LOGFONTW lf = {};
  lf.lfHeight = height;
  lf.lfCharSet = DEFAULT_CHARSET;
  lf.lfQuality = CLEARTYPE_QUALITY;

  NONCLIENTMETRICSW ncm = {};
  ncm.cbSize = sizeof(ncm);
  if (::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0) && ncm.lfMessageFont.lfFaceName[0])
    wcscpy_s(lf.lfFaceName, ncm.lfMessageFont.lfFaceName);
  else
    wcscpy_s(lf.lfFaceName, L"Segoe UI");

  return ::CreateFontIndirectW(&lf);
}

HFONT GetAppFontByPt(unsigned pt, unsigned dpi)
{
  // SSS defensive: absurd sizes (corrupt registry values) are ignored.
  if (pt == 0 || pt > 36)
    return nullptr;
  if (dpi == 0)
    dpi = USER_DEFAULT_SCREEN_DPI;

  CFontKey key = { pt, dpi };
  auto it = g_FontCache.find(key);
  if (it != g_FontCache.end())
    return it->second;

  HFONT font = CreateAppFont(pt, dpi);
  if (font)
    g_FontCache.emplace(key, font);
  return font;
}

unsigned GetAppFontContainerHeight(unsigned baseHeight, unsigned pt,
    unsigned dpi, unsigned extraPixels)
{
  if (dpi == 0)
    dpi = USER_DEFAULT_SCREEN_DPI;
  // SSS defensive: absurd sizes (corrupt registry values) fall back to the
  // system default; otherwise the header heights explode and the file list
  // collapses to zero height.
  if (pt == 0 || pt > 36)
    return MulDiv((int)baseHeight, (int)dpi, USER_DEFAULT_SCREEN_DPI);

  const unsigned textHeight = MulDiv((int)pt, (int)dpi, 72);
  const unsigned baseTextHeight = MulDiv(9, (int)dpi, 72);
  const unsigned denominator = baseTextHeight ? baseTextHeight : 1;
  const unsigned scaled = (baseHeight * (textHeight ? textHeight : 1) + denominator - 1) /
      denominator;
  const unsigned baseScaled = MulDiv((int)baseHeight, (int)dpi, USER_DEFAULT_SCREEN_DPI);
  const unsigned extraScaled = MulDiv((int)extraPixels, (int)dpi, USER_DEFAULT_SCREEN_DPI);
  return (scaled + extraScaled > baseScaled) ? scaled + extraScaled : baseScaled;
}

static BOOL CALLBACK SetChildFontProc(HWND hwnd, LPARAM lParam)
{
  ::SendMessageW(hwnd, WM_SETFONT, (WPARAM)lParam, TRUE);
  return TRUE;
}

void ApplyFontToDialog(HWND dialog, unsigned pt)
{
  if (pt == 0 || !dialog)
    return;

  const UINT dpi = ::GetDpiForWindow(dialog);
  HFONT font = GetAppFontByPt(pt, dpi);
  if (!font)
    return;

  ::SendMessageW(dialog, WM_SETFONT, (WPARAM)font, TRUE);
  ::EnumChildWindows(dialog, SetChildFontProc, (LPARAM)font);
}

// ---- Dialog compact layout ----
// Keep horizontal positions; recompute row heights from the font size and
// grow the window so nothing is clipped. The original layout is captured
// once per window and remembered in the SSS_OrigLayout property, so calling
// this again with a new font size is always based on the template layout.

struct CDialogLayoutItem
{
  HWND Hwnd;
  int X, Y, Width, Height;
};

struct CDialogLayoutHeader
{
  UINT Count;
  CDialogLayoutItem Items[1]; // actually Count entries
};

struct CDialogCollectContext
{
  HWND Parent;
  std::vector<CDialogLayoutItem> Items;
};

static BOOL CALLBACK CollectLayoutItemProc(HWND hwnd, LPARAM lParam)
{
  CDialogCollectContext *ctx = (CDialogCollectContext *)lParam;
  RECT r = {};
  if (::GetWindowRect(hwnd, &r))
  {
    POINT p = { r.left, r.top };
    ::ScreenToClient(ctx->Parent, &p);
    CDialogLayoutItem item;
    item.Hwnd = hwnd;
    item.X = p.x;
    item.Y = p.y;
    item.Width = r.right - r.left;
    item.Height = r.bottom - r.top;
    ctx->Items.push_back(item);
  }
  return TRUE;
}

static void SaveDialogLayout(HWND dialog)
{
  if (::GetPropW(dialog, L"SSS_OrigLayout"))
    return; // already captured

  CDialogCollectContext ctx;
  ctx.Parent = dialog;
  ::EnumChildWindows(dialog, CollectLayoutItemProc, (LPARAM)&ctx);

  const SIZE_T total = sizeof(CDialogLayoutHeader) +
      (ctx.Items.size() > 1 ? (ctx.Items.size() - 1) * sizeof(CDialogLayoutItem) : 0);
  HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, total);
  if (!hMem)
    return;
  CDialogLayoutHeader *header = (CDialogLayoutHeader *)::GlobalLock(hMem);
  header->Count = (UINT)ctx.Items.size();
  for (UINT i = 0; i < header->Count; i++)
    header->Items[i] = ctx.Items[i];
  ::GlobalUnlock(hMem);
  ::SetPropW(dialog, L"SSS_OrigLayout", (HANDLE)hMem);
}

// Returns the new client height needed to contain all rows (incl. padding).
static int RelayoutDialogCompact(HWND dialog, unsigned pt)
{
  SaveDialogLayout(dialog);
  HANDLE h = ::GetPropW(dialog, L"SSS_OrigLayout");
  if (!h)
    return 0;
  CDialogLayoutHeader *header = (CDialogLayoutHeader *)::GlobalLock(h);
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

  std::vector<unsigned> order;
  order.reserve(header->Count);
  for (UINT i = 0; i < header->Count; i++)
    order.push_back(i);
  for (UINT i = 1; i < order.size(); i++)
  {
    unsigned v = order[i];
    UINT j = i;
    while (j > 0)
    {
      const CDialogLayoutItem &a = header->Items[order[j - 1]];
      const CDialogLayoutItem &b = header->Items[v];
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
  for (UINT k = 0; k < order.size(); k++)
  {
    const CDialogLayoutItem &item = header->Items[order[k]];
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

static void ResizeDialogToContent(HWND dialog, int contentBottom)
{
  if (contentBottom <= 0)
    return;
  RECT r = {};
  RECT cr = {};
  if (!::GetWindowRect(dialog, &r) || !::GetClientRect(dialog, &cr))
    return;
  const int newH = (r.bottom - r.top) - cr.bottom + contentBottom;
  if (newH > (int)(r.bottom - r.top))
    ::SetWindowPos(dialog, nullptr, r.left, r.top, r.right - r.left, newH,
        SWP_NOZORDER | SWP_NOACTIVATE);
}

struct CDialogSheetAdjustContext
{
  int Delta;
  HWND Page;
};

static BOOL CALLBACK AdjustSheetChildProc(HWND hwnd, LPARAM lParam)
{
  const CDialogSheetAdjustContext *ctx = (const CDialogSheetAdjustContext *)lParam;
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

int ApplyFontToDialogCompact(HWND dialog, unsigned pt, bool isPropertyPage)
{
  if (!dialog || pt == 0)
    return 0;

  const int contentBottom = RelayoutDialogCompact(dialog, pt);

  if (isPropertyPage && contentBottom > 0)
  {
    HWND sheet = ::GetParent(dialog);
    RECT pr = {};
    RECT pcr = {};
    if (::GetWindowRect(dialog, &pr) && ::GetClientRect(dialog, &pcr))
    {
      const int border = (pr.bottom - pr.top) - pcr.bottom;
      const int oldH = pr.bottom - pr.top;
      const int newH = contentBottom + border;
      POINT pos = { pr.left, pr.top };
      if (sheet)
        ::ScreenToClient(sheet, &pos);
      ::MoveWindow(dialog, pos.x, pos.y, pr.right - pr.left, newH, TRUE);

      const int dh = newH - oldH;
      if (sheet && dh != 0)
      {
        RECT sr = {};
        if (::GetWindowRect(sheet, &sr))
          ::SetWindowPos(sheet, nullptr, sr.left, sr.top,
              sr.right - sr.left, sr.bottom - sr.top + dh,
              SWP_NOZORDER | SWP_NOACTIVATE);
        CDialogSheetAdjustContext ctx = { dh, dialog };
        ::EnumChildWindows(sheet, AdjustSheetChildProc, (LPARAM)&ctx);
      }
    }
  }
  else
    ResizeDialogToContent(dialog, contentBottom);

  ApplyFontToDialog(dialog, pt);
  return contentBottom;
}

// ---- WinUI (XAML) font helpers ----

static void ApplyFontSizeToXamlObject(DependencyObject const &obj, double size)
{
  if (!obj)
    return;

  if (auto textBlock = obj.try_as<TextBlock>())
    textBlock.FontSize(size);
  else if (auto richBlock = obj.try_as<RichTextBlock>())
    richBlock.FontSize(size);
  else if (auto richEdit = obj.try_as<RichEditBox>())
    richEdit.FontSize(size);
  else if (auto fontIcon = obj.try_as<FontIcon>())
    fontIcon.FontSize(size);
  else if (auto control = obj.try_as<Control>())
    control.FontSize(size);

  const uint32_t count = VisualTreeHelper::GetChildrenCount(obj);
  for (uint32_t i = 0; i < count; i++)
    ApplyFontSizeToXamlObject(VisualTreeHelper::GetChild(obj, i), size);
}

static void ResetFontSizeOnXamlObject(DependencyObject const &obj)
{
  if (!obj)
    return;

  if (auto textBlock = obj.try_as<TextBlock>())
    textBlock.ClearValue(TextBlock::FontSizeProperty());
  else if (auto richBlock = obj.try_as<RichTextBlock>())
    richBlock.ClearValue(RichTextBlock::FontSizeProperty());
  else if (auto fontIcon = obj.try_as<FontIcon>())
    fontIcon.ClearValue(FontIcon::FontSizeProperty());
  else if (auto control = obj.try_as<Control>())
    control.ClearValue(Control::FontSizeProperty());

  const uint32_t count = VisualTreeHelper::GetChildrenCount(obj);
  for (uint32_t i = 0; i < count; i++)
    ResetFontSizeOnXamlObject(VisualTreeHelper::GetChild(obj, i));
}

void ApplyFontSizeToXamlSource(DesktopWindowXamlSource const &source, unsigned pt)
{
  if (!source)
    return;

  DependencyObject content = source.Content();
  if (!content)
    return;

  if (pt == 0)
  {
    ResetFontSizeOnXamlObject(content);
    return;
  }

  // XAML FontSize is in DIPs; convert from points at the 96 DPI baseline.
  const double size = (double)pt * 96.0 / 72.0;
  ApplyFontSizeToXamlObject(content, size);
}

// **************** SSS Modification End ****************
