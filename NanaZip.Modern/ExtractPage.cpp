#include "pch.h"
#include "ExtractPage.h"
#if __has_include("ExtractPage.g.cpp")
#include "ExtractPage.g.cpp"
#endif

#include "NanaZip.Modern.h"
#include <K7User.h>

#include <shlobj.h>

#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Animation.h>
#include <winrt/Windows.UI.Core.h>

#include <vector>
#include <algorithm>
#include <cwctype>

namespace winrt::NanaZip::Modern::implementation
{
    ExtractPage::ExtractPage(
        _In_opt_ HWND WindowHandle,
        _In_ PK7_EXTRACT_DIALOG_CONTEXT Context) :
        m_WindowHandle(WindowHandle),
        m_Context(Context),
        m_InitGuard(false),
        m_OkClicked(false),
        m_FirstLayout(true),
        m_WrapThresholdW(0.0),
        m_ProgrammaticPasswordChange(false),
        m_PasswordMatchRunning(false),
        m_LocalMatchRequestId(0),
        m_CloudQueryRequestId(0),
        m_AutoQueryStarted(false),
        m_AutoQueryActive(false)
    {
        this->Unloaded({ this, &ExtractPage::OnUnloaded });
        this->Loaded({ this, &ExtractPage::OnLoaded });
        this->SizeChanged({ this, &ExtractPage::OnSizeChanged });
    }

    void ExtractPage::InitializeComponent()
    {
        ExtractPageT::InitializeComponent();
    }

