// MenuFont.cpp
// Selective owner-draw support for NanaZip-owned Win32 popup menus.

#include "StdAfx.h"

#include "../../../Common/StringConvert.h"

#include "MenuFont.h"
#include "FontUtils.h"

#include <K7User.h>

#include <windowsx.h>

#include <memory>
#include <string>
#include <vector>

namespace
{
  struct CMenuDrawItem
  {
    HMENU RootMenu;
    HMENU Menu;
    UINT Position;
    UINT Type;
    ULONG_PTR OriginalItemData;
    UINT State;
    HMENU SubMenu;
    HBITMAP CheckedBitmap;
    HBITMAP UncheckedBitmap;
    std::wstring Text;
    HFONT Font;
    bool IsSeparator;
    int SeparatorHeight;
    int IconSlotWidth;
    int PopupArrowWidth;
  };

  static std::vector<std::unique_ptr<CMenuDrawItem>> g_MenuDrawItems;
  static std::vector<HMENU> g_ExcludedMenuTrees;

  static bool IsExcludedMenu(HMENU menu)
  {
    for (HMENU excluded : g_ExcludedMenuTrees)
      if (excluded == menu)
        return true;
    return false;
  }

  static CMenuDrawItem *FindItem(ULONG_PTR itemData)
  {
    CMenuDrawItem *item = reinterpret_cast<CMenuDrawItem *>(itemData);
    for (const auto &candidate : g_MenuDrawItems)
      if (candidate.get() == item)
        return item;
    return nullptr;
  }

  static UINT GetDpi(HWND owner)
  {
    return owner ? ::GetDpiForWindow(owner) : USER_DEFAULT_SCREEN_DPI;
  }

  static int GetMetric(HWND owner, int index)
  {
    return ::GetSystemMetricsForDpi(index, GetDpi(owner));
  }

  static bool GetMenuItemText(HMENU menu, UINT position, std::wstring &text)
  {
    MENUITEMINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = MIIM_STRING;
    if (!::GetMenuItemInfoW(menu, position, TRUE, &info))
      return false;

    std::vector<wchar_t> buffer(info.cch + 1, L'\0');
    info.dwTypeData = buffer.data();
    info.cch = static_cast<UINT>(buffer.size() - 1);
    if (!::GetMenuItemInfoW(menu, position, TRUE, &info))
      return false;

    text = buffer.data();
    return true;
  }

