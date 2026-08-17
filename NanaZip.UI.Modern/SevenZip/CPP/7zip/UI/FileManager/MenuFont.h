// MenuFont.h
// Selective owner-draw support for NanaZip-owned Win32 popup menus.

#ifndef __MENU_FONT_H
#define __MENU_FONT_H

#include <windows.h>

// Turns NanaZip-owned items in [firstPosition, firstPosition + itemCount)
// into owner-drawn items using the supplied point size. A zero point size
// leaves the range native so Windows continues to draw it with its menu font.
void ApplyNanaZipMenuFont(
    HMENU menu,
    HWND owner,
    unsigned pt,
    UINT firstPosition = 0,
    UINT itemCount = static_cast<UINT>(-1));

// Applies the selected font to an entire NanaZip menu tree. Before calling it,
// register every embedded Shell/third-party IContextMenu popup through
// ExcludeNanaZipMenuFont(); those submenus retain their provider-owned item
// data and rendering contract.
void ApplyNanaZipMenuFontTree(HMENU menu, HWND owner, unsigned pt);
void ExcludeNanaZipMenuFont(HMENU menu);

// Restores menu items registered by either apply function and releases their
// owner-draw metadata. Call this immediately after TrackPopupMenuEx returns.
void ResetNanaZipMenuFont(HMENU rootMenu);

// Owner-window dispatch helpers. Return true only when the message belongs to
// an item registered by the apply functions.
bool MeasureNanaZipMenuItem(MEASUREITEMSTRUCT *measureItem);
bool DrawNanaZipMenuItem(DRAWITEMSTRUCT *drawItem);

#endif
