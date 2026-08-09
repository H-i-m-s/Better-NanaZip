// Windows/Control/Dialog.cpp

#include "StdAfx.h"

// #include "../../Windows/DLL.h"

#ifndef _UNICODE
#include "../../Common/StringConvert.h"
#endif

#include "Dialog.h"

// **************** NanaZip Modification Start ****************
#include <K7User.h>
// **************** NanaZip Modification End ****************

extern HINSTANCE g_hInstance;
#ifndef _UNICODE
extern bool g_IsNT;
#endif

namespace NWindows {
namespace NControl {

// **************** SSS Modification Start ****************
// Generic dialog font support: every dialog built on CDialog reads the
// registered dialog font size once at WM_INITDIALOG, applies the font and
// recomputes a compact layout (row heights only, horizontal positions
// untouched) so the dialog adapts without stretching the gaps. The original
// template layout is captured once and remembered in the SSS_OrigLayout
// property, so repeated calls are idempotent.

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
    return;

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
    // The window must contain the actual bottom of every control, not just
    // the row height: combo boxes carry their drop-down list inside their
    // height, so sizing the window by rows alone would clip the list.
    const int itemBottom = newY + newH + pad;
    if (itemBottom > bottom)
      bottom = itemBottom;
  }

  ::GlobalUnlock(h);
  return bottom;
}

static void SssResizeDialogToContent(HWND dialog, int contentBottom)
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

static void SssApplyRegisteredDialogSettings(HWND hwnd)
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

  SssResizeDialogToContent(hwnd, SssRelayoutCompact(hwnd, pt));
  SssApplyFontToTree(hwnd, pt);
  ::EnumChildWindows(hwnd, SssClearComboSelectionProc, 0);
}
// **************** SSS Modification End ****************

static
#ifdef Z7_OLD_WIN_SDK
  BOOL
#else
  INT_PTR
#endif
APIENTRY
DialogProcedure(HWND dialogHWND, UINT message, WPARAM wParam, LPARAM lParam)
{
  CWindow tempDialog(dialogHWND);
  if (message == WM_INITDIALOG)
    tempDialog.SetUserDataLongPtr(lParam);
  CDialog *dialog = (CDialog *)(tempDialog.GetUserDataLongPtr());
  if (dialog == NULL)
    return FALSE;
  if (message == WM_INITDIALOG)
  // **************** NanaZip Modification Start ****************
  {
  // **************** NanaZip Modification End ****************
    dialog->Attach(dialogHWND);
  // **************** SSS Modification Start ****************
    SssApplyRegisteredDialogSettings(dialogHWND);
  // **************** SSS Modification End ****************
  // **************** NanaZip Modification Start ****************
    ::K7UserModernSetForegroundWindow(dialogHWND);
  }
  // **************** NanaZip Modification End ****************
  // **************** SSS Modification Start ****************
  else if (message == WM_DESTROY)
    SssFreeLayout(dialogHWND);
  // **************** SSS Modification End ****************

  /* MSDN: The dialog box procedure should return
       TRUE  - if it processed the message
       FALSE - if it did not process the message
     If the dialog box procedure returns FALSE,
     the dialog manager performs the default dialog operation in response to the message.
  */

  try { return BoolToBOOL(dialog->OnMessage(message, wParam, lParam)); }
  catch(...) { return TRUE; }
}

bool CDialog::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
    case WM_INITDIALOG: return OnInit();
    case WM_COMMAND: return OnCommand(HIWORD(wParam), LOWORD(wParam), lParam);
    case WM_NOTIFY: return OnNotify((UINT)wParam, (LPNMHDR) lParam);
    case WM_TIMER: return OnTimer(wParam, lParam);
    case WM_SIZE: return OnSize(wParam, LOWORD(lParam), HIWORD(lParam));
    case WM_DESTROY: return OnDestroy();
    // **************** NanaZip Modification Start ****************
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
    case WM_HELP: OnHelp(); return true;
    /*
        OnHelp(
          #ifdef UNDER_CE
          (void *)
          #else
          (LPHELPINFO)
          #endif
          lParam);
        return true;
    */
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
    // **************** NanaZip Modification End ****************
    default: return false;
  }
}

