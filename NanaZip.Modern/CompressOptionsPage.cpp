#include "pch.h"
#include "CompressOptionsPage.h"
#if __has_include("CompressOptionsPage.g.cpp")
#include "CompressOptionsPage.g.cpp"
#endif

#include "NanaZip.Modern.h"

#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.h>

// Temporary diagnostics for the compression-options dialog bring-up.
static void CompressOptionsDiagLog(const wchar_t* msg)
{
    wchar_t path[MAX_PATH];
    const DWORD n = ::GetTempPathW(MAX_PATH, path);
    if (n == 0 || n >= MAX_PATH)
    {
        return;
    }
    wcscat_s(path, L"sss_settings_diag.log");
    HANDLE h = ::CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        ::WriteFile(h, msg, (DWORD)(wcslen(msg) * sizeof(wchar_t)),
            &written, nullptr);
        ::WriteFile(h, L"\r\n", 4, &written, nullptr);
        ::CloseHandle(h);
    }
}

namespace winrt::NanaZip::Modern::implementation
{
    CompressOptionsPage::CompressOptionsPage(
        _In_opt_ HWND WindowHandle,
        _In_ PK7_COMPRESS_OPTIONS_DIALOG_CONTEXT Context) :
        m_WindowHandle(WindowHandle),
        m_Context(Context)
    {
        CompressOptionsDiagLog(L"[P1] CompressOptionsPage ctor");
    }

    void CompressOptionsPage::InitializeComponent()
    {
        CompressOptionsPageT::InitializeComponent();
    }

    void CompressOptionsPage::ApplyDialogFont(UINT32 Pt)
    {
        // UWP does not inherit FontSize down the visual tree from a
        // Control, so walk the tree and set FontSize explicitly.
        const double FontSizePx =
            (Pt == 0) ? 0.0 : (double)Pt * 96.0 / 72.0;
        ApplyFontToTree(*this, FontSizePx);
    }

    void CompressOptionsPage::ApplyFontToTree(
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

        const int ChildCount =
            winrt::Windows::UI::Xaml::Media::VisualTreeHelper::
                GetChildrenCount(Node);
        for (int i = 0; i < ChildCount; i++)
        {
            ApplyFontToTree(
                winrt::Windows::UI::Xaml::Media::VisualTreeHelper::
                    GetChild(Node, i),
                FontSizePx);
        }
    }

    void CompressOptionsPage::UpdateBoolBox(
        winrt::Windows::UI::Xaml::Controls::CheckBox const& SetCheck,
        winrt::Windows::UI::Xaml::Controls::CheckBox const& Check,
        bool supported,
        bool isSet,
        bool val,
        bool defaultVal)
    {
        const winrt::Windows::UI::Xaml::Visibility Show =
            supported ? winrt::Windows::UI::Xaml::Visibility::Visible
                      : winrt::Windows::UI::Xaml::Visibility::Collapsed;

        SetCheck.IsChecked(isSet);
        SetCheck.Visibility(Show);
        Check.IsChecked(isSet ? val : defaultVal);
        Check.IsEnabled(isSet);
        Check.Visibility(Show);
    }

    void CompressOptionsPage::ReadBoolBox(
        winrt::Windows::UI::Xaml::Controls::CheckBox const& SetCheck,
        winrt::Windows::UI::Xaml::Controls::CheckBox const& Check,
        bool& isSet,
        bool& val)
    {
        const auto SetChecked = SetCheck.IsChecked();
        const auto Checked = Check.IsChecked();
        isSet = SetChecked && SetChecked.Value();
        val = Checked && Checked.Value();
    }

    UINT32 CompressOptionsPage::GetPrecValue()
    {
        if (!m_Context)
        {
            return (UINT32)(INT32)-1;
        }
        const auto SetChecked = PrecSetCheck().IsChecked();
        if (!(SetChecked && SetChecked.Value()))
        {
            // The "set" box is off: auto.
            return (UINT32)(INT32)-1;
        }
        const int Sel = PrecCombo().SelectedIndex();
        if (Sel < 0 || Sel >= (int)m_Context->PrecCount)
        {
            return (UINT32)(INT32)-1;
        }
        return m_Context->PrecItems[Sel].Value;
    }

