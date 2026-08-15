#include "pch.h"
#include "SplitPage.h"
#if __has_include("SplitPage.g.cpp")
#include "SplitPage.g.cpp"
#endif

#include "NanaZip.Modern.h"
#include <K7User.h>

#include <shlobj.h>

#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.h>

#include <string>

namespace winrt::NanaZip::Modern::implementation
{
    // The preset volume sizes, matching the Win32 SplitDialog
    // (SplitUtils.cpp AddVolumeItems). Kept here as display text only; the
    // parsing rules stay on the 7-Zip side (ParseVolumeSizes via callback).
    static const wchar_t* const k_VolumePresets[] =
    {
        L"10M",
        L"100M",
        L"1000M",
        L"650M - CD",
        L"700M - CD",
        L"4092M - FAT",
        L"4480M - DVD",
        L"8128M - DVD DL",
        L"23040M - BD"
    };

    SplitPage::SplitPage(
        _In_opt_ HWND WindowHandle,
        _In_ PK7_SPLIT_DIALOG_CONTEXT Context) :
        m_WindowHandle(WindowHandle),
        m_Context(Context),
        m_OkClicked(false)
    {
        this->Unloaded({ this, &SplitPage::OnUnloaded });
    }

    void SplitPage::InitializeComponent()
    {
        SplitPageT::InitializeComponent();
    }

    winrt::hstring SplitPage::Res(
        UINT32 ResourceId,
        LPCWSTR Fallback)
    {
        LPCWSTR Content = ::K7ModernGetLegacyStringResource(ResourceId);
        if (Content && Content[0] != L'\0')
        {
            return winrt::hstring(Content);
        }
        return winrt::hstring(Fallback ? Fallback : L"");
    }

    winrt::hstring SplitPage::RemoveMnemonic(
        winrt::hstring const& Text)
    {
        std::wstring Result;
        std::wstring Source(Text.c_str());
        Result.reserve(Source.size());
        for (size_t i = 0; i < Source.size(); i++)
        {
            if (Source[i] == L'&')
            {
                if (i + 1 < Source.size() && Source[i + 1] == L'&')
                {
                    Result += L'&';
                    i++;
                }
                continue;
            }
            Result += Source[i];
        }
        return winrt::hstring(Result);
    }

    void SplitPage::ApplyDialogFont(UINT32 Pt)
    {
        double FontSizePx = (Pt == 0) ? 0.0 : (double)Pt * 96.0 / 72.0;
        ApplyFontToTree(*this, FontSizePx);
    }

    void SplitPage::ApplyFontToTree(
        winrt::Windows::UI::Xaml::DependencyObject const& Node,
        double FontSizePx)
    {
        if (FontSizePx > 0.0)
        {
            if (auto Control =
                Node.try_as<winrt::Windows::UI::Xaml::Controls::Control>())
            {
                Control.FontSize(FontSizePx);
            }
            else if (auto TextBlock =
                Node.try_as<winrt::Windows::UI::Xaml::Controls::TextBlock>())
            {
                TextBlock.FontSize(FontSizePx);
            }
        }
        else
        {
            if (auto Control =
                Node.try_as<winrt::Windows::UI::Xaml::Controls::Control>())
            {
                Control.ClearValue(
                    winrt::Windows::UI::Xaml::Controls::Control::FontSizeProperty());
            }
            else if (auto TextBlock =
                Node.try_as<winrt::Windows::UI::Xaml::Controls::TextBlock>())
            {
                TextBlock.ClearValue(
                    winrt::Windows::UI::Xaml::Controls::TextBlock::FontSizeProperty());
            }
        }

        int ChildCount =
            winrt::Windows::UI::Xaml::Media::VisualTreeHelper::GetChildrenCount(Node);
        for (int i = 0; i < ChildCount; i++)
        {
            ApplyFontToTree(
                winrt::Windows::UI::Xaml::Media::VisualTreeHelper::GetChild(Node, i),
                FontSizePx);
        }
    }

