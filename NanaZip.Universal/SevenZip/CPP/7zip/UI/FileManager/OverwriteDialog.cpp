// OverwriteDialog.cpp

#include "StdAfx.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/FileFind.h"
#include "../../../Windows/PropVariantConv.h"
#include "../../../Windows/ResourceString.h"

#include "../../../Windows/Control/Static.h"

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
