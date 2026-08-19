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
#include <winrt/Windows.UI.Core.h>

#include <string>

namespace winrt::NanaZip::Modern::implementation
{
    PasswordPage::PasswordPage(
        _In_opt_ HWND WindowHandle,
        _In_ PK7_PASSWORD_DIALOG_CONTEXT Context) :
        m_WindowHandle(WindowHandle),
        m_Context(Context),
        m_OkClicked(false),
        m_ProgrammaticPasswordChange(false),
        m_PasswordMatchRunning(false),
        m_LocalMatchRequestId(0),
        m_CloudQueryRequestId(0),
        m_AutoQueryStarted(false),
        m_AutoQueryActive(false)
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
        // 2555/2556: the cloud/local password lookup buttons (NanaZip
        // features, English fallback).
        CloudPasswordButton().Content(winrt::box_value(
            RemoveMnemonic(Res(2555, L"Query cloud password"))));
        LocalPasswordButton().Content(winrt::box_value(
            RemoveMnemonic(Res(2556, L"Match local password"))));
        OkButton().Content(winrt::box_value(RemoveMnemonic(Res(401, L"OK"))));
        CancelButton().Content(winrt::box_value(RemoveMnemonic(Res(402, L"Cancel"))));

        // Initial password and show-password state.
        this->m_ProgrammaticPasswordChange = true;
        PasswordInput().Password(winrt::hstring(Context->Password));
        this->m_ProgrammaticPasswordChange = false;
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

    bool PasswordPage::TryCloudPassword(bool automatic)
    {
        UNREFERENCED_PARAMETER(automatic);
        if (!this->m_Context || !this->m_Context->QueryCallback)
        {
            return false;
        }
        if (this->m_CloudQueryRequestId != 0)
        {
            // A cloud lookup is already in flight; do not stack another one.
            return true;
        }
        wchar_t Password[K7_PASSWORD_MAX_PASSWORD_LENGTH] = {};
        UINT64 RequestId = 0;
        const UINT32 Result = this->m_Context->QueryCallback(
            this->m_Context->ArchivePath,
            K7_PASSWORD_QUERY_SOURCE_CLOUD,
            this->m_Context->QueryContext,
            this->m_WindowHandle,
            &RequestId,
            Password,
            ARRAYSIZE(Password));
        if (Result == K7_PASSWORD_QUERY_RESULT_PENDING)
        {
            // The host runs the network request on a worker; the outcome
            // arrives through SetPasswordFromMatch carrying the same id.
            // The automatic chain stays active so a NOMATCH result can
            // still fall back to the local match (cloud-first priority) or
            // wait for the parallel local match (mixed mode).
            this->m_CloudQueryRequestId = RequestId;
            this->UpdatePasswordMatchRunning();
            return true;
        }
        if (Result != K7_PASSWORD_QUERY_RESULT_MATCHED)
        {
            return false;
        }
        this->m_ProgrammaticPasswordChange = true;
        PasswordInput().Password(winrt::hstring(Password));
        this->m_ProgrammaticPasswordChange = false;
        this->m_Context->PasswordSource = K7_PASSWORD_QUERY_SOURCE_CLOUD;
        this->m_AutoQueryActive = false;
        this->MatchStatusText().Visibility(
            winrt::Windows::UI::Xaml::Visibility::Collapsed);
        return true;
    }

    bool PasswordPage::StartLocalPasswordMatch(bool automatic)
    {
        if (!this->m_Context || !this->m_Context->QueryCallback)
        {
            return false;
        }
        if (this->m_LocalMatchRequestId != 0)
        {
            // A local match is already in flight; do not stack another one.
            return true;
        }
        if (!automatic)
        {
            this->m_AutoQueryActive = false;
        }
        this->MatchStatusText().Visibility(
            winrt::Windows::UI::Xaml::Visibility::Collapsed);
        wchar_t Password[K7_PASSWORD_MAX_PASSWORD_LENGTH] = {};
        UINT64 RequestId = 0;
        const UINT32 Result = this->m_Context->QueryCallback(
            this->m_Context->ArchivePath,
            K7_PASSWORD_QUERY_SOURCE_LOCAL,
            this->m_Context->QueryContext,
            this->m_WindowHandle,
            &RequestId,
            Password,
            ARRAYSIZE(Password));
        if (Result == K7_PASSWORD_QUERY_RESULT_MATCHED)
        {
            this->m_ProgrammaticPasswordChange = true;
            PasswordInput().Password(winrt::hstring(Password));
            this->m_ProgrammaticPasswordChange = false;
            this->m_Context->PasswordSource = K7_PASSWORD_QUERY_SOURCE_LOCAL;
            this->m_AutoQueryActive = false;
            return true;
        }
        if (Result == K7_PASSWORD_QUERY_RESULT_PENDING)
        {
            // The host really started a task; only now does the button
            // switch to the cancelling state and the page wait for the
            // result message carrying this request id.
            this->m_LocalMatchRequestId = RequestId;
            this->UpdatePasswordMatchRunning();
            this->LocalPasswordButton().Content(winrt::box_value(
                RemoveMnemonic(Res(2558, L"Cancel matching"))));
            return true;
        }
        // NOT_FOUND: the request was not started, so the button stays in its
        // normal state and no result message will arrive.
        return false;
    }