    winrt::Windows::Foundation::Size SplitPage::PrepareForShow()
    {
        if (!this->m_Context)
        {
            return winrt::Windows::Foundation::Size(360, 140);
        }

        PK7_SPLIT_DIALOG_CONTEXT Context = this->m_Context;

        // Dialog title: "Split File <source name>" (the Win32 dialog appends
        // the source file name to the static caption with a space).
        {
            std::wstring Title = Res(7300, L"Split File").c_str();
            std::wstring FileName(Context->FileName);
            if (!FileName.empty())
            {
                Title += L" ";
                Title += FileName;
            }
            ::SetWindowTextW(this->m_WindowHandle, Title.c_str());
        }

        SplitToText().Text(RemoveMnemonic(Res(7301, L"Split to:")));
        VolumeText().Text(RemoveMnemonic(Res(7302, L"Split to volumes, bytes:")));
        OkButton().Content(winrt::box_value(RemoveMnemonic(Res(401, L"OK"))));
        CancelButton().Content(winrt::box_value(RemoveMnemonic(Res(402, L"Cancel"))));
        ErrorText().Text(RemoveMnemonic(Res(7307, L"Incorrect volume size")));

        // Initial output path.
        PathCombo().Text(winrt::hstring(Context->DirPath));

        // Add the current path as the first drop-down item: an editable
        // combo whose text does not match any item clears the box when the
        // drop-down opens (same as the extract dialog, which always keeps
        // the current path as its first item).
        if (Context->DirPath[0])
        {
            PathCombo().Items().Append(winrt::box_value(
                winrt::hstring(Context->DirPath)));
        }

        // Guard against the editable combo blanking the path text when its
        // drop-down is opened/closed (same as the extract dialog).
        PathCombo().DropDownOpened(
            { this, &SplitPage::OnPathComboDropDownOpened });
        PathCombo().DropDownClosed(
            { this, &SplitPage::OnPathComboDropDownClosed });

        // Volume-size presets (display text only, matching AddVolumeItems).
        for (size_t i = 0; i < _countof(k_VolumePresets); i++)
        {
            VolumeCombo().Items().Append(winrt::box_value(
                winrt::hstring(k_VolumePresets[i])));
        }
        VolumeCombo().SelectedIndex(0);

        ApplyDialogFont(Context->FontSizeDialog);

        // Measure the content with an infinite constraint so every control
        // reports its natural size (including the longest combo item and the
        // current dialog font size). The caller sizes the window from this
        // before it is shown, so there is no visible resize after the dialog
        // appears. The returned size is the content size without margin.
        winrt::Windows::Foundation::Size Inf(
            100000.0f, 100000.0f);
        this->Measure(Inf);
        winrt::Windows::Foundation::Size Desired = this->DesiredSize();

        // Minimum window size: the measured content (plus a small margin), so
        // dragging the border can never clip the controls. This dialog has no
        // wrap layout, so the minimum is computed once here and written back
        // for the host window subclass (WM_GETMINMAXINFO).
        const UINT Dpi = ::GetDpiForWindow(this->m_WindowHandle);
        const float Scale = (float)Dpi / (float)USER_DEFAULT_SCREEN_DPI;

        int MinClientW = (int)((Desired.Width + 32.0f) * Scale + 0.5f);
        int MinClientH = (int)((Desired.Height + 4.0f) * Scale + 0.5f);
        if (MinClientW < 360) MinClientW = 360;
        if (MinClientH < 110) MinClientH = 110;

        RECT rc = { 0, 0, MinClientW, MinClientH };
        {
            const LONG_PTR Style = ::GetWindowLongPtrW(
                this->m_WindowHandle, GWL_STYLE);
            const LONG_PTR ExStyle = ::GetWindowLongPtrW(
                this->m_WindowHandle, GWL_EXSTYLE);
            ::AdjustWindowRectEx(&rc, (DWORD)Style, FALSE, (DWORD)ExStyle);
        }

        Context->MinTrackW = rc.right - rc.left;
        Context->MinTrackH = rc.bottom - rc.top;

        return Desired;
    }

    void SplitPage::OnPathComboDropDownOpened(
        winrt::IInspectable const& sender,
        winrt::IInspectable const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        this->m_PathTextSnapshot =
            PathCombo().Text().c_str();
    }

    void SplitPage::OnPathComboDropDownClosed(
        winrt::IInspectable const& sender,
        winrt::IInspectable const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (!this->m_PathTextSnapshot.empty() &&
            PathCombo().Text().empty())
        {
            PathCombo().Text(
                winrt::hstring(this->m_PathTextSnapshot));
        }
        this->m_PathTextSnapshot.clear();
    }