/*
bool CDialog::OnCommand2(WPARAM wParam, LPARAM lParam)
{
  return OnCommand(HIWORD(wParam), LOWORD(wParam), lParam);
}
*/

bool CDialog::OnCommand(unsigned code, unsigned itemID, LPARAM lParam)
{
  if (code == BN_CLICKED)
    return OnButtonClicked(itemID, (HWND)lParam);
  return false;
}

bool CDialog::OnButtonClicked(unsigned buttonID, HWND /* buttonHWND */)
{
  switch (buttonID)
  {
    case IDOK: OnOK(); break;
    case IDCANCEL: OnCancel(); break;
    case IDCLOSE: OnClose(); break;
    case IDCONTINUE: OnContinue(); break;
    // **************** NanaZip Modification Start ****************
    //case IDHELP: OnHelp(); break;
    // **************** NanaZip Modification End ****************
    default: return false;
  }
  return true;
}

#ifndef UNDER_CE
/* in win2000/win98 : monitor functions are supported.
   We need dynamic linking, if we want nt4/win95 support in program.
   Even if we compile the code with low (WINVER) value, we still
   want to use monitor functions. So we declare missing functions here */
// #if (WINVER < 0x0500)
#ifndef MONITOR_DEFAULTTOPRIMARY
extern "C" {
DECLARE_HANDLE(HMONITOR);
#define MONITOR_DEFAULTTOPRIMARY    0x00000001
typedef struct tagMONITORINFO
{
    DWORD   cbSize;
    RECT    rcMonitor;
    RECT    rcWork;
    DWORD   dwFlags;
} MONITORINFO, *LPMONITORINFO;
WINUSERAPI HMONITOR WINAPI MonitorFromWindow(HWND hwnd, DWORD dwFlags);
WINUSERAPI BOOL WINAPI GetMonitorInfoA(HMONITOR hMonitor, LPMONITORINFO lpmi);
}
#endif
#endif

static bool GetWorkAreaRect(RECT *rect, HWND hwnd)
{
  if (hwnd)
  {
    #ifndef UNDER_CE
    /* MonitorFromWindow() is supported in Win2000+
       MonitorFromWindow() : retrieves a handle to the display monitor that has the
         largest area of intersection with the bounding rectangle of a specified window.
       dwFlags: Determines the function's return value if the window does not intersect any display monitor.
         MONITOR_DEFAULTTONEAREST : Returns display that is nearest to the window.
         MONITOR_DEFAULTTONULL    : Returns NULL.
         MONITOR_DEFAULTTOPRIMARY : Returns the primary display monitor.
    */
    const HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
    if (hmon)
    {
      MONITORINFO mi;
      memset(&mi, 0, sizeof(mi));
      mi.cbSize = sizeof(mi);
      if (GetMonitorInfoA(hmon, &mi))
      {
        *rect = mi.rcWork;
        return true;
      }
    }
    #endif
  }

  /* Retrieves the size of the work area on the primary display monitor.
     The work area is the portion of the screen not obscured
     by the system taskbar or by application desktop toolbars.
     Any DPI virtualization mode of the caller has no effect on this output. */

  return BOOLToBool(::SystemParametersInfo(SPI_GETWORKAREA, 0, rect, 0));
}


bool IsDialogSizeOK(int xSize, int ySize, HWND hwnd)
{
  // it returns for system font. Real font uses another values
  const LONG v = GetDialogBaseUnits();
  const int x = LOWORD(v);
  const int y = HIWORD(v);

  RECT rect;
  GetWorkAreaRect(&rect, hwnd);
  const int wx = RECT_SIZE_X(rect);
  const int wy = RECT_SIZE_Y(rect);
  return
    xSize / 4 * x <= wx &&
    ySize / 8 * y <= wy;
}