  static void MeasureText(HDC dc, HFONT font, const std::wstring &text,
      int &captionWidth, int &shortcutWidth, int &height)
  {
    HGDIOBJ oldFont = ::SelectObject(dc, font);
    TEXTMETRICW metric = {};
    ::GetTextMetricsW(dc, &metric);
    height = metric.tmHeight;

    const std::size_t tab = text.find(L'\t');
    if (tab == std::wstring::npos)
    {
      SIZE size = {};
      ::GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
      captionWidth = size.cx;
      shortcutWidth = 0;
    }
    else
    {
      SIZE caption = {};
      SIZE shortcut = {};
      ::GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(tab), &caption);
      const wchar_t *shortcutText = text.c_str() + tab + 1;
      ::GetTextExtentPoint32W(
          dc,
          shortcutText,
          static_cast<int>(text.size() - tab - 1),
          &shortcut);
      captionWidth = caption.cx;
      shortcutWidth = shortcut.cx;
    }
    ::SelectObject(dc, oldFont);
  }

  static void DrawMenuText(HDC dc, HFONT font, const std::wstring &text,
      RECT textRect, COLORREF color, bool hideAccelerators)
  {
    HGDIOBJ oldFont = ::SelectObject(dc, font);
    const int oldMode = ::SetBkMode(dc, TRANSPARENT);
    const COLORREF oldColor = ::SetTextColor(dc, color);
    const std::size_t tab = text.find(L'\t');
    UINT format = DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
    if (hideAccelerators)
      format |= DT_HIDEPREFIX;

    if (tab == std::wstring::npos)
    {
      ::DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &textRect, format | DT_LEFT);
    }
    else
    {
      RECT captionRect = textRect;
      ::DrawTextW(dc, text.c_str(), static_cast<int>(tab), &captionRect, format | DT_LEFT);
      RECT shortcutRect = textRect;
      const wchar_t *shortcutText = text.c_str() + tab + 1;
      ::DrawTextW(
          dc,
          shortcutText,
          static_cast<int>(text.size() - tab - 1),
          &shortcutRect,
          format | DT_RIGHT);
    }

    ::SetTextColor(dc, oldColor);
    ::SetBkMode(dc, oldMode);
    ::SelectObject(dc, oldFont);
  }

  static void DrawCheckOrIcon(CMenuDrawItem *item, HDC dc, const RECT &rect,
      bool checked, bool disabled)
  {
    if (!checked)
      return;

    HBITMAP bitmap = item->CheckedBitmap ? item->CheckedBitmap : item->UncheckedBitmap;
    if (bitmap)
    {
      HDC memory = ::CreateCompatibleDC(dc);
      if (memory)
      {
        HGDIOBJ old = ::SelectObject(memory, bitmap);
        BITMAP source = {};
        if (::GetObjectW(bitmap, sizeof(source), &source))
        {
          const int x = rect.left + ((rect.right - rect.left) - source.bmWidth) / 2;
          const int y = rect.top + ((rect.bottom - rect.top) - source.bmHeight) / 2;
          ::BitBlt(dc, x, y, source.bmWidth, source.bmHeight, memory, 0, 0, SRCCOPY);
        }
        ::SelectObject(memory, old);
        ::DeleteDC(memory);
        return;
      }
    }

    const int checkSize = (std::min)(rect.right - rect.left, rect.bottom - rect.top) / 2;
    RECT checkRect = {
      rect.left + ((rect.right - rect.left) - checkSize) / 2,
      rect.top + ((rect.bottom - rect.top) - checkSize) / 2,
      rect.left + ((rect.right - rect.left) + checkSize) / 2,
      rect.top + ((rect.bottom - rect.top) + checkSize) / 2
    };
    ::DrawFrameControl(dc, &checkRect, DFC_MENU,
        DFCS_MENUCHECK | (disabled ? DFCS_INACTIVE : 0));
  }

  static void DrawSubMenuArrow(HDC dc, RECT rect, bool disabled, bool darkMode)
  {
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const int arrowWidth = (std::max)(3, width / 5);
    const int arrowHeight = (std::max)(5, height / 4);
    const int centerX = rect.left + width / 2;
    const int centerY = rect.top + height / 2;
    const POINT points[] = {
      { centerX - arrowWidth / 2, centerY - arrowHeight / 2 },
      { centerX - arrowWidth / 2, centerY + arrowHeight / 2 },
      { centerX + arrowWidth / 2, centerY }
    };

    HBRUSH brush = ::CreateSolidBrush(disabled
        ? (darkMode ? RGB(109, 109, 109) : ::GetSysColor(COLOR_GRAYTEXT))
        : (darkMode ? RGB(255, 255, 255) : ::GetSysColor(COLOR_MENUTEXT)));
    if (brush)
    {
      HGDIOBJ oldBrush = ::SelectObject(dc, brush);
      HGDIOBJ oldPen = ::SelectObject(dc, ::GetStockObject(NULL_PEN));
      ::Polygon(dc, points, ARRAY_SIZE(points));
      ::SelectObject(dc, oldPen);
      ::SelectObject(dc, oldBrush);
      ::DeleteObject(brush);
    }
  }

  static void ApplyMenuRange(HMENU rootMenu, HMENU menu, HWND owner, HFONT font,
      UINT firstPosition, UINT itemCount)
  {
    if (!menu || !font)
      return;

    const int menuCount = ::GetMenuItemCount(menu);
    if (menuCount <= 0 || firstPosition >= static_cast<UINT>(menuCount))
      return;

    const UINT endPosition = (itemCount == static_cast<UINT>(-1))
        ? static_cast<UINT>(menuCount)
        : (std::min)(static_cast<UINT>(menuCount), firstPosition + itemCount);

    for (UINT position = firstPosition; position < endPosition; position++)
    {
      MENUITEMINFOW info = {};
      info.cbSize = sizeof(info);
      info.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_SUBMENU | MIIM_CHECKMARKS | MIIM_DATA;
      if (!::GetMenuItemInfoW(menu, position, TRUE, &info))
        continue;

      // Bitmap and provider-owned owner-drawn entries stay untouched. Native
      // separators cannot remain native here: Windows otherwise paints their
      // full row with the light popup-menu theme between dark owner-drawn
      // items. We register them as an intentionally simple owner-draw row.
      if ((info.fType & (MFT_BITMAP | MFT_OWNERDRAW)) != 0)
        continue;

      const bool isSeparator = (info.fType & MFT_SEPARATOR) != 0;
      std::wstring text;
      if (!isSeparator && !GetMenuItemText(menu, position, text))
        continue;

      auto item = std::make_unique<CMenuDrawItem>();
      item->RootMenu = rootMenu;
      item->Menu = menu;
      item->Position = position;
      item->Type = info.fType;
      item->OriginalItemData = info.dwItemData;
      item->State = info.fState;
      item->SubMenu = info.hSubMenu;
      item->CheckedBitmap = info.hbmpChecked;
      item->UncheckedBitmap = info.hbmpUnchecked;
      item->Text = text;
      item->Font = font;
      item->IsSeparator = isSeparator;
      item->SeparatorHeight = ::MulDiv(8, GetDpi(owner), USER_DEFAULT_SCREEN_DPI);
      item->IconSlotWidth = GetMetric(owner, SM_CXMENUCHECK) +
          ::MulDiv(8, GetDpi(owner), USER_DEFAULT_SCREEN_DPI);
      item->PopupArrowWidth = info.hSubMenu ? GetMetric(owner, SM_CXMENUCHECK) : 0;

      MENUITEMINFOW update = {};
      update.cbSize = sizeof(update);
      update.fMask = MIIM_FTYPE | MIIM_DATA;
      update.fType = (info.fType & ~MFT_SEPARATOR) | MFT_OWNERDRAW;
      update.dwItemData = reinterpret_cast<ULONG_PTR>(item.get());
      if (::SetMenuItemInfoW(menu, position, TRUE, &update))
        g_MenuDrawItems.emplace_back(std::move(item));
    }
  }

  static void ApplyMenuTree(HMENU rootMenu, HMENU menu, HWND owner, HFONT font)
  {
    if (!menu || IsExcludedMenu(menu))
      return;

    ApplyMenuRange(rootMenu, menu, owner, font, 0, static_cast<UINT>(-1));

    const int count = ::GetMenuItemCount(menu);
    for (int position = 0; position < count; position++)
    {
      MENUITEMINFOW info = {};
      info.cbSize = sizeof(info);
      info.fMask = MIIM_SUBMENU;
      if (::GetMenuItemInfoW(menu, position, TRUE, &info) && info.hSubMenu)
        ApplyMenuTree(rootMenu, info.hSubMenu, owner, font);
    }
  }
}