    void SplitPage::OnUnloaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        // The window can also be closed with the X button or Alt+F4, which
        // never passes through OnCancelClicked. Treat every close that was
        // not confirmed by OK as a cancel, otherwise the caller would start
        // splitting with values the user never confirmed.
        if (this->m_Context && !this->m_OkClicked)
        {
            this->m_Context->OK = FALSE;
        }
    }

    void SplitPage::OnPageKeyDown(
        winrt::IInspectable const& sender,
        winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        if (e.Key() == winrt::Windows::System::VirtualKey::Escape)
        {
            // Esc closes the dialog like the X button (a cancel).
            e.Handled(true);
            if (this->m_Context && !this->m_OkClicked)
            {
                this->m_Context->OK = FALSE;
            }
            ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
        }
    }

    void SplitPage::OnBrowseClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        std::wstring Current = PathCombo().Text().c_str();
        std::wstring Title = Res(6007, L"Specify a location for output folder").c_str();

        // Use the modern folder picker (IFileOpenDialog + FOS_PICKFOLDERS)
        // instead of the legacy SHBrowseForFolder dialog: the modern dialog
        // follows the system dark theme, while the legacy one renders a
        // white list even in dark mode.
        K7UserDarkModeWorkaroundBypassScope DarkModeWorkaroundBypass;
        winrt::com_ptr<IFileOpenDialog> Dialog;
        HRESULT hr = ::CoCreateInstance(
            CLSID_FileOpenDialog,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&Dialog));
        if (FAILED(hr))
        {
            return;
        }

        DWORD Options = 0;
        Dialog->GetOptions(&Options);
        Dialog->SetOptions(Options | FOS_PICKFOLDERS);
        Dialog->SetTitle(Title.c_str());

        if (!Current.empty())
        {
            winrt::com_ptr<IShellItem> Folder;
            if (SUCCEEDED(::SHCreateItemFromParsingName(
                Current.c_str(),
                nullptr,
                IID_PPV_ARGS(&Folder))))
            {
                Dialog->SetFolder(Folder.get());
            }
        }

        hr = Dialog->Show(this->m_WindowHandle);
        if (SUCCEEDED(hr))
        {
            winrt::com_ptr<IShellItem> Result;
            if (SUCCEEDED(Dialog->GetResult(Result.put())))
            {
                PWSTR PathOut = nullptr;
                if (SUCCEEDED(Result->GetDisplayName(
                    SIGDN_FILESYSPATH, &PathOut)))
                {
                    PathCombo().Text(winrt::hstring(PathOut));
                    ::CoTaskMemFree(PathOut);
                }
            }
        }
    }

    void SplitPage::OnCancelClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        if (this->m_Context)
        {
            this->m_Context->OK = FALSE;
        }
        ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }

    void SplitPage::OnOkClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        if (!this->m_Context)
        {
            ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
            return;
        }

        PK7_SPLIT_DIALOG_CONTEXT Context = this->m_Context;

        // Output path. Trim trailing spaces (the Win32 dialog keeps the raw
        // text; the caller normalizes the directory prefix afterwards).
        {
            std::wstring Path = PathCombo().Text().c_str();
            const size_t EndPos = Path.find_last_not_of(L' ');
            if (EndPos == std::wstring::npos)
            {
                Path.clear();
            }
            else if (EndPos + 1 != Path.size())
            {
                Path.erase(EndPos + 1);
            }
            // Fixed-size ABI buffer: truncate before writing so a very long
            // path can never trigger wcscpy_s failure (which would clear the
            // buffer silently).
            if (Path.size() >= MAX_PATH)
            {
                Path.resize(MAX_PATH - 1);
            }
            wcscpy_s(Context->OutDirPath, Path.c_str());
        }

        // Volume-size validation stays on the 7-Zip side (ParseVolumeSizes);
        // the page only forwards the raw trimmed text through the command
        // callback and keeps the dialog open when it is rejected.
        {
            std::wstring VolumeText = VolumeCombo().Text().c_str();
            const size_t Begin = VolumeText.find_first_not_of(L' ');
            const size_t End = VolumeText.find_last_not_of(L' ');
            if (Begin == std::wstring::npos)
            {
                VolumeText.clear();
            }
            else
            {
                VolumeText = VolumeText.substr(Begin, End - Begin + 1);
            }

            bool Valid = false;
            if (Context->CommandCallback)
            {
                Valid = Context->CommandCallback(
                    Context->CallbackContext,
                    K7_SPLIT_DIALOG_COMMAND_VALIDATE_VOLUME,
                    0,
                    VolumeText.c_str()) != FALSE;
            }

            if (!Valid)
            {
                // Keep the dialog open and show the inline error (the Win32
                // dialog shows a message box and stays open). The window
                // grows once so the error line is fully visible; the initial
                // size was measured without it.
                ErrorText().Visibility(
                    winrt::Windows::UI::Xaml::Visibility::Visible);
                {
                    RECT rc = {};
                    if (::GetWindowRect(
                        this->m_WindowHandle, &rc))
                    {
                        ::SetWindowPos(
                            this->m_WindowHandle,
                            nullptr,
                            0, 0,
                            rc.right - rc.left,
                            rc.bottom - rc.top + 48,
                            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
                    }
                }
                return;
            }
        }

        ErrorText().Visibility(
            winrt::Windows::UI::Xaml::Visibility::Collapsed);

        this->m_OkClicked = true;
        Context->OK = TRUE;
        ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }
}