bool CDialog::GetMargins(int margin, int &x, int &y)
{
  x = margin;
  y = margin;
  RECT rect;
  rect.left = 0;
  rect.top = 0;
  rect.right = margin;
  rect.bottom = margin;
  if (!MapRect(&rect))
    return false;
  x = rect.right - rect.left;
  y = rect.bottom - rect.top;
  return true;
}

int CDialog::Units_To_Pixels_X(int units)
{
  RECT rect;
  rect.left = 0;
  rect.top = 0;
  rect.right = units;
  rect.bottom = units;
  if (!MapRect(&rect))
    return units * 3 / 2;
  return rect.right - rect.left;
}

bool CDialog::GetItemSizes(unsigned id, int &x, int &y)
{
  RECT rect;
  if (!::GetWindowRect(GetItem(id), &rect))
    return false;
  x = RECT_SIZE_X(rect);
  y = RECT_SIZE_Y(rect);
  return true;
}

void CDialog::GetClientRectOfItem(unsigned id, RECT &rect)
{
  ::GetWindowRect(GetItem(id), &rect);
  ScreenToClient(&rect);
}

bool CDialog::MoveItem(unsigned id, int x, int y, int width, int height, bool repaint)
{
  return BOOLToBool(::MoveWindow(GetItem(id), x, y, width, height, BoolToBOOL(repaint)));
}


/*
typedef BOOL (WINAPI * Func_DwmGetWindowAttribute)(
    HWND hwnd, DWORD dwAttribute, PVOID pvAttribute, DWORD cbAttribute);

static bool GetWindowsRect_DWM(HWND hwnd, RECT *rect)
{
  // dll load and free is too slow : 300 calls in second.
  NDLL::CLibrary dll;
  if (!dll.Load(FTEXT("dwmapi.dll")))
    return false;
  Func_DwmGetWindowAttribute f = (Func_DwmGetWindowAttribute)dll.GetProc("DwmGetWindowAttribute" );
  if (f)
  {
    #define MY__DWMWA_EXTENDED_FRAME_BOUNDS 9
    // 30000 per second
    RECT r;
    if (f(hwnd, MY__DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(RECT)) == S_OK)
    {
      *rect = r;
      return true;
    }
  }
  return false;
}
*/


static bool IsRect_Small_Inside_Big(const RECT &sm, const RECT &big)
{
  return sm.left   >= big.left
      && sm.right  <= big.right
      && sm.top    >= big.top
      && sm.bottom <= big.bottom;
}


static bool AreRectsOverlapped(const RECT &r1, const RECT &r2)
{
  return r1.left   < r2.right
      && r1.right  > r2.left
      && r1.top    < r2.bottom
      && r1.bottom > r2.top;
}


static bool AreRectsEqual(const RECT &r1, const RECT &r2)
{
  return r1.left   == r2.left
      && r1.right  == r2.right
      && r1.top    == r2.top
      && r1.bottom == r2.bottom;
}


void CDialog::NormalizeSize(bool fullNormalize)
{
  RECT workRect;
  if (!GetWorkAreaRect(&workRect, *this))
    return;
  RECT rect;
  if (!GetWindowRect(&rect))
    return;
  int xs = RECT_SIZE_X(rect);
  int ys = RECT_SIZE_Y(rect);

  // we don't want to change size using workRect, if window is outside of WorkArea
  if (!AreRectsOverlapped(rect, workRect))
    return;

  /* here rect and workRect are overlapped, but it can be false
     overlapping of small shadow when window in another display. */

  const int xsW = RECT_SIZE_X(workRect);
  const int ysW = RECT_SIZE_Y(workRect);
  if (xs <= xsW && ys <= ysW)
    return; // size of window is OK
  if (fullNormalize)
  {
    Show(SW_SHOWMAXIMIZED);
    return;
  }
  int x = workRect.left;
  int y = workRect.top;
  if (xs < xsW)  x += (xsW - xs) / 2;  else xs = xsW;
  if (ys < ysW)  y += (ysW - ys) / 2;  else ys = ysW;
  Move(x, y, xs, ys, true);
}


