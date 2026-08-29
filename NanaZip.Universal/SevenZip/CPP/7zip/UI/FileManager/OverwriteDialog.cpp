// OverwriteDialog.cpp

#include "StdAfx.h"

#include <gdiplus.h>
#include <commctrl.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/FileFind.h"
#include "../../../Windows/PropVariantConv.h"
#include "../../../Windows/ResourceString.h"

#include "../../../Windows/Control/Static.h"

// **************** SSS Fluent Overwrite Dialog Start ****************
#include <K7User.h>
// **************** SSS Fluent Overwrite Dialog End ****************

#include "FormatUtils.h"
#include "LangUtils.h"
#include "OverwriteDialog.h"

#include "PropertyNameRes.h"

#include <algorithm>
#include <string>

using namespace NWindows;

namespace
{
  struct COverwriteLayoutMetrics
  {
    int FontHeight;
    int LineHeight;
    int Margin;
    int Gap;
    int ButtonGap;
    int ButtonHeight;
    int InfoGap;
    int IconColumn;
  };

  static std::wstring GetWindowTextString(HWND window)
  {
    if (!window)
      return std::wstring();

    const int Length = ::GetWindowTextLengthW(window);
    std::wstring Result(static_cast<size_t>(Length + 1), L'\0');
    if (Length > 0)
    {
      ::GetWindowTextW(window, &Result[0], Length + 1);
      Result.resize(static_cast<size_t>(Length));
    }
    else
    {
      Result.clear();
    }
    return Result;
  }

  static int CountTextLines(const std::wstring &text)
  {
    if (text.empty())
      return 1;

    int lines = 1;
    for (size_t i = 0; i < text.size(); i++)
    {
      if (text[i] == L'\n')
        lines++;
    }
    return lines;
  }

  static HFONT GetControlFont(HWND control, HFONT fallback)
  {
    if (!control)
      return fallback;
    HFONT font = reinterpret_cast<HFONT>(::SendMessageW(
        control, WM_GETFONT, 0, 0));
    return font ? font : fallback;
  }

  static int MeasureTextHeight(
      HDC dc,
      HWND control,
      HFONT fallback,
      const std::wstring &text,
      int width,
      int minLines,
      const COverwriteLayoutMetrics &metrics)
  {
    if (width <= 0)
      return 0;

    int height = 0;
    int lineHeight = metrics.LineHeight;
    if (dc && !text.empty())
    {
      HFONT font = GetControlFont(control, fallback);
      HFONT oldFont = font ? reinterpret_cast<HFONT>(::SelectObject(dc, font))
                           : nullptr;
      TEXTMETRICW textMetrics = {};
      if (::GetTextMetricsW(dc, &textMetrics))
      {
        const int externalLeading = (std::max)(
            2, static_cast<int>(textMetrics.tmExternalLeading));
        lineHeight = (std::max)(1, static_cast<int>(textMetrics.tmHeight)) +
            externalLeading;
      }
      RECT rect = { 0, 0, width, 0 };
      ::DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rect,
          DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX | DT_EDITCONTROL);
      height = static_cast<int>(rect.bottom - rect.top);
      if (font && oldFont)
        ::SelectObject(dc, oldFont);
    }