void ApplyNanaZipMenuFont(HMENU menu, HWND owner, unsigned pt,
    UINT firstPosition, UINT itemCount)
{
  if (!menu || pt == 0)
    return;

  HFONT font = GetAppFontByPt(pt, GetDpi(owner));
  if (font)
    ApplyMenuRange(menu, menu, owner, font, firstPosition, itemCount);
}

void ApplyNanaZipMenuFontTree(HMENU menu, HWND owner, unsigned pt)
{
  if (!menu || pt == 0)
    return;

  HFONT font = GetAppFontByPt(pt, GetDpi(owner));
  if (font)
    ApplyMenuTree(menu, menu, owner, font);
}

void ExcludeNanaZipMenuFont(HMENU menu)
{
  if (menu && !IsExcludedMenu(menu))
    g_ExcludedMenuTrees.push_back(menu);
}

void ResetNanaZipMenuFont(HMENU rootMenu)
{
  for (auto it = g_MenuDrawItems.begin(); it != g_MenuDrawItems.end();)
  {
    CMenuDrawItem *item = it->get();
    if (item->RootMenu != rootMenu)
    {
      ++it;
      continue;
    }

    MENUITEMINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = MIIM_FTYPE | MIIM_DATA;
    info.fType = item->Type;
    info.dwItemData = item->OriginalItemData;
    ::SetMenuItemInfoW(item->Menu, item->Position, TRUE, &info);
    it = g_MenuDrawItems.erase(it);
  }
  // Popup menus are displayed synchronously by this UI thread, so exclusions
  // registered for the just-closed root cannot be needed by another popup.
  g_ExcludedMenuTrees.clear();
}

