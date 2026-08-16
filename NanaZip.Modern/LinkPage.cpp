#include "pch.h"
#include "LinkPage.h"
#if __has_include("LinkPage.g.cpp")
#include "LinkPage.g.cpp"
#endif

#include "NanaZip.Modern.h"

#include <K7User.h>

#include <Mile.Helpers.CppWinRT.h>

#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.h>

#include <shlobj.h>

#include <string>

namespace winrt::NanaZip::Modern::implementation
{
    LinkPage::LinkPage(
        _In_opt_ HWND WindowHandle,
        _In_ PK7_LINK_DIALOG_CONTEXT Context) :
        m_WindowHandle(WindowHandle),
        m_Context(Context),
        m_OkClicked(false)
    {
        this->Loaded({ this, &LinkPage::OnLoaded });
        this->Unloaded({ this, &LinkPage::OnUnloaded });
    }

    void LinkPage::InitializeComponent()
    {
        LinkPageT::InitializeComponent();
    }

    winrt::hstring LinkPage::Res(
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

    winrt::hstring LinkPage::RemoveMnemonic(
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

    void LinkPage::ApplyDialogFont(UINT32 Pt)
    {
        double FontSizePx = (Pt == 0) ? 0.0 : (double)Pt * 96.0 / 72.0;
        ApplyFontToTree(*this, FontSizePx);
    }

    void LinkPage::ApplyFontToTree(
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

    winrt::Windows::Foundation::Size LinkPage::PrepareForShow()
    {
        if (!this->m_Context)
        {
            return winrt::Windows::Foundation::Size(420, 320);
        }

        PK7_LINK_DIALOG_CONTEXT Context = this->m_Context;

        // Dialog title comes from the caller (it owns the language strings,
        // like the Win32 CLinkDialog caption).
        ::SetWindowTextW(this->m_WindowHandle, Context->Title);

        FromLabelText().Text(RemoveMnemonic(
            winrt::hstring(Context->FromLabel)));
        ToLabelText().Text(RemoveMnemonic(
            winrt::hstring(Context->ToLabel)));
        TypeGroupLabelText().Text(RemoveMnemonic(
            winrt::hstring(Context->TypeGroupLabel)));
        LinkButton().Content(winrt::box_value(RemoveMnemonic(
            winrt::hstring(Context->LinkButtonText))));
        CancelButton().Content(winrt::box_value(
            RemoveMnemonic(Res(402, L"Cancel"))));
        HintText().Text(winrt::hstring(Context->Hint));

        FromCombo().Text(winrt::hstring(Context->From));
        if (Context->From[0])
        {
            FromCombo().Items().Append(winrt::box_value(
                winrt::hstring(Context->From)));
        }
        ToCombo().Text(winrt::hstring(Context->To));
        if (Context->To[0])
        {
            ToCombo().Items().Append(winrt::box_value(
                winrt::hstring(Context->To)));
        }

        // Link type radio buttons: name text from the caller, initial
        // selection from InitialLinkType (0-4).
        winrt::Windows::UI::Xaml::Controls::RadioButton Radios[K7_LINK_MAX_TYPE_COUNT] =
        {
            TypeRadio0(),
            TypeRadio1(),
            TypeRadio2(),
            TypeRadio3(),
            TypeRadio4(),
        };
        for (UINT32 i = 0; i < K7_LINK_MAX_TYPE_COUNT; i++)
        {
            Radios[i].Content(winrt::box_value(RemoveMnemonic(
                winrt::hstring(Context->TypeNames[i]))));
            Radios[i].IsChecked(
                (i == Context->InitialLinkType) && (Context->InitialLinkType < K7_LINK_MAX_TYPE_COUNT));
        }

        ApplyDialogFont(Context->FontSizeDialog);

        // Measure the content with an infinite constraint so every control
        // reports its natural size (with the current dialog font size). The
        // caller sizes the window from this before it is shown, so there is
        // no visible resize after the dialog appears.
        winrt::Windows::Foundation::Size Inf(
            100000.0f, 100000.0f);
        this->Measure(Inf);
        winrt::Windows::Foundation::Size Desired = this->DesiredSize();

        // Minimum window size: the measured content (plus a small margin), so
        // dragging the border can never clip the controls.
        const UINT Dpi = ::GetDpiForWindow(this->m_WindowHandle);
        const float Scale = (float)Dpi / (float)USER_DEFAULT_SCREEN_DPI;

        int MinClientW = (int)((Desired.Width + 32.0f) * Scale + 0.5f);
        int MinClientH = (int)((Desired.Height + 4.0f) * Scale + 0.5f);
        if (MinClientW < 420) MinClientW = 420;
        if (MinClientH < 300) MinClientH = 300;

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

    void LinkPage::OnLoaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        // Focus the first combo like the Win32 dialog (first control).
        FromCombo().Focus(
            winrt::Windows::UI::Xaml::FocusState::Programmatic);
    }

    void LinkPage::OnUnloaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        // The window can also be closed with the X button or Alt+F4, which
        // never passes through OnCancelClicked. Treat every close that was
        // not confirmed by OK as a cancel.
        if (this->m_Context && !this->m_OkClicked)
        {
            this->m_Context->OK = FALSE;
        }
    }

    void LinkPage::OnComboKeyDown(
        winrt::IInspectable const& sender,
        winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        if (e.Key() == winrt::Windows::System::VirtualKey::Enter)
        {
            // Enter confirms like the default button of the Win32 dialog.
            e.Handled(true);
            OnLinkClicked(nullptr, nullptr);
        }
    }

    void LinkPage::OnPageKeyDown(
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

    void LinkPage::OnCancelClicked(
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

    void LinkPage::OnLinkClicked(
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

        PK7_LINK_DIALOG_CONTEXT Context = this->m_Context;

        // Read the typed paths. Fixed-size ABI buffers: truncate before
        // writing so an over-long value can never trigger wcscpy_s failure.
        std::wstring From = FromCombo().Text().c_str();
        if (From.size() >= K7_LINK_MAX_TEXT_LENGTH)
        {
            From.resize(K7_LINK_MAX_TEXT_LENGTH - 1);
        }
        wcscpy_s(Context->From, From.c_str());

        std::wstring To = ToCombo().Text().c_str();
        if (To.size() >= K7_LINK_MAX_TEXT_LENGTH)
        {
            To.resize(K7_LINK_MAX_TEXT_LENGTH - 1);
        }
        wcscpy_s(Context->To, To.c_str());

        // Read the selected link type (0-4).
        UINT32 LinkType = 0;
        {
            winrt::Windows::UI::Xaml::Controls::RadioButton Radios[K7_LINK_MAX_TYPE_COUNT] =
            {
                TypeRadio0(),
                TypeRadio1(),
                TypeRadio2(),
                TypeRadio3(),
                TypeRadio4(),
            };
            for (UINT32 i = 0; i < K7_LINK_MAX_TYPE_COUNT; i++)
            {
                auto IsChecked = Radios[i].IsChecked();
                if (IsChecked && IsChecked.Value())
                {
                    LinkType = i;
                    break;
                }
            }
        }
        Context->LinkType = LinkType;

        // Ask the 7-Zip side to create the link (the caller owns the
        // business rules). Success closes the dialog with OK; failure keeps
        // the dialog open with the error shown inline.
        if (Context->CommandCallback)
        {
            WCHAR ErrorBuffer[K7_LINK_MAX_ERROR_LENGTH] = {};
            if (Context->CommandCallback(
                Context->From,
                Context->To,
                Context->LinkType,
                Context->CallbackContext,
                ErrorBuffer,
                K7_LINK_MAX_ERROR_LENGTH))
            {
                this->m_OkClicked = true;
                Context->OK = TRUE;
                ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
                return;
            }

            this->ErrorText().Text(winrt::hstring(ErrorBuffer));
            this->ErrorText().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Visible);
        }
    }

    void LinkPage::BrowseTo(
        winrt::Windows::UI::Xaml::Controls::ComboBox const& Combo)
    {
        K7UserDarkModeWorkaroundBypassScope DarkModeWorkaroundBypass;
        winrt::com_ptr<IFileDialog> FileDialog =
            winrt::try_create_instance<IFileDialog>(
                CLSID_FileOpenDialog,
                CLSCTX_INPROC_SERVER);
        if (!FileDialog)
        {
            return;
        }

        FILEOPENDIALOGOPTIONS Options;
        if (FAILED(FileDialog->GetOptions(&Options)))
        {
            Options = 0;
        }
        if (FAILED(FileDialog->SetOptions(
            Options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM)))
        {
            return;
        }

        FileDialog->SetTitle(Mile::WinRT::GetLocalizedString(
            L"NanaZip.Modern/LinkPage/SelectFolderText",
            L"Select a folder.").c_str());

        {
            IShellItem* DefaultFolder = nullptr;
            if (SUCCEEDED(::SHCreateItemFromParsingName(
                Combo.Text().c_str(),
                nullptr,
                IID_PPV_ARGS(&DefaultFolder))))
            {
                FileDialog->SetFolder(DefaultFolder);
                DefaultFolder->Release();
            }
        }

        if (SUCCEEDED(FileDialog->Show(this->m_WindowHandle)))
        {
            IShellItem* Result = nullptr;
            if (SUCCEEDED(FileDialog->GetResult(&Result)))
            {
                std::wstring Path;
                {
                    LPWSTR RawPath = nullptr;
                    if (SUCCEEDED(Result->GetDisplayName(
                        SIGDN_FILESYSPATH,
                        &RawPath)))
                    {
                        Path = std::wstring(RawPath);
                        ::CoTaskMemFree(RawPath);
                    }
                }
                if (!Path.empty())
                {
                    if (!Path.ends_with(L"\\"))
                    {
                        Path.append(L"\\");
                    }
                    Combo.Text(Path);
                }

                Result->Release();
            }
        }
    }

    void LinkPage::OnBrowseFromClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        BrowseTo(FromCombo());
    }

    void LinkPage::OnBrowseToClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        BrowseTo(ToCombo());
    }
}