    winrt::hstring ExtractPage::Res(
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

    void ExtractPage::ApplyDialogFont(UINT32 Pt)
    {
        double FontSizePx = (Pt == 0) ? 0.0 : (double)Pt * 96.0 / 72.0;
        ApplyFontToTree(*this, FontSizePx);
    }

    void ExtractPage::ApplyFontToTree(
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

    bool ExtractPage::GetBoolsVal(
        BOOLEAN Def1, BOOLEAN Val1,
        BOOLEAN Def2, BOOLEAN Val2) const
    {
        if (Def1)
        {
            return Val1 != 0;
        }
        if (Def2)
        {
            return Val2 != 0;
        }
        return Val1 != 0;
    }

    void ExtractPage::SetBoolsResult(
        bool Value,
        BOOLEAN& Def1, BOOLEAN& Val1,
        BOOLEAN& Def2, BOOLEAN& Val2) const
    {
        const bool Old = GetBoolsVal(Def1, Val1, Def2, Val2);
        if (Value != Old)
        {
            Def1 = TRUE;
            Def2 = TRUE;
        }
        Val1 = Value ? TRUE : FALSE;
        Val2 = Value ? TRUE : FALSE;
    }

    static bool IsPathSeparator(wchar_t Ch)
    {
        return Ch == L'\\' || Ch == L'/';
    }

    static winrt::Windows::Foundation::IReference<bool> BoxBool(bool value)
    {
        return winrt::box_value(value).as<
            winrt::Windows::Foundation::IReference<bool>>();
    }

    static bool GetBool(
        winrt::Windows::Foundation::IReference<bool> const& ref)
    {
        if (ref)
        {
            return ref.Value();
        }
        return false;
    }

    void ExtractPage::UpdatePasswordControl()
    {
        const bool Show =
            GetBool(ShowPasswordCheck().IsChecked());
        PasswordBox().PasswordRevealMode(Show
            ? winrt::Windows::UI::Xaml::Controls::PasswordRevealMode::Visible
            : winrt::Windows::UI::Xaml::Controls::PasswordRevealMode::Hidden);
    }

    static std::wstring NormalizeDirPathPrefix(std::wstring Path)
    {
        // Trim trailing spaces.
        size_t End = Path.find_last_not_of(L' ');
        if (End == std::wstring::npos)
        {
            Path.clear();
        }
        else
        {
            Path.erase(End + 1);
        }
        // NormalizeDirPathPrefix: ensure the path ends with a path separator.
        if (!Path.empty() && !IsPathSeparator(Path.back()))
        {
            Path += L'\\';
        }
        return Path;
    }

    static void SplitPathToPartsSmart(
        std::wstring const& Path,
        std::wstring& Prefix,
        std::wstring& Name)
    {
        std::wstring Path2 = Path;
        while (Path2.size() > 1 && IsPathSeparator(Path2.back()))
        {
            Path2.pop_back();
        }
        size_t Pos = Path2.find_last_of(L"\\/");
        if (Pos == std::wstring::npos)
        {
            Prefix.clear();
            Name = Path2;
        }
        else
        {
            Prefix = Path2.substr(0, Pos + 1);
            Name = Path2.substr(Pos + 1);
        }
    }

    static void TrimString(std::wstring& s)
    {
        size_t Start = s.find_first_not_of(L" \t\r\n");
        size_t End = s.find_last_not_of(L" \t\r\n");
        if (Start == std::wstring::npos)
        {
            s.clear();
        }
        else
        {
            s = s.substr(Start, End - Start + 1);
        }
    }

    bool ExtractPage::TryCloudPassword(bool automatic)
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
            this->m_Context->ArcPath,
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
        PasswordBox().Password(winrt::hstring(Password));
        this->m_ProgrammaticPasswordChange = false;
        this->m_Context->PasswordSource = K7_PASSWORD_QUERY_SOURCE_CLOUD;
        this->m_AutoQueryActive = false;
        this->MatchStatusText().Visibility(
            winrt::Windows::UI::Xaml::Visibility::Collapsed);
        return true;
    }

    bool ExtractPage::StartLocalPasswordMatch(bool automatic)
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
            this->m_Context->ArcPath,
            K7_PASSWORD_QUERY_SOURCE_LOCAL,
            this->m_Context->QueryContext,
            this->m_WindowHandle,
            &RequestId,
            Password,
            ARRAYSIZE(Password));
        if (Result == K7_PASSWORD_QUERY_RESULT_MATCHED)
        {
            this->m_ProgrammaticPasswordChange = true;
            PasswordBox().Password(winrt::hstring(Password));
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
            this->LocalPasswordButton().Content(
                winrt::box_value(Res(2558, L"Cancel matching")));
            return true;
        }
        // NOT_FOUND: the request was not started (e.g. no candidates or the
        // host could not run a match), so the button stays in its normal
        // state and no result message will arrive.
        return false;
    }

    void ExtractPage::StartEncryptionCheck()
    {
        if (!this->m_Context || !this->m_Context->EncryptionCheckCallback ||
            this->m_Context->HasEncryptedItems)
        {
            // No pre-check available: the host already supplied
            // HasEncryptedItems, or cannot scan, so keep the automatic
            // lookup decision unchanged.
            this->StartAutomaticPasswordQuery();
            return;
        }
        // Start the pre-check on the host thread; the outcome arrives as
        // K7_PASSWORD_ENCRYPTION_CHECK_DONE_MESSAGE. Until then the page
        // waits: it must not run a lookup on an archive it cannot confirm
        // needs a password.
        if (!this->m_Context->EncryptionCheckCallback(
            this->m_Context->QueryContext,
            this->m_WindowHandle))
        {
            // The host could not start the check; without confirmation the
            // automatic lookup must not guess.
            this->m_Context->HasEncryptedItems = FALSE;
            this->StartAutomaticPasswordQuery();
        }
    }

    void ExtractPage::SetEncryptionCheckResult(BOOLEAN HasEncryptedItems)
    {
        if (!this->m_Context)
        {
            return;
        }
        this->m_Context->HasEncryptedItems = HasEncryptedItems;
        this->StartAutomaticPasswordQuery();
    }

    void ExtractPage::StartAutomaticPasswordQuery()
    {
        if (this->m_AutoQueryStarted || !this->m_Context ||
            !this->m_Context->HasEncryptedItems ||
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

    void ExtractPage::OnLoaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        this->m_InitGuard = false;
        // Run after the first layout so every control is ready; the dialog
        // is already visible by then, so the pre-check (and any lookup it
        // enables) never delays the dialog from appearing.
        this->Dispatcher().TryRunAsync(
            winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
            [this]()
            {
                this->StartEncryptionCheck();
            });
    }

    winrt::Windows::Foundation::Size ExtractPage::PrepareForShow()
    {
        if (!this->m_Context)
        {
            return winrt::Windows::Foundation::Size(560, 460);
        }

        PK7_EXTRACT_DIALOG_CONTEXT Context = this->m_Context;

        // Dialog title: "Extract : <arc path>".
        {
            std::wstring Title = Res(3400, L"Extract").c_str();
            std::wstring ArcPath(Context->ArcPath);
            if (!ArcPath.empty())
            {
                Title += L" : ";
                Title += ArcPath;
            }
            ::SetWindowTextW(this->m_WindowHandle, Title.c_str());
        }

        ExtractToText().Text(Res(3401, L"E&xtract to:"));
        PathModeText().Text(Res(3410, L"Path mode:"));
        OverwriteModeText().Text(Res(3420, L"Overwrite mode:"));
        ElimDupCheck().Content(winrt::box_value(Res(3430, L"Eliminate duplication of root folder")));
        NtSecurityCheck().Content(winrt::box_value(Res(3431, L"Restore file security")));
        OpenFolderCheck().Content(winrt::box_value(Res(3433, L"&Open folder after extract")));
        DeleteAfterCheck().Content(winrt::box_value(Res(3435, L"Delete archive after extraction")));
        PasswordGroupText().Text(Res(3807, L"Password"));
        ShowPasswordCheck().Content(winrt::box_value(Res(3803, L"Show Password")));
        // 2554: NanaZip-specific free ID; the "share password" box
        // resolves like every other label (English fallback).
        SharePasswordCheck().Content(winrt::box_value(Res(2554, L"Share Password")));
        // 2555/2556: the cloud/local password lookup buttons (NanaZip
        // features, English fallback).
        CloudPasswordButton().Content(winrt::box_value(Res(2555, L"Query cloud password")));
        LocalPasswordButton().Content(winrt::box_value(Res(2556, L"Match local password")));
        // 2559: the add-to-password-book button next to the password box.
        AddPasswordButton().IsEnabled(
            this->m_Context && this->m_Context->AddPasswordCallback != nullptr);
        winrt::Windows::UI::Xaml::Controls::ToolTipService::SetToolTip(
            this->AddPasswordButton(),
            winrt::box_value(Res(2600, L"Add to password book")));
        OkButton().Content(winrt::box_value(Res(401, L"OK")));
        CancelButton().Content(winrt::box_value(Res(402, L"Cancel")));

        // --- Path field: split the initial directory into prefix + name. ---
        std::wstring PathPrefix;
        std::wstring PathName;
        SplitPathToPartsSmart(Context->DirPath, PathPrefix, PathName);
        if (PathPrefix.empty())
        {
            PathPrefix = PathName;
        }
        else
        {
            NameBox().Text(winrt::hstring(PathName));
        }

        const bool SplitDest =
            GetBoolsVal(Context->SplitDestDef, Context->SplitDestVal,
                Context->SplitDestDef2, Context->SplitDestVal2);
        NameEnableCheck().IsChecked(BoxBool(SplitDest));
        NameBox().Visibility(SplitDest
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);

        PathCombo().Text(winrt::hstring(PathPrefix));

        // Drop-down: the current path goes first, followed by the history
        // (deduplicated), so the combo never hides what is shown in the box.
        if (!PathPrefix.empty())
        {
            PathCombo().Items().Append(winrt::box_value(
                winrt::hstring(PathPrefix)));
        }
        for (UINT32 i = 0; i < Context->NumPaths && i < 16; i++)
        {
            if (Context->Paths[i][0] &&
                Context->Paths[i] != PathPrefix)
            {
                PathCombo().Items().Append(winrt::box_value(
                    winrt::hstring(Context->Paths[i])));
            }
        }

        // Guard against the editable combo blanking the path text when its
        // drop-down is opened and closed.
        PathCombo().DropDownOpened(
            { this, &ExtractPage::OnPathComboDropDownOpened });
        PathCombo().DropDownClosed(
            { this, &ExtractPage::OnPathComboDropDownClosed });

        // --- Path mode combo ---
        UINT32 PathMode = Context->PathMode;
        if (!Context->PathMode_Force &&
            Context->PathModeDefault != 0xFFFFFFFF)
        {
            PathMode = Context->PathModeDefault;
        }
        {
            const wchar_t* Items[3] = {
                L"Full paths", L"No paths", L"Absolute paths" };
            const UINT32 IDs[3] = { 3411, 3412, 3413 };
            int CurSel = 0;
            for (int i = 0; i < 3; i++)
            {
                PathModeCombo().Items().Append(winrt::box_value(
                    Res(IDs[i], Items[i])));
                if ((UINT32)i == PathMode)
                {
                    CurSel = i;
                }
            }
            PathModeCombo().SelectedIndex(CurSel);
        }

        // --- Overwrite mode combo ---
        UINT32 OverwriteMode = Context->OverwriteMode;
        if (!Context->OverwriteMode_Force &&
            Context->OverwriteModeDefault != 0xFFFFFFFF)
        {
            OverwriteMode = Context->OverwriteModeDefault;
        }
        {
            const wchar_t* Items[5] = {
                L"Ask before overwrite", L"Overwrite without prompt",
                L"Skip existing files", L"Rename existing file",
                L"Auto rename existing file" };
            const UINT32 IDs[5] = { 3421, 3422, 3423, 3424, 3425 };
            int CurSel = 0;
            for (int i = 0; i < 5; i++)
            {
                OverwriteModeCombo().Items().Append(winrt::box_value(
                    Res(IDs[i], Items[i])));
                if ((UINT32)i == OverwriteMode)
                {
                    CurSel = i;
                }
            }
            OverwriteModeCombo().SelectedIndex(CurSel);
        }

        // --- Checkboxes ---
        ElimDupCheck().IsChecked(BoxBool(
            GetBoolsVal(Context->ElimDupDef, Context->ElimDupVal,
                Context->ElimDupDef2, Context->ElimDupVal2)));
        NtSecurityCheck().IsChecked(BoxBool(
            GetBoolsVal(Context->NtSecurityDef, Context->NtSecurityVal,
                Context->NtSecurityDef2, Context->NtSecurityVal2)));
        OpenFolderCheck().IsChecked(BoxBool(
            GetBoolsVal(Context->OpenFolderDef, Context->OpenFolderVal,
                Context->OpenFolderDef2, Context->OpenFolderVal2)));
        DeleteAfterCheck().IsChecked(BoxBool(
            Context->DeleteAfterExtract != FALSE));
        ShowPasswordCheck().IsChecked(BoxBool(
            GetBoolsVal(Context->ShowPasswordDef, Context->ShowPasswordVal,
                Context->ShowPasswordDef2, Context->ShowPasswordVal2)));
        // "Share password" initial state comes from the caller (the
        // "auto share password" setting); changes here never write back.
        SharePasswordCheck().IsChecked(BoxBool(
            Context->SharePassword != FALSE));

        // --- Password ---
        this->m_ProgrammaticPasswordChange = true;
        PasswordBox().Password(winrt::hstring(Context->Password));
        this->m_ProgrammaticPasswordChange = false;
        UpdatePasswordControl();

        ApplyDialogFont(Context->FontSizeDialog);

        // Measure the content with an infinite constraint so every control
        // reports its natural size (including the longest combo item and the
        // current dialog font size). The two columns are equal-width (1:1),
        // so the dialog width must fit the wider column twice, otherwise the
        // equal split would squeeze the wider column and clip its controls
        // (e.g. the path combo) or wrap its text. The caller sizes the window
        // from this before it is shown, so there is no visible resize after
        // the dialog appears. The returned size is the content size without
        // margin; the caller adds a comfortable margin for the default size
        // and uses a smaller value for the minimum track size.
        winrt::Windows::Foundation::Size Inf(
            100000.0f, 100000.0f);
        this->Measure(Inf);
        winrt::Windows::Foundation::Size Desired = this->DesiredSize();

        float LeftW = LeftColumnPanel().DesiredSize().Width;
        float RightW = RightColumnPanel().DesiredSize().Width;
        float Wide = LeftW > RightW ? LeftW : RightW;
        // Column gap (12+12) plus the page padding (12+12).
        Desired.Width = Wide * 2.0f + 48.0f;
        return Desired;
    }

    void ExtractPage::OnUnloaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        // The window can also be closed with the X button or Alt+F4, which
        // never passes through OnCancelClicked. Treat every close that was
        // not confirmed by OK as a cancel, otherwise the caller would start
        // extracting with a dialog the user never confirmed.
        if (this->m_Context && !this->m_OkClicked)
        {
            this->m_Context->OK = FALSE;
        }
        // Cancel any in-flight lookup so workers stop promptly (their data
        // is a snapshot, so they can also finish alone; results are routed
        // by request id and are ignored once the page forgets the ids
        // below).
        if (this->m_AddPasswordTimer)
        {
            this->m_AddPasswordTimer.Stop();
        }
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

    void ExtractPage::OnSizeChanged(
        winrt::IInspectable const& sender,
        winrt::Windows::UI::Xaml::SizeChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        this->UpdateModeRowLayout();
    }

    void ExtractPage::OnPageKeyDown(
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

    // Recursively collects all visual children of a given type.
    template <typename T>
    static void FindVisualChildren(
        winrt::Windows::UI::Xaml::DependencyObject const& Node,
        std::vector<T>& Out)
    {
        const int Count =
            winrt::Windows::UI::Xaml::Media::VisualTreeHelper::
                GetChildrenCount(Node);
        for (int i = 0; i < Count; i++)
        {
            auto Child =
                winrt::Windows::UI::Xaml::Media::VisualTreeHelper::
                    GetChild(Node, i);
            if (auto Tried = Child.try_as<T>())
            {
                Out.push_back(Tried);
            }
            FindVisualChildren(Child, Out);
        }
    }

    void ExtractPage::OnPathComboDropDownOpened(
        winrt::IInspectable const& sender,
        winrt::IInspectable const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        this->m_PathTextSnapshot =
            PathCombo().Text().c_str();

        // Show the "x" on every history entry once the drop-down is open,
        // but never on the first entry (the current path). Containers are
        // generated asynchronously, so defer one dispatch.
        this->Dispatcher().TryRunAsync(
            winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
            [this]()
        {
            const uint32_t Count = PathCombo().Items().Size();
            for (uint32_t i = 0; i < Count; i++)
            {
                auto Container = PathCombo().ContainerFromIndex((int)i);
                if (!Container)
                {
                    continue;
                }
                std::vector<
                    winrt::Windows::UI::Xaml::Controls::Button> Buttons;
                FindVisualChildren(
                    Container.as<
                        winrt::Windows::UI::Xaml::DependencyObject>(),
                    Buttons);
                const auto Vis = (i == 0)
                    ? winrt::Windows::UI::Xaml::Visibility::Collapsed
                    : winrt::Windows::UI::Xaml::Visibility::Visible;
                for (auto& B : Buttons)
                {
                    B.Visibility(Vis);
                }
            }
        });
    }

    void ExtractPage::OnPathComboDropDownClosed(
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

        // Hide every "x" again; this also covers the closed-state
        // selection renderer which would otherwise show an "x" in the box.
        std::vector<
            winrt::Windows::UI::Xaml::Controls::Button> Buttons;
        FindVisualChildren(PathCombo(), Buttons);
        for (auto& B : Buttons)
        {
            B.Visibility(winrt::Windows::UI::Xaml::Visibility::Collapsed);
        }
    }

    void ExtractPage::OnDeleteHistoryPathClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        auto Button = sender.as<
            winrt::Windows::UI::Xaml::Controls::Button>();
        auto Data = Button.DataContext();
        if (!Data)
        {
            return;
        }
        std::wstring Path =
            winrt::unbox_value<winrt::hstring>(Data).c_str();

        // The first entry mirrors the current path in the box; it is not a
        // history entry, so its "x" does nothing.
        if (Path.empty() ||
            Path == PathCombo().Text().c_str())
        {
            return;
        }

        // Remove the entry from the drop-down list.
        const auto& Items = PathCombo().Items();
        for (uint32_t i = 0; i < Items.Size(); i++)
        {
            auto Item = Items.GetAt(i);
            if (Item)
            {
                auto Text = winrt::unbox_value_or<
                    winrt::hstring>(Item, winrt::hstring());
                if (Text == winrt::hstring(Path))
                {
                    Items.RemoveAt(i);
                    break;
                }
            }
        }

        // Record it so the caller can persist the removal (even on cancel).
        if (this->m_Context &&
            this->m_Context->NumRemovedPaths < 16)
        {
            wcscpy_s(
                this->m_Context->RemovedPaths[
                    this->m_Context->NumRemovedPaths],
                Path.c_str());
            this->m_Context->NumRemovedPaths++;
        }
    }

    // Lays a mode combo out on the same row as its label, or below it
    // (indented to the label text, with vertical separation).
    static void SetModeRowLayout(
        winrt::Windows::UI::Xaml::Controls::ComboBox const& Combo,
        bool Wrap)
    {
        winrt::Windows::UI::Xaml::Controls::Grid::SetRow(
            Combo, Wrap ? 1 : 0);
        winrt::Windows::UI::Xaml::Controls::Grid::SetColumn(
            Combo, Wrap ? 0 : 1);
        winrt::Windows::UI::Xaml::Controls::Grid::SetColumnSpan(
            Combo, Wrap ? 2 : 1);
        Combo.Margin(Wrap
            ? winrt::Windows::UI::Xaml::Thickness(28.0, 8.0, 0.0, 0.0)
            : winrt::Windows::UI::Xaml::Thickness(6.0, 0.0, 0.0, 0.0));
    }

    void ExtractPage::UpdateModeRowLayout()
    {
        if (!this->m_Context)
        {
            return;
        }

        const double PageW = this->ActualWidth();
        if (PageW <= 0.0)
        {
            return;
        }

        // Wrap once the page gets narrower than the wrapped layout's need
        // (plus a little slack). The track size is computed from the same
        // wrapped layout, so the wrap is always reachable before the drag
        // is stopped. The minimum is only recomputed on the first layout
        // and on wrap state changes, so dragging stays smooth instead of
        // re-measuring on every resize tick.
        const bool Wrap = PageW < this->m_WrapThresholdW;
        const bool WasWrap =
            winrt::Windows::UI::Xaml::Controls::Grid::GetRow(
                PathModeCombo()) == 1;
        if (this->m_FirstLayout || Wrap != WasWrap)
        {
            this->m_FirstLayout = false;
            this->RecalcMinTrack();
        }
        SetModeRowLayout(PathModeCombo(), Wrap);
        SetModeRowLayout(OverwriteModeCombo(), Wrap);

        // The two password buttons stay side by side while they fit; when
        // the right column gets too narrow, the second button drops below
        // the first one.
        {
            const double RightColW = (PageW - 24.0 - 24.0) / 2.0;
            const double ButtonsNeed =
                CloudPasswordButton().ActualWidth() +
                6.0 +
                LocalPasswordButton().ActualWidth();
            const bool ButtonsWrap = ButtonsNeed > RightColW;
            PasswordButtonsPanel().Orientation(ButtonsWrap
                ? winrt::Windows::UI::Xaml::Controls::Orientation::Vertical
                : winrt::Windows::UI::Xaml::Controls::Orientation::Horizontal);
            LocalPasswordButton().Margin(ButtonsWrap
                ? winrt::Windows::UI::Xaml::Thickness(0.0, 6.0, 0.0, 0.0)
                : winrt::Windows::UI::Xaml::Thickness(6.0, 0.0, 0.0, 0.0));
        }
    }

    void ExtractPage::RecalcMinTrack()
    {
        if (!this->m_Context)
        {
            return;
        }

        const double PageW = this->ActualWidth();
        if (PageW <= 0.0)
        {
            return;
        }

        // Measure in the wrapped state: that is the smallest the dialog can
        // shrink to. The wrap threshold sits slightly above it, so the user
        // can actually reach the wrap before the track size stops the drag
        // (measuring in the same-row state would set the minimum too high
        // and block the wrap entirely).
        SetModeRowLayout(PathModeCombo(), true);
        SetModeRowLayout(OverwriteModeCombo(), true);

        winrt::Windows::Foundation::Size Constraint(
            (float)PageW, 100000.0f);
        this->Measure(Constraint);
        winrt::Windows::Foundation::Size Desired = this->DesiredSize();

        this->m_WrapThresholdW = Desired.Width + 40.0;

        const UINT Dpi = ::GetDpiForWindow(this->m_WindowHandle);
        const float Scale = (float)Dpi / (float)USER_DEFAULT_SCREEN_DPI;

        int MinClientW = (int)((Desired.Width + 32.0f) * Scale + 0.5f);
        int MinClientH = (int)(Desired.Height * Scale + 0.5f);
        if (MinClientW < 480) MinClientW = 480;
        if (MinClientH < 400) MinClientH = 400;

        RECT rc = { 0, 0, MinClientW, MinClientH };
        {
            const LONG_PTR Style = ::GetWindowLongPtrW(
                this->m_WindowHandle, GWL_STYLE);
            const LONG_PTR ExStyle = ::GetWindowLongPtrW(
                this->m_WindowHandle, GWL_EXSTYLE);
            ::AdjustWindowRectEx(&rc, (DWORD)Style, FALSE, (DWORD)ExStyle);
        }

        this->m_Context->MinTrackW = rc.right - rc.left;
        this->m_Context->MinTrackH = rc.bottom - rc.top;

        // Restore the same-row layout; the caller applies the final state.
        SetModeRowLayout(PathModeCombo(), false);
        SetModeRowLayout(OverwriteModeCombo(), false);
    }

    void ExtractPage::OnBrowseClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        std::wstring Current =
            PathCombo().Text().c_str();
        std::wstring Title = Res(3402, L"Extract to folder").c_str();

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

    void ExtractPage::OnNameEnableClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        const bool Enabled =
            GetBool(NameEnableCheck().IsChecked());
        NameBox().Visibility(Enabled
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
    }

    void ExtractPage::OnShowPasswordClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        UpdatePasswordControl();
    }

    void ExtractPage::OnPasswordChanged(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (this->m_Context && !this->m_ProgrammaticPasswordChange &&
            !this->m_InitGuard)
        {
            this->m_Context->PasswordSource = 0;
        }
    }

    void ExtractPage::OnElimDupClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
    }

    void ExtractPage::OnNtSecurityClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
    }

    void ExtractPage::OnOpenFolderClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
    }

    void ExtractPage::OnDeleteAfterClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
    }

    void ExtractPage::OnCloudPasswordClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        this->TryCloudPassword(false);
    }

    void ExtractPage::OnLocalPasswordClicked(
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
            this->LocalPasswordButton().Content(
                winrt::box_value(Res(2556, L"Match local password")));
            this->MatchStatusText().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Collapsed);
            return;
        }
        this->StartLocalPasswordMatch(false);
    }

    void ExtractPage::SetPasswordFromMatch(
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
        this->LocalPasswordButton().Content(
            winrt::box_value(Res(2556, L"Match local password")));

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
                // windows cannot be shown from inside the extract dialog
                // (Mile.Xaml posts WM_QUIT when a ContentWindow is
                // destroyed, which would close the dialog too), and a
                // native MessageBox cannot follow the dialog theme. The
                // text inherits the dialog font size and theme colors.
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

    void ExtractPage::UpdatePasswordMatchRunning()
    {
        this->m_PasswordMatchRunning =
            this->m_LocalMatchRequestId != 0 ||
            this->m_CloudQueryRequestId != 0;
    }

    void ExtractPage::FillPassword(LPCWSTR Password, UINT32 Source)
    {
        this->m_ProgrammaticPasswordChange = true;
        PasswordBox().Password(winrt::hstring(Password));
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

    void ExtractPage::OnSharePasswordClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        // Placeholder: password sharing is implemented later.
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
    }

    void ExtractPage::OnAddPasswordClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (!this->m_Context || !this->m_Context->AddPasswordCallback)
        {
            return;
        }
        const std::wstring password(PasswordBox().Password().c_str());
        if (password.empty())
        {
            return;
        }
        if (this->m_Context->AddPasswordCallback(password.c_str()) == FALSE)
        {
            // Silent failure: keep the '+' glyph.
            return;
        }
        // Success: check mark on a translucent light-green background, then
        // fade back to '+' after a short pause.
        this->AddPasswordGlyph().Glyph(winrt::hstring(L"\uE73E"));
        this->m_AddPasswordBrush =
            winrt::Windows::UI::Xaml::Media::SolidColorBrush(
                winrt::Windows::UI::ColorHelper::FromArgb(
                    0x40, 0x6E, 0xC8, 0x6E));
        this->AddPasswordButton().Background(this->m_AddPasswordBrush);
        if (!this->m_AddPasswordTimer)
        {
            this->m_AddPasswordTimer =
                winrt::Windows::UI::Xaml::DispatcherTimer();
            this->m_AddPasswordTimer.Interval(
                std::chrono::milliseconds(1200));
            this->m_AddPasswordTimer.Tick(
                [this](winrt::IInspectable const&, winrt::IInspectable const&)
            {
                this->RestoreAddPasswordButton();
            });
        }
        this->m_AddPasswordTimer.Start();
    }

    void ExtractPage::RestoreAddPasswordButton()
    {
        if (this->m_AddPasswordTimer)
        {
            this->m_AddPasswordTimer.Stop();
        }
        // Swap the glyph back to '+' immediately and fade the green
        // background out.
        this->AddPasswordGlyph().Glyph(winrt::hstring(L"\uE710"));
        if (this->m_AddPasswordBrush)
        {
            auto storyboard =
                winrt::Windows::UI::Xaml::Media::Animation::Storyboard();
            auto animation =
                winrt::Windows::UI::Xaml::Media::Animation::ColorAnimation();
            animation.From(this->m_AddPasswordBrush.Color());
            animation.To(winrt::Windows::UI::Color{ 0, 0, 0, 0 });
            animation.Duration(
                winrt::Windows::UI::Xaml::DurationHelper::FromTimeSpan(
                    winrt::Windows::Foundation::TimeSpan{
                        std::chrono::milliseconds(250) }));
            winrt::Windows::UI::Xaml::Media::Animation::Storyboard::
                SetTarget(animation, this->m_AddPasswordBrush);
            winrt::Windows::UI::Xaml::Media::Animation::Storyboard::
                SetTargetProperty(animation, L"Color");
            storyboard.Children().Append(animation);
            auto button = this->AddPasswordButton();
            auto brush = this->m_AddPasswordBrush;
            storyboard.Completed(
                [button, brush](winrt::IInspectable const&,
                    winrt::IInspectable const&)
            {
                // The animation only faded the color; clear the brush so
                // the button returns to its default look.
                button.Background(nullptr);
                brush.Color(winrt::Windows::UI::Color{ 0, 0, 0, 0 });
            });
            storyboard.Begin();
        }
    }

    void ExtractPage::OnCancelClicked(
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

    void ExtractPage::OnOkClicked(
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

        PK7_EXTRACT_DIALOG_CONTEXT Context = this->m_Context;

        // Path mode (0/1/2).
        {
            int Sel = PathModeCombo().SelectedIndex();
            Context->PathMode = (Sel >= 0 && Sel <= 2) ? (UINT32)Sel : 0;
        }

        // Overwrite mode (0..4).
        {
            int Sel = OverwriteModeCombo().SelectedIndex();
            Context->OverwriteMode = (Sel >= 0 && Sel <= 4) ? (UINT32)Sel : 0;
        }

        // Password.
        {
            std::wstring Password = PasswordBox().Password().c_str();
            if (Password.size() >= ARRAYSIZE(Context->Password))
            {
                Password.resize(ARRAYSIZE(Context->Password) - 1);
            }
            wcscpy_s(Context->Password, Password.c_str());
        }

        // Checkbox pairs.
        SetBoolsResult(
            GetBool(ElimDupCheck().IsChecked()),
            Context->ElimDupDef, Context->ElimDupVal,
            Context->ElimDupDef2, Context->ElimDupVal2);
        SetBoolsResult(
            GetBool(NtSecurityCheck().IsChecked()),
            Context->NtSecurityDef, Context->NtSecurityVal,
            Context->NtSecurityDef2, Context->NtSecurityVal2);
        SetBoolsResult(
            GetBool(OpenFolderCheck().IsChecked()),
            Context->OpenFolderDef, Context->OpenFolderVal,
            Context->OpenFolderDef2, Context->OpenFolderVal2);
        SetBoolsResult(
            GetBool(ShowPasswordCheck().IsChecked()),
            Context->ShowPasswordDef, Context->ShowPasswordVal,
            Context->ShowPasswordDef2, Context->ShowPasswordVal2);
        Context->DeleteAfterExtract =
            GetBool(DeleteAfterCheck().IsChecked()) ? TRUE : FALSE;
        Context->SharePassword =
            GetBool(SharePasswordCheck().IsChecked()) ? TRUE : FALSE;

        const bool SplitDest =
            GetBool(NameEnableCheck().IsChecked());
        SetBoolsResult(
            SplitDest,
            Context->SplitDestDef, Context->SplitDestVal,
            Context->SplitDestDef2, Context->SplitDestVal2);
        Context->SplitDestEnable = SplitDest ? TRUE : FALSE;

        // Path: the combo text (without sub path) goes into history.
        std::wstring PathText = PathCombo().Text().c_str();
        TrimString(PathText);
        std::wstring PathNoSub = NormalizeDirPathPrefix(PathText);

        // History: current path first, unique, max 16 entries.
        {
            struct Entry
            {
                std::wstring Value;
            };
            std::vector<Entry> History;
            auto AddUnique = [&History](std::wstring const& s)
            {
                std::wstring T = s;
                TrimString(T);
                if (T.empty())
                {
                    return;
                }
                for (auto const& Item : History)
                {
                    std::wstring A = Item.Value;
                    std::wstring B = T;
                    std::transform(A.begin(), A.end(), A.begin(), ::towlower);
                    std::transform(B.begin(), B.end(), B.begin(), ::towlower);
                    if (A == B)
                    {
                        return;
                    }
                }
                History.push_back(Entry{ T });
            };

            AddUnique(PathText);
            for (UINT32 i = 0; i < Context->NumPaths && i < 16; i++)
            {
                AddUnique(Context->Paths[i]);
            }

            UINT32 Num = 0;
            for (auto const& Item : History)
            {
                if (Num >= 16)
                {
                    break;
                }
                wcscpy_s(Context->Paths[Num], Item.Value.c_str());
                Num++;
            }
            Context->NumPaths = Num;
        }

        // Final directory: prefix + sub path when enabled.
        std::wstring FinalDir = PathNoSub;
        if (SplitDest)
        {
            std::wstring SubPath = NameBox().Text().c_str();
            TrimString(SubPath);
            if (!SubPath.empty())
            {
                FinalDir += SubPath;
                FinalDir = NormalizeDirPathPrefix(FinalDir);
            }
        }
        wcscpy_s(Context->OutDirPath, FinalDir.c_str());

        {
            std::wstring SubPath = NameBox().Text().c_str();
            wcscpy_s(Context->OutPathName, SubPath.c_str());
        }

        Context->OK = TRUE;
        this->m_OkClicked = true;
        ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }
}