void CDialog::NormalizePosition()
{
  RECT workRect;
  if (!GetWorkAreaRect(&workRect, *this))
    return;

  RECT rect2 = workRect;
  bool useWorkArea = true;
  const HWND parentHWND = GetParent();

  if (parentHWND)
  {
    RECT workRectParent;
    if (!GetWorkAreaRect(&workRectParent, parentHWND))
      return;

    // if windows are in different monitors, we use only workArea of current window

    if (AreRectsEqual(workRectParent, workRect))
    {
      // RECT rect3; if (GetWindowsRect_DWM(parentHWND, &rect3)) {}
      CWindow wnd(parentHWND);
      if (wnd.GetWindowRect(&rect2))
      {
        // it's same monitor. So we try to use parentHWND rect.
        /* we don't want to change position, if parent window is not inside work area.
           In Win10 : parent window rect is 8 pixels larger for each corner than window size for shadow.
           In maximize mode : window is outside of workRect.
           if parent window is inside workRect, we will use parent window instead of workRect */
        if (IsRect_Small_Inside_Big(rect2, workRect))
          useWorkArea = false;
      }
    }
  }

  RECT rect;
  if (!GetWindowRect(&rect))
    return;

  if (useWorkArea)
  {
    // we don't want to move window, if it's already inside.
    if (IsRect_Small_Inside_Big(rect, workRect))
      return;
    // we don't want to move window, if it's outside of workArea
    if (!AreRectsOverlapped(rect, workRect))
      return;
    rect2 = workRect;
  }

  {
    const int xs = RECT_SIZE_X(rect);
    const int ys = RECT_SIZE_Y(rect);
    const int xs2 = RECT_SIZE_X(rect2);
    const int ys2 = RECT_SIZE_Y(rect2);
    // we don't want to change position if parent is smaller.
    if (xs <= xs2 && ys <= ys2)
    {
      const int x = rect2.left + (xs2 - xs) / 2;
      const int y = rect2.top  + (ys2 - ys) / 2;

      if (x != rect.left || y != rect.top)
        Move(x, y, xs, ys, true);
      // SetWindowPos(*this, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
      return;
    }
  }
}



bool CModelessDialog::Create(LPCTSTR templateName, HWND parentWindow)
{
  const HWND aHWND = CreateDialogParam(g_hInstance, templateName, parentWindow, DialogProcedure, (LPARAM)this);
  if (!aHWND)
    return false;
  Attach(aHWND);
  return true;
}

INT_PTR CModalDialog::Create(LPCTSTR templateName, HWND parentWindow)
{
  return DialogBoxParam(g_hInstance, templateName, parentWindow, DialogProcedure, (LPARAM)this);
}

#ifndef _UNICODE

bool CModelessDialog::Create(LPCWSTR templateName, HWND parentWindow)
{
  HWND aHWND;
  if (g_IsNT)
    aHWND = CreateDialogParamW(g_hInstance, templateName, parentWindow, DialogProcedure, (LPARAM)this);
  else
  {
    AString name;
    LPCSTR templateNameA;
    if (IS_INTRESOURCE(templateName))
      templateNameA = (LPCSTR)templateName;
    else
    {
      name = GetSystemString(templateName);
      templateNameA = name;
    }
    aHWND = CreateDialogParamA(g_hInstance, templateNameA, parentWindow, DialogProcedure, (LPARAM)this);
  }
  if (aHWND == 0)
    return false;
  Attach(aHWND);
  return true;
}

INT_PTR CModalDialog::Create(LPCWSTR templateName, HWND parentWindow)
{
  if (g_IsNT)
    return DialogBoxParamW(g_hInstance, templateName, parentWindow, DialogProcedure, (LPARAM)this);
  AString name;
  LPCSTR templateNameA;
  if (IS_INTRESOURCE(templateName))
    templateNameA = (LPCSTR)templateName;
  else
  {
    name = GetSystemString(templateName);
    templateNameA = name;
  }
  return DialogBoxParamA(g_hInstance, templateNameA, parentWindow, DialogProcedure, (LPARAM)this);
}
#endif

}}
