// FontUtils.h
//
// SSS custom font-size support.
// Win32 helpers create and cache fonts for dialogs;
// WinUI helpers walk a XAML visual tree and set FontSize.

#ifndef __FONT_UTILS_H
#define __FONT_UTILS_H

#include <windows.h>

// winrt hosting headers transitively include Windows.UI.Xaml.Media.Animation
// whose GetCurrentTime member collides with the mmsystem.h macro.
#pragma push_macro("GetCurrentTime")
#undef GetCurrentTime
#include <winrt/Windows.UI.Xaml.Hosting.h>
#pragma pop_macro("GetCurrentTime")

// Win32 helpers.

// Get a cached font of the given point size for the given DPI.
// pt == 0 returns nullptr. Fonts are cached for the process lifetime.
HFONT GetAppFontByPt(unsigned pt, unsigned dpi = 0);

// Return a host-window height that can contain the selected font. The base
// height is used for the system-default setting.
unsigned GetAppFontContainerHeight(unsigned baseHeight, unsigned pt,
    unsigned dpi, unsigned extraPixels = 4);

// Apply the font of the given point size to a dialog and all its child
// controls. pt == 0 means "leave unchanged".
void ApplyFontToDialog(HWND dialog, unsigned pt);

// Compact dialog layout: keeps the horizontal positions, recomputes row
// heights from the font size and grows the window so nothing is clipped.
// isPropertyPage=true also adjusts the parent property sheet (tab control,
// buttons and window height). The original layout is captured once per
// window and remembered, so repeated calls are idempotent.
int ApplyFontToDialogCompact(HWND dialog, unsigned pt, bool isPropertyPage);

// WinUI helpers.

// Recursively set FontSize (converted from points) on the XAML content
// hosted by the given DesktopWindowXamlSource. pt == 0 means no-op.
void ApplyFontSizeToXamlSource(winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource const &source, unsigned pt);

#endif