    void CompressOptionsPage::FillFromContext()
    {
        CompressOptionsDiagLog(L"[P4] FillFromContext enter");
        if (!m_Context)
        {
            CompressOptionsDiagLog(L"[P4b] FillFromContext no context");
            return;
        }
        PK7_COMPRESS_OPTIONS_DIALOG_CONTEXT C = m_Context;

        // Group titles.
        const wchar_t* GroupNtfs =
            C->GroupNtfsText[0] ? C->GroupNtfsText : L"NTFS";
        const wchar_t* GroupTime =
            C->GroupTimeText[0] ? C->GroupTimeText : L"Time";
        NtfsGroupText().Text(winrt::hstring(GroupNtfs));
        TimeGroupText().Text(winrt::hstring(GroupTime));

        // NTFS group.
        NtSymLinksText().Text(winrt::hstring(C->NtSymLinksText));
        NtHardLinksText().Text(winrt::hstring(C->NtHardLinksText));
        NtAltStreamsText().Text(winrt::hstring(C->NtAltStreamsText));
        NtSecurityText().Text(winrt::hstring(C->NtSecurityText));
        NtSymLinksCheck().IsChecked(C->NtSymLinks);
        NtSymLinksCheck().Visibility(
            C->NtSymLinksSupported
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        NtHardLinksCheck().IsChecked(C->NtHardLinks);
        NtHardLinksCheck().Visibility(
            C->NtHardLinksSupported
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        NtAltStreamsCheck().IsChecked(C->NtAltStreams);
        NtAltStreamsCheck().Visibility(
            C->NtAltStreamsSupported
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        NtSecurityCheck().IsChecked(C->NtSecurity);
        NtSecurityCheck().Visibility(
            C->NtSecuritySupported
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        NtfsBorder().Visibility(
            (C->NtSymLinksSupported || C->NtHardLinksSupported ||
             C->NtAltStreamsSupported || C->NtSecuritySupported)
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        NtfsGroupText().Visibility(NtfsBorder().Visibility());

        // Time info line.
        TimeInfoText().Text(winrt::hstring(C->InfoText));

        // Precision row.
        PrecText().Text(winrt::hstring(C->PrecLabelText));
        PrecCombo().Items().Clear();
        for (UINT32 i = 0; i < C->PrecCount && i < K7_COMPRESS_OPTIONS_MAX_PREC; i++)
        {
            PrecCombo().Items().Append(
                winrt::box_value(winrt::hstring(C->PrecItems[i].Text)));
        }
        const bool ShowPrec = (C->PrecCount != 0);
        const winrt::Windows::UI::Xaml::Visibility PrecVis =
            ShowPrec ? winrt::Windows::UI::Xaml::Visibility::Visible
                     : winrt::Windows::UI::Xaml::Visibility::Collapsed;
        PrecRow().Visibility(PrecVis);
        PrecSetCheck().IsChecked(C->TimePrecSet);
        const bool SetIsSupported = C->TimePrecSet || (C->PrecCount > 1);
        PrecSetCheck().Visibility(
            SetIsSupported ? winrt::Windows::UI::Xaml::Visibility::Visible
                           : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        PrecCombo().IsEnabled(C->TimePrecSet && (C->PrecCount > 1));

        // Select the current precision; fall back to the default entry.
        int Sel = -1;
        if ((INT32)C->TimePrec >= 0)
        {
            for (UINT32 i = 0; i < C->PrecCount; i++)
            {
                if (C->PrecItems[i].Value == C->TimePrec)
                {
                    Sel = (int)i;
                    break;
                }
            }
        }
        if (Sel < 0)
        {
            for (UINT32 i = 0; i < C->PrecCount; i++)
            {
                if (C->PrecItems[i].IsDefault)
                {
                    Sel = (int)i;
                    break;
                }
            }
        }
        if (Sel >= 0)
        {
            PrecCombo().SelectedIndex(Sel);
        }

        // Time boxes (M/C/A/Z) and texts.
        MTimeText().Text(winrt::hstring(C->MTimeText));
        CTimeText().Text(winrt::hstring(C->CTimeText));
        ATimeText().Text(winrt::hstring(C->ATimeText));
        ZTimeText().Text(winrt::hstring(C->ZTimeText));
        PreserveATimeText().Text(winrt::hstring(C->PreserveATimeText));
        PreserveATimeCheck().IsChecked(C->PreserveATime);

        // The interactive zip/tar rules recompute the allowed boxes; this
        // also applies the initial state (reads back what was just set).
        ApplyTimeMAC();

        // Buttons.
        OkButton().Content(winrt::box_value(winrt::hstring(
            C->OkText[0] ? C->OkText : L"OK")));
        CancelButton().Content(winrt::box_value(winrt::hstring(
            C->CancelText[0] ? C->CancelText : L"Cancel")));
        CompressOptionsDiagLog(L"[P5] FillFromContext exit");
    }

    void CompressOptionsPage::ApplyTimeMAC()
    {
        if (!m_Context)
        {
            return;
        }
        PK7_COMPRESS_OPTIONS_DIALOG_CONTEXT C = m_Context;

        bool m_allow = !!C->MTimeSupported;
        bool c_allow = !!C->CTimeSupported;
        bool a_allow = !!C->ATimeSupported;

        if (C->IsTar)
        {
            c_allow = false;
            a_allow = !!C->TarPosix;
        }
        if (C->IsZip)
        {
            UINT32 prec = GetPrecValue();
            if (prec == (UINT32)(INT32)-1)
            {
                prec = C->DefaultTimePrec;
            }
            if (prec != 0 /* kTimePrec_Win */)
            {
                c_allow = false;
                a_allow = false;
            }
        }

        // Preserve the current UI values (which may have been edited
        // already) while re-applying the allowed/enabled state.
        bool mSet = false, mVal = false;
        bool cSet = false, cVal = false;
        bool aSet = false, aVal = false;
        bool zSet = false, zVal = false;
        ReadBoolBox(MTimeSetCheck(), MTimeCheck(), mSet, mVal);
        ReadBoolBox(CTimeSetCheck(), CTimeCheck(), cSet, cVal);
        ReadBoolBox(ATimeSetCheck(), ATimeCheck(), aSet, aVal);
        ReadBoolBox(ZTimeSetCheck(), ZTimeCheck(), zSet, zVal);

        UpdateBoolBox(MTimeSetCheck(), MTimeCheck(),
            m_allow, mSet, mVal, !!C->MTimeDefault);
        UpdateBoolBox(CTimeSetCheck(), CTimeCheck(),
            c_allow, cSet, cVal, !!C->CTimeDefault);
        UpdateBoolBox(ATimeSetCheck(), ATimeCheck(),
            a_allow, aSet, aVal, !!C->ATimeDefault);
        UpdateBoolBox(ZTimeSetCheck(), ZTimeCheck(),
            true, zSet, zVal, false);

        // Keep-name formats hide the MTime "set" box when the format's own
        // default applies.
        if (m_allow && !mSet && !C->IsSingleFile)
        {
            MTimeSetCheck().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Collapsed);
            MTimeCheck().IsEnabled(false);
        }
    }

    winrt::Windows::Foundation::Size CompressOptionsPage::PrepareForShow()
    {
        if (!m_Context)
        {
            return winrt::Windows::Foundation::Size(420, 480);
        }

        ApplyDialogFont(m_Context->FontSizePt);
        FillFromContext();
        m_Filled = true;

        // The content drives the default window size: measure the fully
        // laid-out page at infinite width and height (the host caps the
        // height at 75% of the screen; the ScrollViewer handles the rest).
        this->Measure(winrt::Windows::Foundation::Size(100000.0f, 100000.0f));
        const winrt::Windows::Foundation::Size Desired = this->DesiredSize();
        CompressOptionsDiagLog(L"[P6] PrepareForShow measured");

        double W = Desired.Width;
        double H = Desired.Height;
        if (W < 380.0)
        {
            W = 380.0;
        }
        if (H < 320.0)
        {
            H = 320.0;
        }
        return winrt::Windows::Foundation::Size((float)W, (float)H);
    }

    void CompressOptionsPage::OnLoaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        // The visual tree is complete here; (re)apply the dialog font size.
        // Only the first Loaded fills the controls: Mile.Xaml fires Loaded
        // again when the window is shown inside the message loop, and
        // mutating control state (visibility/selection) at that point runs
        // on the layout critical path of a nested XAML island and can
        // deadlock. PrepareForShow has already populated everything.
        if (!m_Context)
        {
            return;
        }
        ApplyDialogFont(m_Context->FontSizePt);
        if (!m_Filled)
        {
            m_Filled = true;
            FillFromContext();
        }
    }

    void CompressOptionsPage::OnPageKeyDown(
        winrt::IInspectable const& sender,
        winrt::KeyRoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        if (e.Key() == winrt::Windows::System::VirtualKey::Escape)
        {
            // Esc closes the dialog like the X button (a cancel). The
            // Win32 host subclass is the reliable path, but hooking the
            // page too covers the case where focus sits inside the XAML
            // tree.
            e.Handled(true);
            if (m_WindowHandle)
            {
                ::PostMessageW(m_WindowHandle, WM_CLOSE, 0, 0);
            }
        }
    }

    void CompressOptionsPage::OnNtfsClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        // Plain check boxes; the values are read back on OK.
    }

    void CompressOptionsPage::OnPrecSetClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (!m_Context)
        {
            return;
        }
        const auto SetChecked = PrecSetCheck().IsChecked();
        const bool isSet = SetChecked && SetChecked.Value();
        PrecCombo().IsEnabled(isSet && (m_Context->PrecCount > 1));
        // A changed precision changes the allowed time boxes for zip.
        ApplyTimeMAC();
    }

    void CompressOptionsPage::OnPrecComboChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        // The zip format only allows C/A time with the Windows precision.
        ApplyTimeMAC();
    }

    void CompressOptionsPage::OnMTimeSetClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (!m_Context)
        {
            return;
        }
        const auto SetChecked = MTimeSetCheck().IsChecked();
        const bool isSet = SetChecked && SetChecked.Value();
        if (!isSet)
        {
            MTimeCheck().IsChecked(!!m_Context->MTimeDefault);
        }
        MTimeCheck().IsEnabled(isSet);
    }