bool MeasureNanaZipMenuItem(MEASUREITEMSTRUCT *measureItem)
{
  if (!measureItem || measureItem->CtlType != ODT_MENU)
    return false;

  CMenuDrawItem *item = FindItem(measureItem->itemData);
  if (!item)
    return false;

  if (item->IsSeparator)
  {
    // The row itself carries the dark surface; the one-pixel rule is drawn in
    // DrawNanaZipMenuItem(). This prevents a light native separator band.
    measureItem->itemWidth = 1;
    measureItem->itemHeight = item->SeparatorHeight;
    return true;
  }

  HDC dc = ::GetDC(nullptr);
  if (!dc)
    return false;

  int captionWidth = 0;
  int shortcutWidth = 0;
  int textHeight = 0;
  MeasureText(dc, item->Font, item->Text, captionWidth, shortcutWidth, textHeight);
  ::ReleaseDC(nullptr, dc);

  // Text and icon slot were both created for the owner window's DPI. The
  // metrics below are only padding, intentionally modest so the menu remains
  // compact while still scaling with the selected point size.
  const int horizontalPadding = 16;
  const int shortcutGap = shortcutWidth ? 28 : 0;
  const int verticalPadding = 8;

  measureItem->itemWidth = item->IconSlotWidth + horizontalPadding + captionWidth +
      shortcutGap + shortcutWidth + item->PopupArrowWidth;
  measureItem->itemHeight = (std::max)(textHeight + verticalPadding,
      ::GetSystemMetrics(SM_CYMENUCHECK) + 4);
  return true;
}

bool DrawNanaZipMenuItem(DRAWITEMSTRUCT *drawItem)
{
  if (!drawItem || drawItem->CtlType != ODT_MENU)
    return false;

  CMenuDrawItem *item = FindItem(drawItem->itemData);
  if (!item)
    return false;

  const bool disabled = (drawItem->itemState & (ODS_DISABLED | ODS_GRAYED)) != 0;
  const bool selected = (drawItem->itemState & (ODS_SELECTED | ODS_HOTLIGHT)) != 0;
  const bool checked = (drawItem->itemState & ODS_CHECKED) != 0;
  const bool hideAccelerators = (drawItem->itemState & ODS_NOACCEL) != 0;
  const bool darkMode = ::K7UserShouldAppsUseDarkMode() != FALSE;
  const COLORREF background = darkMode
      ? (selected ? RGB(65, 65, 65) : RGB(0, 0, 0))
      : (selected ? ::GetSysColor(COLOR_HIGHLIGHT) : ::GetSysColor(COLOR_MENU));
  const COLORREF textColor = disabled
      ? (darkMode ? RGB(109, 109, 109) : ::GetSysColor(COLOR_GRAYTEXT))
      : (darkMode ? RGB(255, 255, 255) :
      (selected ? ::GetSysColor(COLOR_HIGHLIGHTTEXT) : ::GetSysColor(COLOR_MENUTEXT)));

  HBRUSH brush = ::CreateSolidBrush(background);
  if (brush)
  {
    ::FillRect(drawItem->hDC, &drawItem->rcItem, brush);
    ::DeleteObject(brush);
  }

  if (item->IsSeparator)
  {
    RECT separatorRect = drawItem->rcItem;
    separatorRect.top = (separatorRect.top + separatorRect.bottom) / 2;
    separatorRect.bottom = separatorRect.top + 1;
    HBRUSH separatorBrush = ::CreateSolidBrush(
        darkMode ? RGB(63, 63, 63) : ::GetSysColor(COLOR_3DSHADOW));
    if (separatorBrush)
    {
      ::FillRect(drawItem->hDC, &separatorRect, separatorBrush);
      ::DeleteObject(separatorBrush);
    }
    return true;
  }

  RECT iconRect = drawItem->rcItem;
  iconRect.right = iconRect.left + item->IconSlotWidth;
  DrawCheckOrIcon(item, drawItem->hDC, iconRect, checked, disabled);

  RECT textRect = drawItem->rcItem;
  textRect.left = iconRect.right + 4;
  textRect.right -= item->PopupArrowWidth + 8;
  DrawMenuText(drawItem->hDC, item->Font, item->Text, textRect, textColor,
      hideAccelerators);

  if (item->SubMenu)
  {
    RECT arrowRect = drawItem->rcItem;
    arrowRect.left = arrowRect.right - item->PopupArrowWidth;
    DrawSubMenuArrow(drawItem->hDC, arrowRect, disabled, darkMode);
  }
  return true;
}