    void PasswordPage::StartAutomaticPasswordQuery()
    {
        if (this->m_AutoQueryStarted || !this->m_Context ||
            !this->m_Context->QueryCallback ||
            (!this->m_Context->AutoQueryCloud &&
             !this->m_Context->AutoMatchLocal))
        {
            return;
        }
        this->m_AutoQueryStarted = true;
        this->m_AutoQueryActive = true;
        const DWORD Priority = this->m_Context->MatchPriority;
        if (Priority == 2)
        {
            // Mixed: run both lookups in parallel, first match wins. Each
            // request is routed independently by its own id; the result
            // handling in SetPasswordFromMatch resolves the winner.
            bool anyStarted = false;
            if (this->m_Context->AutoMatchLocal &&
                this->StartLocalPasswordMatch(true))
            {
                anyStarted = true;
            }
            if (this->m_Context->AutoQueryCloud &&
                this->TryCloudPassword(true))
            {
                anyStarted = true;
            }
            if (!anyStarted)
            {
                this->m_AutoQueryActive = false;
            }
            return;
        }
        if (Priority == 1)
        {
            // Cloud first: cloud, then local when the cloud misses (the
            // fallback is started from SetPasswordFromMatch).
            if (this->m_Context->AutoQueryCloud &&
                this->TryCloudPassword(true))
            {
                return;
            }
            if (this->m_Context->AutoMatchLocal &&
                this->StartLocalPasswordMatch(true))
            {
                return;
            }
        }
        else
        {
            // Local first: local, then cloud when the local match misses.
            if (this->m_Context->AutoMatchLocal &&
                this->StartLocalPasswordMatch(true))
            {
                return;
            }
            if (this->m_Context->AutoQueryCloud &&
                this->TryCloudPassword(true))
            {
                return;
            }
        }
        this->m_AutoQueryActive = false;
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
        this->Dispatcher().TryRunAsync(
            winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
            [this]()
            {
                this->StartAutomaticPasswordQuery();
            });
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
        // Cancel any in-flight lookup so workers stop promptly (their data
        // is a snapshot, so they can also finish alone; results are routed
        // by request id and are ignored once the page forgets the ids
        // below).
        if (this->m_Context && this->m_Context->QueryCancelCallback)
        {
            if (this->m_LocalMatchRequestId != 0)
            {
                this->m_Context->QueryCancelCallback(
                    this->m_Context->QueryContext,
                    this->m_LocalMatchRequestId);
            }
            if (this->m_CloudQueryRequestId != 0)
            {
                this->m_Context->QueryCancelCallback(
                    this->m_Context->QueryContext,
                    this->m_CloudQueryRequestId);
            }
        }
        this->m_LocalMatchRequestId = 0;
        this->m_CloudQueryRequestId = 0;
        this->m_PasswordMatchRunning = false;
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
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        this->TryCloudPassword(false);
    }