    void CompressOptionsPage::OnCTimeSetClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (!m_Context)
        {
            return;
        }
        const auto SetChecked = CTimeSetCheck().IsChecked();
        const bool isSet = SetChecked && SetChecked.Value();
        if (!isSet)
        {
            CTimeCheck().IsChecked(!!m_Context->CTimeDefault);
        }
        CTimeCheck().IsEnabled(isSet);
    }

    void CompressOptionsPage::OnATimeSetClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (!m_Context)
        {
            return;
        }
        const auto SetChecked = ATimeSetCheck().IsChecked();
        const bool isSet = SetChecked && SetChecked.Value();
        if (!isSet)
        {
            ATimeCheck().IsChecked(!!m_Context->ATimeDefault);
        }
        ATimeCheck().IsEnabled(isSet);
    }

    void CompressOptionsPage::OnZTimeSetClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (!m_Context)
        {
            return;
        }
        const auto SetChecked = ZTimeSetCheck().IsChecked();
        const bool isSet = SetChecked && SetChecked.Value();
        if (!isSet)
        {
            ZTimeCheck().IsChecked(false);
        }
        ZTimeCheck().IsEnabled(isSet);
    }

    void CompressOptionsPage::OnOkClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (m_Context)
        {
            PK7_COMPRESS_OPTIONS_DIALOG_CONTEXT C = m_Context;

            // NTFS group.
            {
                const auto Sym = NtSymLinksCheck().IsChecked();
                const auto Hard = NtHardLinksCheck().IsChecked();
                const auto Alt = NtAltStreamsCheck().IsChecked();
                const auto Sec = NtSecurityCheck().IsChecked();
                const auto Pres = PreserveATimeCheck().IsChecked();
                C->NtSymLinks = Sym && Sym.Value();
                C->NtHardLinks = Hard && Hard.Value();
                C->NtAltStreams = Alt && Alt.Value();
                C->NtSecurity = Sec && Sec.Value();
                C->PreserveATime = Pres && Pres.Value();
            }

            // Precision: the selected item's value, or auto when the
            // "set" box is off or the default entry is selected.
            {
                const auto SetChecked = PrecSetCheck().IsChecked();
                const bool isSet = SetChecked && SetChecked.Value();
                UINT32 Prec = (UINT32)(INT32)-1;
                if (isSet)
                {
                    const int Sel = PrecCombo().SelectedIndex();
                    if (Sel >= 0 && Sel < (int)C->PrecCount)
                    {
                        Prec = C->PrecItems[Sel].Value;
                        if (Prec == C->DefaultTimePrec)
                        {
                            Prec = (UINT32)(INT32)-1;
                        }
                    }
                }
                C->TimePrec = Prec;
                C->TimePrecSet = isSet;
            }

            // Time boxes.
            bool isSet = false, val = false;
            ReadBoolBox(MTimeSetCheck(), MTimeCheck(), isSet, val);
            C->MTimeIsSet = isSet;
            C->MTimeVal = val;
            ReadBoolBox(CTimeSetCheck(), CTimeCheck(), isSet, val);
            C->CTimeIsSet = isSet;
            C->CTimeVal = val;
            ReadBoolBox(ATimeSetCheck(), ATimeCheck(), isSet, val);
            C->ATimeIsSet = isSet;
            C->ATimeVal = val;
            ReadBoolBox(ZTimeSetCheck(), ZTimeCheck(), isSet, val);
            C->ZTimeIsSet = isSet;
            C->ZTimeVal = val;

            C->OK = TRUE;
        }
        if (m_WindowHandle)
        {
            ::PostMessageW(m_WindowHandle, WM_CLOSE, 0, 0);
        }
    }

    void CompressOptionsPage::OnCancelClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (m_WindowHandle)
        {
            ::PostMessageW(m_WindowHandle, WM_CLOSE, 0, 0);
        }
    }
}
