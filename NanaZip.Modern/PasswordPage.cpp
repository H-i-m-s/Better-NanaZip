#include "pch.h"
#include "PasswordPage.h"
#if __has_include("PasswordPage.g.cpp")
#include "PasswordPage.g.cpp"
#endif

#include "NanaZip.Modern.h"

#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.h>

#include <string>

namespace winrt::NanaZip::Modern::implementation
{
    PasswordPage::PasswordPage(
        _In_opt_ HWND WindowHandle,
        _In_ PK7_PASSWORD_DIALOG_CONTEXT Context) :
        m_WindowHandle(WindowHandle),
        m_Context(Context),
        m_OkClicked(false)
    {
        this->Loaded({ this, &PasswordPage::OnLoaded });
        this->Unloaded({ this, &PasswordPage::OnUnloaded });
    }

    void PasswordPage::InitializeComponent()
    {
        PasswordPageT::InitializeComponent();
    }

    winrt::hstring PasswordPage::Res(
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

    static winrt::Windows::Foundation::IReference<bool> BoxBool(bool value)
    {
        return winrt::box_value(value).as<
            winrt::Windows::Foundation::IReference<bool>>();
    }

    winrt::hstring PasswordPage::RemoveMnemonic(
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

    void PasswordPage::ApplyDialogFont(UINT32 Pt)
    {
        double FontSizePx = (Pt == 0) ? 0.0 : (double)Pt * 96.0 / 72.0;
        ApplyFontToTree(*this, FontSizePx);
    }

    void PasswordPage::ApplyFontToTree(
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

    void PasswordPage::UpdatePasswordControl()
    {
        bool Show = false;
        if (ShowPasswordCheck().IsChecked())
        {
            Show = ShowPasswordCheck().IsChecked().Value();
        }
        PasswordInput().PasswordRevealMode(
            Show
            ? winrt::Windows::UI::Xaml::Controls::PasswordRevealMode::Visible
            : winrt::Windows::UI::Xaml::Controls::PasswordRevealMode::Hidden);
    }

    winrt::Windows::Foundation::Size PasswordPage::PrepareForShow()
    {
        if (!this->m_Context)
        {
            return winrt::Windows::Foundation::Size(320, 120);
        }

        PK7_PASSWORD_DIALOG_CONTEXT Context = this->m_Context;

        // Dialog title (the Win32 dialog captions the window with the same
        // string as its static label).
        ::SetWindowTextW(
            this->m_WindowHandle,
            Res(3807, L"Password").c_str());

        PasswordText().Text(RemoveMnemonic(Res(3807, L"Password")));
        ShowPasswordCheck().Content(winrt::box_value(
            RemoveMnemonic(Res(3803, L"Show Password"))));
        // 2554: NanaZip-specific free ID; the "share password" box
        // resolves like every other label (English fallback).
        SharePasswordCheck().Content(winrt::box_value(
            RemoveMnemonic(Res(2554, L"Share Password"))));
        OkButton().Content(winrt::box_value(RemoveMnemonic(Res(401, L"OK"))));
        CancelButton().Content(winrt::box_value(RemoveMnemonic(Res(402, L"Cancel"))));

        // Initial password and show-password state.
        PasswordInput().Password(winrt::hstring(Context->Password));
        ShowPasswordCheck().IsChecked(BoxBool(
            Context->ShowPassword != FALSE));
        // "Share password" initial state comes from the host (the
        // "auto share password" setting); changes here never write back.
        SharePasswordCheck().IsChecked(BoxBool(
            Context->SharePassword != FALSE));
        UpdatePasswordControl();

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
        if (MinClientW < 300) MinClientW = 300;
        if (MinClientH < 120) MinClientH = 120;

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

    void PasswordPage::OnLoaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        // Focus the password box like the Win32 dialog (first control).
        PasswordInput().Focus(
            winrt::Windows::UI::Xaml::FocusState::Programmatic);
    }

    void PasswordPage::OnUnloaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        // The window can also be closed with the X button or Alt+F4, which
        // never passes through OnCancelClicked. Treat every close that was
        // not confirmed by OK as a cancel, otherwise the caller would use a
        // password the user never confirmed.
        if (this->m_Context && !this->m_OkClicked)
        {
            this->m_Context->OK = FALSE;
        }
    }

    void PasswordPage::OnShowPasswordClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        UpdatePasswordControl();
    }

    void PasswordPage::OnCloudPasswordClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        // Placeholder: cloud password lookup is implemented later (same as
        // the extract dialog).
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
    }

    void PasswordPage::OnLocalPasswordClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        // Placeholder: local password matching is implemented later (same as
        // the extract dialog).
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
    }

    void PasswordPage::OnSharePasswordClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        // Placeholder: password sharing is implemented later (same as the
        // extract dialog).
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
    }

    void PasswordPage::OnPasswordKeyDown(
        winrt::IInspectable const& sender,
        winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        if (e.Key() == winrt::Windows::System::VirtualKey::Enter)
        {
            // Enter confirms like the default button of the Win32 dialog.
            e.Handled(true);
            OnOkClicked(nullptr, nullptr);
        }
    }

    void PasswordPage::OnPageKeyDown(
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

    void PasswordPage::OnCancelClicked(
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

    void PasswordPage::OnOkClicked(
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

        PK7_PASSWORD_DIALOG_CONTEXT Context = this->m_Context;

        // Read the typed password. Fixed-size ABI buffer: truncate before
        // writing so an over-long password can never trigger wcscpy_s
        // failure (which would clear the buffer silently). The password box
        // caps input at K7_PASSWORD_MAX_PASSWORD_LENGTH - 1, so truncation
        // is only a backstop.
        std::wstring Password = PasswordInput().Password().c_str();
        if (Password.size() >= K7_PASSWORD_MAX_PASSWORD_LENGTH)
        {
            Password.resize(K7_PASSWORD_MAX_PASSWORD_LENGTH - 1);
        }
        wcscpy_s(Context->Password, Password.c_str());

        // Show-password state: the caller persists it to the registry
        // (7-Zip side owns the business rule, like the Win32 dialog).
        bool Show = false;
        if (auto Checked = ShowPasswordCheck().IsChecked())
        {
            Show = Checked.Value();
        }
        Context->ShowPassword = Show ? TRUE : FALSE;

        this->m_OkClicked = true;
        Context->OK = TRUE;
        ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }
}