    void PasswordPage::OnLocalPasswordClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (this->m_PasswordMatchRunning)
        {
            // Cancel every in-flight lookup and restore the button
            // immediately. A worker may still be verifying the current
            // candidate; its result is routed by request id and ignored
            // once the page clears the ids below, so the UI never waits.
            if (this->m_Context && this->m_Context->QueryCancelCallback)
            {
                if (this->m_LocalMatchRequestId != 0)
                {
                    this->m_Context->QueryCancelCallback(
                        this->m_Context->QueryContext,
                        this->m_LocalMatchRequestId);
                }
                if (this->m_CloudQueryRequestId != 0)
                {
                    this->m_Context->QueryCancelCallback(
                        this->m_Context->QueryContext,
                        this->m_CloudQueryRequestId);
                }
            }
            this->m_LocalMatchRequestId = 0;
            this->m_CloudQueryRequestId = 0;
            this->m_PasswordMatchRunning = false;
            this->LocalPasswordButton().Content(winrt::box_value(
                RemoveMnemonic(Res(2556, L"Match local password"))));
            this->MatchStatusText().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Collapsed);
            return;
        }
        this->StartLocalPasswordMatch(false);
    }

    void PasswordPage::SetPasswordFromMatch(
        UINT64 RequestId,
        INT Status,
        LPCWSTR Password,
        UINT32 Source)
    {
        // Route the result by its source request id. Results that match no
        // outstanding request belong to an older dialog or to a task that
        // was already cancelled; ignore them.
        bool IsLocalResult = false;
        if (this->m_LocalMatchRequestId != 0 &&
            RequestId == this->m_LocalMatchRequestId)
        {
            IsLocalResult = true;
            this->m_LocalMatchRequestId = 0;
        }
        else if (this->m_CloudQueryRequestId != 0 &&
            RequestId == this->m_CloudQueryRequestId)
        {
            this->m_CloudQueryRequestId = 0;
        }
        else
        {
            return;
        }
        this->UpdatePasswordMatchRunning();
        this->LocalPasswordButton().Content(winrt::box_value(
            RemoveMnemonic(Res(2556, L"Match local password"))));

        if (Status == K7_PASSWORD_MATCH_STATUS_MATCHED && Password)
        {
            if (IsLocalResult)
            {
                // The verified local password wins. In mixed mode the cloud
                // query keeps running; its later result is ignored below.
                this->FillPassword(Password, Source);
            }
            else
            {
                // Cloud result. In mixed mode the local worker may already
                // have filled the box; a verified local password is never
                // replaced by an unverified cloud answer.
                if (this->m_Context &&
                    this->m_Context->PasswordSource == 0)
                {
                    this->FillPassword(Password, Source);
                    if (this->m_Context->MatchPriority == 2 &&
                        this->m_LocalMatchRequestId != 0)
                    {
                        // Cloud won the race in mixed mode: stop the local
                        // worker so it stops testing candidates.
                        if (this->m_Context->QueryCancelCallback)
                        {
                            this->m_Context->QueryCancelCallback(
                                this->m_Context->QueryContext,
                                this->m_LocalMatchRequestId);
                        }
                        this->m_LocalMatchRequestId = 0;
                        this->UpdatePasswordMatchRunning();
                    }
                }
            }
        }
        else if (Status == K7_PASSWORD_MATCH_STATUS_NOMATCH)
        {
            if (this->m_PasswordMatchRunning)
            {
                // The other source is still running (mixed mode); wait for
                // it instead of declaring failure.
            }
            else if (this->m_Context &&
                this->m_Context->PasswordSource != 0)
            {
                // A password is already in the box (mixed mode, the other
                // source won the race); this NOMATCH changes nothing.
            }
            else if (this->m_AutoQueryActive && this->m_Context)
            {
                // Serial fallback: the automatic chain tries the second
                // source once when the first one misses. m_AutoQueryActive
                // is cleared before starting it so a second NOMATCH never
                // re-enters this branch (no endless loop between the two
                // sources).
                this->m_AutoQueryActive = false;
                const DWORD Priority = this->m_Context->MatchPriority;
                if (Priority == 1 && this->m_Context->AutoMatchLocal)
                {
                    if (this->StartLocalPasswordMatch(true))
                    {
                        return;
                    }
                }
                else if (Priority == 0 && this->m_Context->AutoQueryCloud)
                {
                    if (this->TryCloudPassword(true))
                    {
                        return;
                    }
                }
                this->MatchStatusText().Text(Res(
                    2557, L"No matching password found in the password book"));
                this->MatchStatusText().Visibility(
                    winrt::Windows::UI::Xaml::Visibility::Visible);
            }
            else
            {
                // Inline notice instead of a popup: nested XAML modal
                // windows cannot be shown from inside the dialog (Mile.Xaml
                // posts WM_QUIT when a ContentWindow is destroyed, which
                // would close the dialog too), and a native MessageBox
                // cannot follow the dialog theme. The text inherits the
                // dialog font size and theme colors.
                this->MatchStatusText().Text(Res(
                    2557, L"No matching password found in the password book"));
                this->MatchStatusText().Visibility(
                    winrt::Windows::UI::Xaml::Visibility::Visible);
            }
        }
        else
        {
            // K7_PASSWORD_MATCH_STATUS_CANCELLED: silent restore.
            this->MatchStatusText().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Collapsed);
        }
    }

    void PasswordPage::UpdatePasswordMatchRunning()
    {
        this->m_PasswordMatchRunning =
            this->m_LocalMatchRequestId != 0 ||
            this->m_CloudQueryRequestId != 0;
    }

    void PasswordPage::FillPassword(LPCWSTR Password, UINT32 Source)
    {
        this->m_ProgrammaticPasswordChange = true;
        PasswordInput().Password(winrt::hstring(Password));
        this->m_ProgrammaticPasswordChange = false;
        if (this->m_Context)
        {
            this->m_Context->PasswordSource =
                Source == K7_PASSWORD_QUERY_SOURCE_CLOUD ? 1 : 2;
        }
        this->m_AutoQueryActive = false;
        this->MatchStatusText().Visibility(
            winrt::Windows::UI::Xaml::Visibility::Collapsed);
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

    void PasswordPage::OnPasswordChanged(
        winrt::IInspectable const& sender,
        winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (this->m_Context && !this->m_ProgrammaticPasswordChange)
        {
            this->m_Context->PasswordSource = 0;
        }
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
        Context->SharePassword = SharePasswordCheck().IsChecked() &&
            SharePasswordCheck().IsChecked().Value() ? TRUE : FALSE;

        this->m_OkClicked = true;
        Context->OK = TRUE;
        ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }
}