    const int lineCount = (std::max)(
        (std::max)(1, minLines), CountTextLines(text));
    lineHeight *= lineCount;
    const int safety = (std::max)(6, metrics.FontHeight / 4);
    // Use a small font-relative safety margin. A whole extra line leaves a
    // visible blank band before the following control.
    return (std::max)(lineHeight, height) + safety;
  }

  static int MeasureTextWidth(HDC dc, const std::wstring &text)
  {
    if (!dc || text.empty())
      return 0;

    SIZE size = {};
    if (!::GetTextExtentPoint32W(dc, text.c_str(),
        static_cast<int>(text.size()), &size))
      return 0;
    return size.cx;
  }

  static int MeasureControlTextWidth(
      HDC dc,
      HWND control,
      HFONT fallback,
      const std::wstring &text)
  {
    if (!dc || text.empty())
      return 0;
    HFONT font = GetControlFont(control, fallback);
    HFONT oldFont = font ? reinterpret_cast<HFONT>(::SelectObject(dc, font))
                         : nullptr;
    const int width = MeasureTextWidth(dc, text);
    if (font && oldFont)
      ::SelectObject(dc, oldFont);
    return width;
  }

  static std::wstring GetButtonCaptionForMeasurement(
      const std::wstring &caption)
  {
    std::wstring result;
    for (size_t i = 0; i < caption.size(); i++)
    {
      if (caption[i] == L'&')
      {
        if (i + 1 < caption.size() && caption[i + 1] == L'&')
          result += L'&', i++;
        continue;
      }
      result += caption[i];
    }
    return result;
  }

  static int MeasureButtonTextWidth(HDC dc, HWND button)
  {
    return MeasureTextWidth(dc, GetButtonCaptionForMeasurement(
        GetWindowTextString(button)));
  }

  static int MeasureButtonTextHeight(HDC dc, HWND button)
  {
    const std::wstring caption = GetButtonCaptionForMeasurement(
        GetWindowTextString(button));
    if (!dc || caption.empty())
      return 0;

    RECT rect = { 0, 0, 32767, 0 };
    ::DrawTextW(dc, caption.c_str(), static_cast<int>(caption.size()), &rect,
        DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    return (std::max)(0, static_cast<int>(rect.bottom - rect.top));
  }

  static SIZE GetButtonLayoutSize(HDC dc, HWND button, int fontHeight)
  {
    SIZE result = {};
    // BCM_GETIDEALSIZE is BCM_FIRST + 1. Keep the value local because the
    // older SDK used by this project does not declare the macro.
    const UINT kButtonGetIdealSize = 0x1601;
    if (button && ::SendMessageW(button, kButtonGetIdealSize, 0,
        reinterpret_cast<LPARAM>(&result)) != 0 &&
        result.cx > 0 && result.cy > 0)
      return result;

    const int textWidth = MeasureButtonTextWidth(dc, button);
    const int textHeight = MeasureButtonTextHeight(dc, button);
    const int horizontalPadding = (std::max)(24, fontHeight * 2);
    const int verticalPadding = (std::max)(8, fontHeight / 3);
    result.cx = (std::max)(1, textWidth + horizontalPadding);
    result.cy = (std::max)(1, textHeight + verticalPadding * 2 + 2);
    return result;
  }

  static int MeasureLongestControlLineWidth(
      HDC dc,
      HWND control,
      HFONT fallback,
      const std::wstring &text)
  {
    int result = 0;
    size_t lineStart = 0;
    for (size_t i = 0; i <= text.size(); i++)
    {
      if (i == text.size() || text[i] == L'\n')
      {
        std::wstring line = text.substr(lineStart, i - lineStart);
        if (!line.empty() && line.back() == L'\r')
          line.pop_back();
        result = (std::max)(result,
            MeasureControlTextWidth(dc, control, fallback, line));
        lineStart = i + 1;
      }
    }
    return result;
  }

  // SS_LEFT wraps at word boundaries, but a long filename without spaces can
  // otherwise be truncated. Insert safe character breaks before measuring and
  // assigning the final static-control rectangle.
  static std::wstring WrapTextForWidth(
      HDC dc,
      const std::wstring &text,
      int width)
  {
    if (!dc || width <= 0 || text.empty())
      return text;

    std::wstring result;
    std::wstring line;
    for (size_t i = 0; i <= text.size(); i++)
    {
      const wchar_t ch = i < text.size() ? text[i] : L'\n';
      if (ch == L'\r')
        continue;

      if (ch == L'\n')
      {
        result += line;
        result += L'\n';
        line.clear();
        continue;
      }

      std::wstring candidate = line;
      candidate += ch;
      if (!line.empty() && MeasureTextWidth(dc, candidate) > width)
      {
        result += line;
        result += L'\n';
        line.clear();
      }
      line += ch;
    }

    while (!result.empty() && result.back() == L'\n')
      result.pop_back();
    return result;
  }

  static void SetControlRect(
      HWND dialog,
      unsigned id,
      int x,
      int y,
      int width,
      int height)
  {
    HWND control = ::GetDlgItem(dialog, static_cast<int>(id));
    if (!control)
      return;
    ::SetWindowPos(control, nullptr, x, y, width, height,
        SWP_NOZORDER | SWP_NOACTIVATE);
  }

  static void SetWrappedStaticStyle(HWND control)
  {
    if (!control)
      return;
    LONG_PTR style = ::GetWindowLongPtrW(control, GWL_STYLE);
    style &= ~(SS_CENTER | SS_RIGHT | SS_CENTERIMAGE | SS_ENDELLIPSIS |
        SS_PATHELLIPSIS | SS_WORDELLIPSIS);
    style |= SS_LEFT | SS_NOPREFIX | SS_EDITCONTROL;
    ::SetWindowLongPtrW(control, GWL_STYLE, style);
  }

  static void SetSingleLineButtonStyle(HWND control)
  {
    if (!control)
      return;
    LONG_PTR style = ::GetWindowLongPtrW(control, GWL_STYLE);
    style &= ~(BS_MULTILINE | BS_LEFT | BS_RIGHT | BS_TOP | BS_BOTTOM);
    style |= BS_CENTER | BS_VCENTER;
    ::SetWindowLongPtrW(control, GWL_STYLE, style);
    ::SetWindowPos(control, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
        SWP_FRAMECHANGED);
    ::InvalidateRect(control, nullptr, TRUE);
  }

  static COverwriteLayoutMetrics GetOverwriteLayoutMetrics(HDC dc)
  {
    TEXTMETRICW metrics = {};
    if (!dc || !::GetTextMetricsW(dc, &metrics))
    {
      metrics.tmHeight = 16;
      metrics.tmExternalLeading = 2;
    }

    COverwriteLayoutMetrics Result = {};
    Result.FontHeight = (std::max)(1, static_cast<int>(metrics.tmHeight));
    const int ExternalLeading = (std::max)(
        2, static_cast<int>(metrics.tmExternalLeading));
    Result.LineHeight = Result.FontHeight + ExternalLeading;
    Result.Margin = (std::max)(12, Result.FontHeight);
    Result.Gap = (std::max)(8, Result.FontHeight / 2);
    Result.ButtonGap = (std::max)(10, Result.FontHeight / 2);
    Result.ButtonHeight = (std::max)(30, Result.FontHeight + 14);
    Result.InfoGap = (std::max)(2, Result.FontHeight / 8);
    Result.IconColumn = (std::max)(28, Result.FontHeight);
    return Result;
  }

  static void PositionOverwriteDialog(HWND dialog)
  {
    RECT windowRect = {};
    if (!::GetWindowRect(dialog, &windowRect))
      return;

    HMONITOR monitor = ::MonitorFromWindow(dialog, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!monitor || !::GetMonitorInfoW(monitor, &info))
      return;

    const int width = static_cast<int>(windowRect.right - windowRect.left);
    const int height = static_cast<int>(windowRect.bottom - windowRect.top);
    int x = static_cast<int>(windowRect.left);
    int y = static_cast<int>(windowRect.top);
    const int workLeft = static_cast<int>(info.rcWork.left);
    const int workTop = static_cast<int>(info.rcWork.top);
    const int workRight = static_cast<int>(info.rcWork.right);
    const int workBottom = static_cast<int>(info.rcWork.bottom);

    // Move the dialog into view, but never shrink it. A large-font dialog
    // must retain the content height calculated by LayoutOverwriteDialog.
    if (width <= workRight - workLeft)
      x = (std::max)(workLeft,
          (std::min)(x, workRight - width));
    else
      x = workLeft;
    if (height <= workBottom - workTop)
      y = (std::max)(workTop,
          (std::min)(y, workBottom - height));
    else
      y = workTop;

    if (x != windowRect.left || y != windowRect.top)
    {
      ::SetWindowPos(dialog, nullptr, x, y, 0, 0,
          SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
  }

  static void LayoutOverwriteDialog(
      HWND dialog,
      bool showExtraButtons)
  {
    if (!dialog)
      return;

    HDC dc = ::GetDC(dialog);
    HFONT font = reinterpret_cast<HFONT>(::SendMessageW(
        dialog, WM_GETFONT, 0, 0));
    HFONT oldFont = nullptr;
    if (dc && font)
      oldFont = reinterpret_cast<HFONT>(::SelectObject(dc, font));

    COverwriteLayoutMetrics metrics = GetOverwriteLayoutMetrics(dc);

    SetSingleLineButtonStyle(::GetDlgItem(dialog, IDYES));
    SetSingleLineButtonStyle(::GetDlgItem(dialog, IDNO));
    SetSingleLineButtonStyle(::GetDlgItem(dialog, IDCANCEL));
    SetSingleLineButtonStyle(::GetDlgItem(dialog, IDB_YES_TO_ALL));
    SetSingleLineButtonStyle(::GetDlgItem(dialog, IDB_NO_TO_ALL));
    SetSingleLineButtonStyle(::GetDlgItem(dialog, IDB_AUTO_RENAME));

    const SIZE YesSize = GetButtonLayoutSize(dc,
        ::GetDlgItem(dialog, IDYES), metrics.FontHeight);
    const SIZE NoSize = GetButtonLayoutSize(dc,
        ::GetDlgItem(dialog, IDNO), metrics.FontHeight);
    const SIZE CancelSize = GetButtonLayoutSize(dc,
        ::GetDlgItem(dialog, IDCANCEL), metrics.FontHeight);
    const SIZE YesAllSize = GetButtonLayoutSize(dc,
        ::GetDlgItem(dialog, IDB_YES_TO_ALL), metrics.FontHeight);
    const SIZE NoAllSize = GetButtonLayoutSize(dc,
        ::GetDlgItem(dialog, IDB_NO_TO_ALL), metrics.FontHeight);
    const SIZE AutoRenameSize = GetButtonLayoutSize(dc,
        ::GetDlgItem(dialog, IDB_AUTO_RENAME), metrics.FontHeight);

    // The first two columns share one width. The third column has its own
    // width, so the long Auto Rename caption does not stretch columns 1 and 2.
    const int Column12Width = (std::max)(
        (std::max)(YesSize.cx, NoSize.cx),
        (std::max)(YesAllSize.cx, NoAllSize.cx));
    const int Column3Width = (std::max)(AutoRenameSize.cx, CancelSize.cx);
    const int ButtonGridWidth = Column12Width * 2 + Column3Width +
        metrics.ButtonGap * 2;
    const int MaxButtonHeight = (std::max)(
        (std::max)(static_cast<int>(YesSize.cy),
            static_cast<int>(NoSize.cy)),
        (std::max)(
            (std::max)(static_cast<int>(YesAllSize.cy),
                static_cast<int>(NoAllSize.cy)),
            (std::max)(static_cast<int>(AutoRenameSize.cy),
                static_cast<int>(CancelSize.cy))));
    metrics.ButtonHeight = (std::max)(metrics.ButtonHeight,
        MaxButtonHeight);

    // When the optional actions are hidden, keep the remaining three buttons
    // as one centered-content row while preserving the right alignment.
    const int SingleButtonWidth = (std::max)(
        (std::max)(YesSize.cx, NoSize.cx), CancelSize.cx);
    const int SingleRowWidth = SingleButtonWidth * 3 + metrics.ButtonGap * 2;
    const int VisibleButtonWidth = showExtraButtons
        ? ButtonGridWidth : SingleRowWidth;

    const unsigned HeaderId = IDT_OVERWRITE_HEADER;
    const unsigned QuestionBeginId = IDT_OVERWRITE_QUESTION_BEGIN;
    const unsigned QuestionEndId = IDT_OVERWRITE_QUESTION_END;
    const unsigned OldInfoId = IDT_OVERWRITE_OLD_FILE_SIZE_TIME;
    const unsigned NewInfoId = IDT_OVERWRITE_NEW_FILE_SIZE_TIME;

    SetWrappedStaticStyle(::GetDlgItem(dialog, HeaderId));
    SetWrappedStaticStyle(::GetDlgItem(dialog, QuestionBeginId));
    SetWrappedStaticStyle(::GetDlgItem(dialog, QuestionEndId));
    SetWrappedStaticStyle(::GetDlgItem(dialog, OldInfoId));
    SetWrappedStaticStyle(::GetDlgItem(dialog, NewInfoId));

    const std::wstring OldInfoText = GetWindowTextString(
        ::GetDlgItem(dialog, OldInfoId));
    const std::wstring NewInfoText = GetWindowTextString(
        ::GetDlgItem(dialog, NewInfoId));
    const int LongestFileLineWidth = (std::max)(
        MeasureLongestControlLineWidth(dc, ::GetDlgItem(dialog, OldInfoId),
            font, OldInfoText),
        MeasureLongestControlLineWidth(dc, ::GetDlgItem(dialog, NewInfoId),
            font, NewInfoText));

    RECT client = {};
    ::GetClientRect(dialog, &client);
    const int CurrentWidth = client.right - client.left;
    const int CurrentContentWidth = (std::max)(
        1, CurrentWidth - metrics.Margin * 2);
    const int MinimumContentWidth = (std::max)(
        VisibleButtonWidth, CurrentContentWidth);
    const int PreferredFileTextWidth = (std::max)(
        CurrentContentWidth - metrics.IconColumn - metrics.Gap,
        LongestFileLineWidth);
    int Width = (std::max)(CurrentWidth,
        (std::max)(MinimumContentWidth,
            PreferredFileTextWidth + metrics.IconColumn + metrics.Gap) +
        metrics.Margin * 2);

    // Keep the width content-driven as well. The final positioning pass moves
    // an oversized window into view without shrinking its client area.
    const int ContentWidth = (std::max)(
        1, Width - metrics.Margin * 2);
    const int FileTextWidth = (std::max)(
        60, ContentWidth - metrics.IconColumn - metrics.Gap);

    // Explicitly break long path/name segments. Static controls truncate a
    // single unbreakable word even when their height is sufficient.
    const std::wstring WrappedOldInfo = WrapTextForWidth(
        dc, OldInfoText, FileTextWidth);
    const std::wstring WrappedNewInfo = WrapTextForWidth(
        dc, NewInfoText, FileTextWidth);
    ::SetWindowTextW(::GetDlgItem(dialog, OldInfoId),
        WrappedOldInfo.c_str());
    ::SetWindowTextW(::GetDlgItem(dialog, NewInfoId),
        WrappedNewInfo.c_str());

    const int HeaderHeight = MeasureTextHeight(dc,
        ::GetDlgItem(dialog, HeaderId), font,
        GetWindowTextString(::GetDlgItem(dialog, HeaderId)),
        ContentWidth, 1, metrics);
    const int QuestionBeginHeight = MeasureTextHeight(dc,
        ::GetDlgItem(dialog, QuestionBeginId), font,
        GetWindowTextString(::GetDlgItem(dialog, QuestionBeginId)),
        ContentWidth, 1, metrics);
    const int QuestionEndHeight = MeasureTextHeight(dc,
        ::GetDlgItem(dialog, QuestionEndId), font,
        GetWindowTextString(::GetDlgItem(dialog, QuestionEndId)),
        ContentWidth, 1, metrics);
    const int OldInfoHeight = MeasureTextHeight(dc,
        ::GetDlgItem(dialog, OldInfoId), font, WrappedOldInfo,
        FileTextWidth, 1, metrics);
    const int NewInfoHeight = MeasureTextHeight(dc,
        ::GetDlgItem(dialog, NewInfoId), font, WrappedNewInfo,
        FileTextWidth, 1, metrics);

    const int IconSize = 24;
    const int TextLeft = metrics.Margin + metrics.IconColumn + metrics.Gap;

    int y = metrics.Margin;
    SetControlRect(dialog, HeaderId, metrics.Margin, y,
        ContentWidth, HeaderHeight);
    y += HeaderHeight + metrics.Gap;
    SetControlRect(dialog, QuestionBeginId, metrics.Margin, y,
        ContentWidth, QuestionBeginHeight);
    y += QuestionBeginHeight + metrics.Gap;

    const int OldIconY = y + (std::max)(0, (OldInfoHeight - IconSize * 2) / 2);
    SetControlRect(dialog, IDI_OVERWRITE_OLD_FILE,
        metrics.Margin, OldIconY, IconSize, IconSize);
    SetControlRect(dialog, IDI_OVERWRITE_OLD_FILE_2,
        metrics.Margin, OldIconY + IconSize, IconSize, IconSize);
    SetControlRect(dialog, OldInfoId, TextLeft, y,
        FileTextWidth, OldInfoHeight);
    y += OldInfoHeight + metrics.InfoGap;

    SetControlRect(dialog, QuestionEndId, metrics.Margin, y,
        ContentWidth, QuestionEndHeight);
    y += QuestionEndHeight + metrics.InfoGap;

    const int NewIconY = y + (std::max)(0, (NewInfoHeight - IconSize * 2) / 2);
    SetControlRect(dialog, IDI_OVERWRITE_NEW_FILE,
        metrics.Margin, NewIconY, IconSize, IconSize);
    SetControlRect(dialog, IDI_OVERWRITE_NEW_FILE_2,
        metrics.Margin, NewIconY + IconSize, IconSize, IconSize);
    SetControlRect(dialog, NewInfoId, TextLeft, y,
        FileTextWidth, NewInfoHeight);
    y += NewInfoHeight + metrics.Gap * 2 + 1;

    if (showExtraButtons)
    {
      const int ButtonX = Width - metrics.Margin - ButtonGridWidth;
      // Row 1: Yes / Yes to All / Auto Rename
      SetControlRect(dialog, IDYES, ButtonX, y,
          Column12Width, metrics.ButtonHeight);
      SetControlRect(dialog, IDB_YES_TO_ALL,
          ButtonX + Column12Width + metrics.ButtonGap, y,
          Column12Width, metrics.ButtonHeight);
      SetControlRect(dialog, IDB_AUTO_RENAME,
          ButtonX + (Column12Width + metrics.ButtonGap) * 2, y,
          Column3Width, metrics.ButtonHeight);
      y += metrics.ButtonHeight + metrics.Gap;

      // Row 2: No / No to All / Cancel. Column x coordinates are identical.
      SetControlRect(dialog, IDNO, ButtonX, y,
          Column12Width, metrics.ButtonHeight);
      SetControlRect(dialog, IDB_NO_TO_ALL,
          ButtonX + Column12Width + metrics.ButtonGap, y,
          Column12Width, metrics.ButtonHeight);
      SetControlRect(dialog, IDCANCEL,
          ButtonX + (Column12Width + metrics.ButtonGap) * 2, y,
          Column3Width, metrics.ButtonHeight);
      y += metrics.ButtonHeight;
    }
    else
    {
      const int ButtonX = Width - metrics.Margin - SingleRowWidth;
      SetControlRect(dialog, IDYES, ButtonX, y,
          SingleButtonWidth, metrics.ButtonHeight);
      SetControlRect(dialog, IDNO,
          ButtonX + SingleButtonWidth + metrics.ButtonGap, y,
          SingleButtonWidth, metrics.ButtonHeight);
      SetControlRect(dialog, IDCANCEL,
          ButtonX + (SingleButtonWidth + metrics.ButtonGap) * 2, y,
          SingleButtonWidth, metrics.ButtonHeight);
      y += metrics.ButtonHeight;
    }

    const int Height = y + metrics.Margin;

    if (dc && oldFont)
      ::SelectObject(dc, oldFont);
    if (dc)
      ::ReleaseDC(dialog, dc);

    RECT windowRect = {};
    RECT clientRect = {};
    if (::GetWindowRect(dialog, &windowRect) &&
        ::GetClientRect(dialog, &clientRect))
    {
      const int FrameWidth = (windowRect.right - windowRect.left) -
          (clientRect.right - clientRect.left);
      const int FrameHeight = (windowRect.bottom - windowRect.top) -
          (clientRect.bottom - clientRect.top);
      ::SetWindowPos(dialog, nullptr, 0, 0,
          Width + FrameWidth, Height + FrameHeight,
          SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    ::InvalidateRect(dialog, nullptr, TRUE);
  }

  // **************** SSS Fluent Overwrite Dialog Start ****************
  // Visual refresh for the Win32 overwrite dialog: Segoe UI Variable fonts,
  // owner-drawn Fluent-style buttons (anti-aliased 4px corners, hover/pressed
  // layers, accent default button) aligned with the XAML dark palette.
  // The shared K7User dark-mode pipe is left untouched; everything here is
  // local to this dialog and falls back to the stock look when the font is
  // missing or the system runs in light mode.

  struct SssFluentPalette
  {
    COLORREF ButtonFace;
    COLORREF ButtonBorder;
    COLORREF ButtonText;
    COLORREF ButtonFaceHover;
    COLORREF ButtonFacePressed;
    COLORREF ButtonTextPressed;
    COLORREF FocusRing;
  };

  static const SssFluentPalette &SssGetPalette(bool darkMode)
  {
    static const SssFluentPalette dark =
    {
      RGB(0x2D, 0x2D, 0x2D), // rest face (XAML ControlFillColorSecondary equivalent)
      RGB(0x3D, 0x3D, 0x3D), // rest border
      RGB(0xFF, 0xFF, 0xFF), // text
      RGB(0x38, 0x38, 0x38), // hover face
      RGB(0x29, 0x29, 0x29), // pressed face
      RGB(0x9B, 0x9B, 0x9B), // pressed text (XAML secondary text level)
      RGB(0xFF, 0xFF, 0xFF), // focus ring
    };
    static const SssFluentPalette light =
    {
      RGB(0xFD, 0xFD, 0xFD),
      RGB(0xDD, 0xDD, 0xDD),
      RGB(0x1A, 0x1A, 0x1A),
      RGB(0xF6, 0xF6, 0xF6),
      RGB(0xF0, 0xF0, 0xF0),
      RGB(0x60, 0x60, 0x60),
      RGB(0x00, 0x00, 0x00),
    };
    return darkMode ? dark : light;
  }

  static bool SssIsDarkMode()
  {
    // K7User exposes the exact state its dark-mode pipe uses.
    return ::K7UserShouldAppsUseDarkMode() != FALSE;
  }

  static COLORREF SssGetAccentColor()
  {
    // AccentColorMenu already stores the value in COLORREF byte layout
    // (0xAABBGGRR): the low three bytes are BB GG RR. Use it as-is.
    DWORD value = 0;
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Accent",
        0, KEY_READ, &key) == ERROR_SUCCESS)
    {
      DWORD size = sizeof(value);
      ::RegQueryValueExW(key, L"AccentColorMenu", nullptr, nullptr,
          reinterpret_cast<LPBYTE>(&value), &size);
      ::RegCloseKey(key);
    }
    const COLORREF accent = value & 0x00FFFFFF;
    if (accent != 0)
      return accent;
    return RGB(0x00, 0x78, 0xD4); // Windows default blue accent fallback
  }

  static COLORREF SssAdjustColorBrightness(COLORREF color, int delta)
  {
    auto clamp = [](int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); };
    return RGB(
        clamp(GetRValue(color) + delta),
        clamp(GetGValue(color) + delta),
        clamp(GetBValue(color) + delta));
  }

  static bool SssIsColorLight(COLORREF color)
  {
    const int r = GetRValue(color);
    const int g = GetGValue(color);
    const int b = GetBValue(color);
    return (r * 299 + g * 587 + b * 114) > 127 * 1000;
  }

  struct SssFontEnumContext
  {
    const wchar_t *Name;
    bool Found;
  };

  static int CALLBACK SssFontEnumProc(
      const LOGFONTW *logFont, const TEXTMETRICW *, DWORD, LPARAM lParam)
  {
    SssFontEnumContext *context =
        reinterpret_cast<SssFontEnumContext *>(lParam);
    if (::lstrcmpiW(logFont->lfFaceName, context->Name) == 0)
    {
      context->Found = true;
      return 0; // stop enumeration
    }
    return 1;
  }

  static bool SssFontFamilyExists(const wchar_t *familyName)
  {
    SssFontEnumContext context = { familyName, false };
    HDC dc = ::GetDC(nullptr);
    if (!dc)
      return false;
    LOGFONTW lf = {};
    lf.lfCharSet = DEFAULT_CHARSET;
    ::EnumFontFamiliesExW(dc, &lf, SssFontEnumProc,
        reinterpret_cast<LPARAM>(&context), 0);
    ::ReleaseDC(nullptr, dc);
    return context.Found;
  }

  // Cached for the process lifetime: the dialog is short-lived and 7zG exits
  // right after the archive operation.
  static unsigned SssGetDialogFontSize()
  {
    // Same source as the shared dialog pipe (Font Size dialog setting).
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
    return pt ? static_cast<unsigned>(pt) : 9u;
  }

  static HFONT SssGetBodyFont(unsigned pointSize, bool displayVariant)
  {
    static HFONT cachedText = nullptr;
    static HFONT cachedDisplay = nullptr;
    HFONT &cached = displayVariant ? cachedDisplay : cachedText;
    if (cached)
      return cached;

    const wchar_t *family = displayVariant ? L"Segoe UI Variable Display"
                                           : L"Segoe UI Variable Text";
    if (!SssFontFamilyExists(family))
      return nullptr; // keep the stock dialog font

    HDC dc = ::GetDC(nullptr);
    const UINT dpi = dc ? static_cast<UINT>(::GetDeviceCaps(dc, LOGPIXELSY))
                        : 96;
    if (dc)
      ::ReleaseDC(nullptr, dc);

    LOGFONTW lf = {};
    lf.lfHeight = -::MulDiv(static_cast<int>(pointSize),
        static_cast<int>(dpi), 72);
    lf.lfWeight = displayVariant ? FW_SEMIBOLD : FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    ::lstrcpynW(lf.lfFaceName, family, LF_FACESIZE);
    cached = ::CreateFontIndirectW(&lf);
    return cached;
  }

  struct SssFluentButtonState
  {
    bool Hover = false;
    bool Pressed = false;
    bool KeyboardFocus = false;
    bool Accent = false;
  };

  static void SssInitGdiPlusOnce()
  {
    static bool initialized = false;
    if (initialized)
      return;
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) == Gdiplus::Ok)
      initialized = true;
  }

  static void SssDrawFluentButton(HDC dc, HWND button)
  {
    SssInitGdiPlusOnce();

    RECT rect = {};
    ::GetClientRect(button, &rect);

    // Clear the whole client area first: the band between the face and the
    // control edge (focus-ring zone) must be repainted with the dialog
    // background on every pass, otherwise a stale ring survives focus moves.
    {
      HBRUSH background = reinterpret_cast<HBRUSH>(::SendMessageW(
          ::GetParent(button), WM_CTLCOLORBTN,
          reinterpret_cast<WPARAM>(dc),
          reinterpret_cast<LPARAM>(button)));
      HBRUSH localBackground = nullptr;
      if (!background)
      {
        // Fallback matching the K7User dialog background.
        localBackground = ::CreateSolidBrush(
            SssIsDarkMode() ? RGB(0, 0, 0) : RGB(255, 255, 255));
        background = localBackground;
      }
      if (background)
        ::FillRect(dc, &rect, background);
      if (localBackground)
        ::DeleteObject(localBackground);
    }

    const bool darkMode = SssIsDarkMode();
    const SssFluentPalette &palette = SssGetPalette(darkMode);
    const SssFluentButtonState *state =
        reinterpret_cast<const SssFluentButtonState *>(
            ::GetPropW(button, L"SSS_FluentButton"));
    const bool accent = state && state->Accent;
    const bool hot = state && state->Hover;
    const bool pressed = state && state->Pressed;
    const bool focused = state && state->KeyboardFocus;
    const bool enabled = (button == nullptr) ||
        (::GetWindowLongPtrW(button, GWL_STYLE) & WS_DISABLED) == 0;

    COLORREF face = accent ? SssGetAccentColor() : palette.ButtonFace;
    COLORREF border = accent
        ? SssAdjustColorBrightness(SssGetAccentColor(), -24)
        : palette.ButtonBorder;
    COLORREF text = accent
        ? (SssIsColorLight(face) ? RGB(0, 0, 0) : RGB(0xFF, 0xFF, 0xFF))
        : palette.ButtonText;
    if (!enabled)
    {
      face = darkMode ? RGB(0x1F, 0x1F, 0x1F) : RGB(0xF3, 0xF3, 0xF3);
      border = darkMode ? RGB(0x33, 0x33, 0x33) : RGB(0xE0, 0xE0, 0xE0);
      text = darkMode ? RGB(0x5C, 0x5C, 0x5C) : RGB(0x9D, 0x9D, 0x9D);
    }
    else if (pressed)
    {
      face = accent ? SssAdjustColorBrightness(face, -32)
                    : palette.ButtonFacePressed;
      text = accent ? text : palette.ButtonTextPressed;
    }
    else if (hot)
    {
      face = accent ? SssAdjustColorBrightness(face, 16)
                    : palette.ButtonFaceHover;
    }

    const UINT dpi = ::GetDeviceCaps(dc, LOGPIXELSY);
    // 8px radius for a rounder, Win11 2H24-style look.
    const float radius = static_cast<float>(
        ::MulDiv(8, static_cast<int>(dpi), 96));
    const float width = static_cast<float>(rect.right - rect.left);
    const float height = static_cast<float>(rect.bottom - rect.top);

    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    const Gdiplus::RectF bounds(
        3.0f, 3.0f, (std::max)(0.0f, width - 6.0f),
        (std::max)(0.0f, height - 6.0f));
    Gdiplus::GraphicsPath path;
    path.AddArc(bounds.X, bounds.Y, radius * 2, radius * 2, 180, 90);
    path.AddArc(bounds.X + bounds.Width - radius * 2, bounds.Y,
        radius * 2, radius * 2, 270, 90);
    path.AddArc(bounds.X + bounds.Width - radius * 2,
        bounds.Y + bounds.Height - radius * 2, radius * 2, radius * 2, 0, 90);
    path.AddArc(bounds.X, bounds.Y + bounds.Height - radius * 2,
        radius * 2, radius * 2, 90, 90);
    path.CloseFigure();

    Gdiplus::SolidBrush faceBrush(Gdiplus::Color(
        255, GetRValue(face), GetGValue(face), GetBValue(face)));
    graphics.FillPath(&faceBrush, &path);

    Gdiplus::Pen borderPen(Gdiplus::Color(
        255, GetRValue(border), GetGValue(border), GetBValue(border)), 1.0f);
    graphics.DrawPath(&borderPen, &path);

    // GDI text for ClearType quality on top of the GDI+ background.
    HDC textDc = graphics.GetHDC();
    ::SetBkMode(textDc, TRANSPARENT);
    ::SetTextColor(textDc, text);
    HFONT font = reinterpret_cast<HFONT>(
        ::SendMessageW(button, WM_GETFONT, 0, 0));
    HFONT oldFont = font
        ? reinterpret_cast<HFONT>(::SelectObject(textDc, font)) : nullptr;

    wchar_t caption[128] = {};
    ::GetWindowTextW(button, caption, 128);
    // No DT_NOPREFIX: GDI renders the '&' mnemonic the standard way
    // (e.g. "是(&Y)" shows as 是(Y) with an underlined Y).
    ::DrawTextW(textDc, caption, -1, &rect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (oldFont)
      ::SelectObject(textDc, oldFont);
    graphics.ReleaseHDC(textDc);

    if (focused && enabled)
    {
      // Focus ring hugs the outside of the button face, wrapping its
      // contour so the selected button reads as slightly larger.
      const Gdiplus::RectF inner(
          0.9f, 0.9f, (std::max)(0.0f, width - 1.8f),
          (std::max)(0.0f, height - 1.8f));
      Gdiplus::GraphicsPath focusPath;
      focusPath.AddArc(inner.X, inner.Y, radius * 2, radius * 2, 180, 90);
      focusPath.AddArc(inner.X + inner.Width - radius * 2, inner.Y,
          radius * 2, radius * 2, 270, 90);
      focusPath.AddArc(inner.X + inner.Width - radius * 2,
          inner.Y + inner.Height - radius * 2, radius * 2, radius * 2, 0, 90);
      focusPath.AddArc(inner.X, inner.Y + inner.Height - radius * 2,
          radius * 2, radius * 2, 90, 90);
      focusPath.CloseFigure();
      Gdiplus::Pen focusPen(Gdiplus::Color(
          255, GetRValue(palette.FocusRing), GetGValue(palette.FocusRing),
          GetBValue(palette.FocusRing)), 2.2f);
      graphics.DrawPath(&focusPen, &focusPath);
    }
  }

  static LRESULT CALLBACK SssFluentButtonProc(
      HWND button,
      UINT message,
      WPARAM wParam,
      LPARAM lParam,
      UINT_PTR subclassId,
      DWORD_PTR /* refData */)
  {
    SssFluentButtonState *state =
        reinterpret_cast<SssFluentButtonState *>(
            ::GetPropW(button, L"SSS_FluentButton"));

    switch (message)
    {
      case WM_PAINT:
      case WM_PRINTCLIENT:
      {
        if (state)
        {
          if (message == WM_PAINT)
          {
            PAINTSTRUCT paint = {};
            HDC dc = ::BeginPaint(button, &paint);
            if (dc)
              SssDrawFluentButton(dc, button);
            ::EndPaint(button, &paint);
          }
          else
          {
            SssDrawFluentButton(reinterpret_cast<HDC>(wParam), button);
          }
          return 0;
        }
        break;
      }
      case WM_MOUSEMOVE:
      {
        if (state && !state->Hover)
        {
          TRACKMOUSEEVENT track = { sizeof(track), TME_LEAVE, button, 0 };
          ::TrackMouseEvent(&track);
          state->Hover = true;
          ::InvalidateRect(button, nullptr, FALSE);
        }
        break;
      }
      case WM_MOUSELEAVE:
      {
        if (state && state->Hover)
        {
          state->Hover = false;
          ::InvalidateRect(button, nullptr, FALSE);
        }
        break;
      }
      case WM_LBUTTONDOWN:
      case WM_LBUTTONDBLCLK:
      {
        if (state && !state->Pressed)
        {
          state->Pressed = true;
          ::SetCapture(button);
          ::InvalidateRect(button, nullptr, FALSE);
        }
        break;
      }
      case WM_LBUTTONUP:
      {
        if (state && state->Pressed)
        {
          state->Pressed = false;
          ::InvalidateRect(button, nullptr, FALSE);
        }
        break;
      }
      case WM_CAPTURECHANGED:
      {
        if (state && state->Pressed)
        {
          state->Pressed = false;
          ::InvalidateRect(button, nullptr, FALSE);
        }
        break;
      }
      case WM_KEYDOWN:
      {
        if (state && wParam == VK_SPACE && !state->Pressed)
        {
          state->Pressed = true;
          ::InvalidateRect(button, nullptr, FALSE);
        }
        break;
      }
      case WM_KEYUP:
      {
        if (state && wParam == VK_SPACE && state->Pressed)
        {
          state->Pressed = false;
          ::InvalidateRect(button, nullptr, FALSE);
        }
        break;
      }
      case WM_SETFOCUS:
      case WM_KILLFOCUS:
      {
        if (state)
        {
          state->KeyboardFocus = message == WM_SETFOCUS;
          ::InvalidateRect(button, nullptr, FALSE);
        }
        break;
      }
      case WM_ENABLE:
      {
        ::InvalidateRect(button, nullptr, FALSE);
        break;
      }
      case 0x1601: // BCM_GETIDEALSIZE: feed the layout system our metrics
      {
        if (state && lParam)
        {
          HDC dc = ::GetDC(button);
          HFONT font = reinterpret_cast<HFONT>(
              ::SendMessageW(button, WM_GETFONT, 0, 0));
          HFONT oldFont = font
              ? reinterpret_cast<HFONT>(::SelectObject(dc, font)) : nullptr;
          wchar_t caption[128] = {};
          ::GetWindowTextW(button, caption, 128);
          // Strip '&' mnemonic markers for measurement.
          std::wstring text;
          for (const wchar_t *p = caption; *p; ++p)
          {
            if (*p == L'&' && p[1] == L'&') { text += L'&'; ++p; continue; }
            if (*p == L'&') continue;
            text += *p;
          }
          RECT measure = { 0, 0, 0, 0 };
          ::DrawTextW(dc, text.c_str(), static_cast<int>(text.size()),
              &measure, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
          const UINT dpi = ::GetDeviceCaps(dc, LOGPIXELSY);
          if (oldFont)
            ::SelectObject(dc, oldFont);
          ::ReleaseDC(button, dc);

          SIZE *size = reinterpret_cast<SIZE *>(lParam);
          const int padH = ::MulDiv(14, static_cast<int>(dpi), 96);
          const int padV = ::MulDiv(8, static_cast<int>(dpi), 96);
          size->cx = (measure.right - measure.left) + padH * 2;
          size->cy = (measure.bottom - measure.top) + padV * 2;
          return TRUE;
        }
        break;
      }
      case WM_DESTROY:
      {
        if (state)
        {
          delete state;
          ::RemovePropW(button, L"SSS_FluentButton");
        }
        ::RemoveWindowSubclass(button, SssFluentButtonProc, subclassId);
        break;
      }
      default:
        break;
    }

    return ::DefSubclassProc(button, message, wParam, lParam);
  }

  static BOOL CALLBACK SssSetFontProc(HWND control, LPARAM lParam)
  {
    wchar_t className[32] = {};
    ::GetClassNameW(control, className, 32);
    // Leave buttons alone: they get the font in the dedicated pass below.
    if (::lstrcmpiW(className, L"Button") == 0)
      return TRUE;
    ::SendMessageW(control, WM_SETFONT, lParam, TRUE);
    return TRUE;
  }

  static void SssApplyFluentLook(HWND dialog)
  {
    const unsigned buttonIds[] =
    {
      IDYES, IDNO, IDCANCEL,
      IDB_YES_TO_ALL, IDB_NO_TO_ALL, IDB_AUTO_RENAME
    };

    // Body font for every control, display font for the header. When the
    // Variable font family is missing, both stay null and nothing changes.
    // The size follows the user's Font Size setting (default 9pt).
    const unsigned basePt = SssGetDialogFontSize();
    HFONT bodyFont = SssGetBodyFont(basePt, false);
    HFONT headerFont = SssGetBodyFont(
        static_cast<unsigned>(::MulDiv(static_cast<int>(basePt), 4, 3)),
        true);
    if (bodyFont)
    {
      ::SendMessageW(dialog, WM_SETFONT,
          reinterpret_cast<WPARAM>(bodyFont), TRUE);
      ::EnumChildWindows(dialog, SssSetFontProc,
          reinterpret_cast<LPARAM>(bodyFont));
    }
    if (headerFont)
      ::SendMessageW(::GetDlgItem(dialog, IDT_OVERWRITE_HEADER),
          WM_SETFONT, reinterpret_cast<WPARAM>(headerFont), TRUE);

    for (unsigned id : buttonIds)
    {
      HWND button = ::GetDlgItem(dialog, static_cast<int>(id));
      if (!button)
        continue;
      if (!::GetPropW(button, L"SSS_FluentButton"))
      {
        SssFluentButtonState *state = new SssFluentButtonState();
        // Uniform styling: all buttons share the standard Fluent look,
        // matching the XAML ContentDialog (no accent default button).
        state->Accent = false;
        ::SetPropW(button, L"SSS_FluentButton", state);
        ::SetWindowSubclass(button, SssFluentButtonProc, 1, 0);
        // Body font for the button text.
        if (bodyFont)
          ::SendMessageW(button, WM_SETFONT,
              reinterpret_cast<WPARAM>(bodyFont), TRUE);
      }
    }
  }
  // **************** SSS Fluent Overwrite Dialog End ****************
}

using namespace NWindows;

#ifdef Z7_LANG
static const UInt32 kLangIDs[] =
{
  IDT_OVERWRITE_HEADER,
  IDT_OVERWRITE_QUESTION_BEGIN,
  IDT_OVERWRITE_QUESTION_END,
  IDB_YES_TO_ALL,
  IDB_NO_TO_ALL,
  IDB_AUTO_RENAME
};
#endif

static const unsigned kCurrentFileNameSizeLimit = 72;

void COverwriteDialog::ReduceString(UString &s)
{
  const unsigned size =
#ifdef UNDER_CE
      !_isBig ? 30 : // kCurrentFileNameSizeLimit2
#endif
      kCurrentFileNameSizeLimit;

  if (s.Len() > size)
  {
    s.Delete(size / 2, s.Len() - size);
    s.Insert(size / 2, L" ... ");
  }
  if (!s.IsEmpty() && s.Back() == ' ')
  {
    // s += (wchar_t)(0x2423); // visible space
    s.InsertAtFront(L'\"');
    s.Add_Char('\"');
  }
}


void COverwriteDialog::SetItemIcon(unsigned iconID, HICON hIcon)
{
  NControl::CStatic staticContol;
  staticContol.Attach(GetItem(iconID));
  hIcon = staticContol.SetIcon(hIcon);
  if (hIcon)
    DestroyIcon(hIcon);
}

void AddSizeValue(UString &s, UInt64 value);
void AddSizeValue(UString &s, UInt64 value)
{
  {
    wchar_t sz[32];
    ConvertUInt64ToString(value, sz);
    s += MyFormatNew(IDS_FILE_SIZE, sz);
  }
  if (value >= (1 << 10))
  {
    char c;
          if (value >= ((UInt64)10 << 30)) { value >>= 30; c = 'G'; }
    else  if (value >=         (10 << 20)) { value >>= 20; c = 'M'; }
    else                                   { value >>= 10; c = 'K'; }
    s += " : ";
    s.Add_UInt64(value);
    s.Add_Space();
    s.Add_Char(c);
    s += "iB";
  }
}


void COverwriteDialog::SetFileInfoControl(
    const NOverwriteDialog::CFileInfo &fileInfo,
    unsigned textID,
    unsigned iconID,
    unsigned iconID_2)
{
  {
    const UString &path = fileInfo.Path;
    const int slashPos = path.ReverseFind_PathSepar();
    UString s = path.Left((unsigned)(slashPos + 1));
    ReduceString(s);
    s.Add_LF();
    {
      UString s2 = path.Ptr((unsigned)(slashPos + 1));
      ReduceString(s2);
      s += s2;
    }
    if (fileInfo.Size_IsDefined)
    {
      s.Add_LF();
      AddSizeValue(s, fileInfo.Size);
    }
    if (fileInfo.Time_IsDefined)
    {
      s.Add_LF();
      AddLangString(s, IDS_PROP_MTIME);
      s += ": ";
      char t[64];
      ConvertUtcFileTimeToString(fileInfo.Time, t);
      s += t;
    }
    SetItemText(textID, s);
  }
/*
  SHGetFileInfo():
    DOCs: If uFlags does not contain SHGFI_EXETYPE or SHGFI_SYSICONINDEX,
          the return value is nonzero if successful, or zero otherwise.
    We don't use SHGFI_EXETYPE or SHGFI_SYSICONINDEX here.
  win10: we call with SHGFI_ICON flag set.
    it returns 0: if error : (shFileInfo::*) members are not set.
    it returns non_0, if successful, and retrieve:
      { shFileInfo.hIcon != NULL : the handle to icon (must be destroyed by our code)
        shFileInfo.iIcon is index of the icon image within the system image list.
      }
  Note:
    If we send path to ".exe" file,
    SHGFI_USEFILEATTRIBUTES flag is ignored, and it tries to open file.
    and return icon from that exe file.
    So we still need to reduce path, if want to get raw icon of exe file.
    
  if (name.Len() >= MAX_PATH))
  {
    it can return:
      return 0.
      return 1 and:
        { shFileInfo.hIcon != NULL : is some default icon for file
          shFileInfo.iIcon == 0
        }
    return results (0 or 1) can depend from:
      - unicode/non-unicode
      - (SHGFI_USEFILEATTRIBUTES) flag
      - exact file extension (.exe).
  }
*/
  int iconIndex = -1;
  for (unsigned i = 0; i < 2; i++)
  {
    CSysString name = GetSystemString(fileInfo.Path);
    if (i != 0)
    {
      if (!fileInfo.Is_FileSystemFile)
        break;
      if (name.Len() < 4 ||
          (!StringsAreEqualNoCase_Ascii(name.RightPtr(4), ".exe") &&
           !StringsAreEqualNoCase_Ascii(name.RightPtr(4), ".ico")))
        break;
      // if path for ".exe" file is long, it returns default icon (shFileInfo.iIcon == 0).
      // We don't want to show that default icon.
      // But we will check for default icon later instead of MAX_PATH check here.
      // if (name.Len() >= MAX_PATH) break; // optional
    }
    else
    {
      // we need only file extension with dot
      const int separ = name.ReverseFind_PathSepar();
      name.DeleteFrontal((unsigned)(separ + 1));
      // if (name.Len() >= MAX_PATH)
      {
        const int dot = name.ReverseFind_Dot();
        if (dot >= 0)
          name.DeleteFrontal((unsigned)dot);
        // else name.Empty(); to set default name below
      }
      // name.Empty(); // for debug
    }

    if (name.IsEmpty())
    {
      // If we send empty name, SHGetFileInfo() returns some strange icon.
      // So we use common dummy name without extension,
      // and SHGetFileInfo() will return default icon (iIcon == 0)
      name = "__file__";
    }

    DWORD attrib = FILE_ATTRIBUTE_ARCHIVE;
    if (fileInfo.Is_FileSystemFile)
    {
      NFile::NFind::CFileInfo fi;
      if (fi.Find(us2fs(fileInfo.Path)) && !fi.IsAltStream && !fi.IsDir())
        attrib = fi.Attrib;
    }

    SHFILEINFO shFileInfo;
    // ZeroMemory(&shFileInfo, sizeof(shFileInfo)); // optional
    shFileInfo.hIcon = NULL; // optional
    shFileInfo.iIcon = -1;   // optional
    // memset(&shFileInfo, 1, sizeof(shFileInfo)); // for debug
    const DWORD_PTR res = ::SHGetFileInfo(name, attrib,
        &shFileInfo, sizeof(shFileInfo),
        SHGFI_ICON | SHGFI_LARGEICON | SHGFI_SHELLICONSIZE |
        // (i == 0 ? SHGFI_USEFILEATTRIBUTES : 0)
        SHGFI_USEFILEATTRIBUTES
        // we use SHGFI_USEFILEATTRIBUTES for second icon, because
        // it still returns real icon from exe files
        );
    if (res && shFileInfo.hIcon)
    {
      // we don't show second icon, if icon index (iIcon) is same
      // as first icon index of first shown icon (exe file without icon)
      if (   shFileInfo.iIcon >= 0
          && shFileInfo.iIcon != iconIndex
          && (shFileInfo.iIcon != 0 || i == 0)) // we don't want default icon for second icon
      {
        iconIndex = shFileInfo.iIcon;
        SetItemIcon(i == 0 ? iconID : iconID_2, shFileInfo.hIcon);
      }
      else
        DestroyIcon(shFileInfo.hIcon);
    }
  }
}



bool COverwriteDialog::OnInit()
{
  #ifdef Z7_LANG
  LangSetWindowText(*this, IDD_OVERWRITE);
  LangSetDlgItems(*this, kLangIDs, Z7_ARRAY_SIZE(kLangIDs));
  #endif
  SetFileInfoControl(OldFileInfo,
      IDT_OVERWRITE_OLD_FILE_SIZE_TIME,
      IDI_OVERWRITE_OLD_FILE,
      IDI_OVERWRITE_OLD_FILE_2);
  SetFileInfoControl(NewFileInfo,
      IDT_OVERWRITE_NEW_FILE_SIZE_TIME,
      IDI_OVERWRITE_NEW_FILE,
      IDI_OVERWRITE_NEW_FILE_2);
  if (!ShowExtraButtons)
  {
    HideItem(IDB_YES_TO_ALL);
    HideItem(IDB_NO_TO_ALL);
    HideItem(IDB_AUTO_RENAME);
  }

  // SSS: Fluent look (fonts + owner-drawn buttons) must be applied before
  // the layout pass, which reads BCM_GETIDEALSIZE intercepted by the
  // button subclass.
  SssApplyFluentLook(*this);

  LayoutOverwriteDialog(*this, ShowExtraButtons);
  PositionOverwriteDialog(*this);

  if (DefaultButton_is_NO)
  {
    PostMsg(DM_SETDEFID, IDNO);
    HWND h = GetItem(IDNO);
    PostMsg(WM_NEXTDLGCTL, (WPARAM)h, TRUE);
    // ::SetFocus(h);
  }

  return CModalDialog::OnInit();
}

bool COverwriteDialog::OnDestroy()
{
  SetItemIcon(IDI_OVERWRITE_OLD_FILE, NULL);
  SetItemIcon(IDI_OVERWRITE_OLD_FILE_2, NULL);
  SetItemIcon(IDI_OVERWRITE_NEW_FILE, NULL);
  SetItemIcon(IDI_OVERWRITE_NEW_FILE_2, NULL);
  return false; // we return (false) to perform default dialog operation
}

bool COverwriteDialog::OnButtonClicked(unsigned buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDYES:
    case IDNO:
    case IDB_YES_TO_ALL:
    case IDB_NO_TO_ALL:
    case IDB_AUTO_RENAME:
      End((INT_PTR)buttonID);
      return true;
  }
  return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
}
