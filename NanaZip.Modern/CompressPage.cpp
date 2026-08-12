#include "pch.h"
#include "CompressPage.h"
#if __has_include("CompressPage.g.cpp")
#include "CompressPage.g.cpp"
#endif

#include "NanaZip.Modern.h"

#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Core.h>

#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>

namespace winrt::NanaZip::Modern::implementation
{
    // Border has no IsEnabled (it is not a Control). Recursively apply
    // IsEnabled to every Control inside a container subtree instead.
    static void SetTreeEnabled(
        winrt::Windows::UI::Xaml::UIElement const& Element,
        bool Enabled)
    {
        if (auto Control =
            Element.try_as<winrt::Windows::UI::Xaml::Controls::Control>())
        {
            Control.IsEnabled(Enabled);
        }
        if (auto Panel =
            Element.try_as<winrt::Windows::UI::Xaml::Controls::Panel>())
        {
            for (auto const& Child : Panel.Children())
            {
                SetTreeEnabled(Child, Enabled);
            }
        }
        if (auto Border =
            Element.try_as<winrt::Windows::UI::Xaml::Controls::Border>())
        {
            if (auto Child = Border.Child())
            {
                SetTreeEnabled(Child, Enabled);
            }
        }
    }

    static void TrimString(std::wstring& s)
    {
        size_t Start = s.find_first_not_of(L" \t\r\n");
        size_t End = s.find_last_not_of(L" \t\r\n");
        if (Start == std::wstring::npos)
        {
            s.clear();
            return;
        }
        if (Start != 0 || End + 1 != s.size())
        {
            s = s.substr(Start, End - Start + 1);
        }
    }

    template <typename T>
    static void FindVisualChildren(
        winrt::Windows::UI::Xaml::DependencyObject const& Node,
        std::vector<T>& Out)
    {
        const int Count = winrt::Windows::UI::Xaml::Media::VisualTreeHelper::
            GetChildrenCount(Node);
        for (int i = 0; i < Count; i++)
        {
            auto Child = winrt::Windows::UI::Xaml::Media::VisualTreeHelper::
                GetChild(Node, i);
            if (auto Tried = Child.try_as<T>())
            {
                Out.push_back(Tried);
            }
            FindVisualChildren(Child, Out);
        }
    }

    CompressPage::CompressPage(
        _In_opt_ HWND WindowHandle,
        _In_ PK7_COMPRESS_DIALOG_CONTEXT Context) :
        m_WindowHandle(WindowHandle),
        m_Context(Context),
        m_InitGuard(false),
        m_OkClicked(false),
        m_FirstLayout(true),
        m_LeftWrapped(false),
        m_EncryptionWrapped(false),
        m_LeftWrapThresholdW(0.0),
        m_EncryptionWrapThresholdW(0.0)
    {
        this->Unloaded({ this, &CompressPage::OnUnloaded });
        this->Loaded({ this, &CompressPage::OnLoaded });
        this->SizeChanged({ this, &CompressPage::OnSizeChanged });
    }

    void CompressPage::InitializeComponent()
    {
        CompressPageT::InitializeComponent();
    }

    winrt::hstring CompressPage::Res(
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

    static bool GetBool(
        winrt::Windows::Foundation::IReference<bool> const& ref)
    {
        if (ref)
        {
            return ref.Value();
        }
        return false;
    }

    void CompressPage::ApplyDialogFont(UINT32 Pt)
    {
        double FontSizePx = (Pt == 0) ? 0.0 : (double)Pt * 96.0 / 72.0;
        ApplyFontToTree(*this, FontSizePx);
    }

    void CompressPage::ApplyFontToTree(
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

    void CompressPage::ApplyOptionList(
        winrt::Windows::UI::Xaml::Controls::ComboBox const& Combo,
        _In_ const K7_COMPRESS_OPTION_LIST& List)
    {
        Combo.Items().Clear();
        for (UINT32 i = 0; i < List.Count && i < K7_COMPRESS_MAX_OPTION_ITEMS; i++)
        {
            Combo.Items().Append(winrt::box_value(
                winrt::hstring(List.Items[i].DisplayText)));
        }
        if (List.Count > 0)
        {
            if (List.SelectedIndex >= 0 &&
                (UINT32)List.SelectedIndex < List.Count)
            {
                Combo.SelectedIndex(List.SelectedIndex);
            }
            else
            {
                Combo.SelectedIndex(0);
            }
        }
        else
        {
            Combo.SelectedIndex(-1);
        }
        // Editable combos (archive path, volume) must stay enabled even when
        // the option list is empty so the user can still type.
        if (!Combo.IsEditable())
        {
            Combo.IsEnabled(List.Enabled && List.Count > 0);
        }
    }

    void CompressPage::ApplySnapshotToUi()
    {
        if (!this->m_Context)
        {
            return;
        }

        PK7_COMPRESS_DIALOG_CONTEXT Context = this->m_Context;

        // Static labels are loaded once in ApplyLabels.
        ArchivePathCombo().Text(winrt::hstring(Context->ArchivePath));
        ParametersBox().Text(winrt::hstring(Context->Parameters));
        VolumeCombo().Text(winrt::hstring(Context->VolumeText));

        ApplyOptionList(FormatCombo(), Context->Formats);
        ApplyOptionList(LevelCombo(), Context->Levels);
        ApplyOptionList(MethodCombo(), Context->Methods);
        ApplyOptionList(DictionaryCombo(), Context->Dictionaries);
        ApplyOptionList(OrderCombo(), Context->Orders);
        ApplyOptionList(SolidCombo(), Context->SolidBlocks);
        ApplyOptionList(ThreadsCombo(), Context->Threads);
        ApplyOptionList(MemoryCombo(), Context->MemoryLimits);
        ApplyOptionList(UpdateModeCombo(), Context->UpdateModes);
        ApplyOptionList(PathModeCombo(), Context->PathModes);
        ApplyOptionList(EncryptionMethodCombo(), Context->EncryptionMethods);
        ApplyOptionList(VolumeCombo(), Context->Volumes);

        // Capability and visibility flags.
        SfxCheck().IsChecked(BoxBool(Context->SfxMode != FALSE));
        SfxCheck().IsEnabled(Context->SfxEnabled != FALSE);
        SfxCheck().Visibility(Context->SfxVisible
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);

        SharedCheck().IsChecked(BoxBool(Context->OpenShareForWrite != FALSE));
        DeleteCheck().IsChecked(BoxBool(Context->DeleteAfterCompressing != FALSE));

        EncryptionBorder().Visibility(Context->EncryptionVisible
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        SetTreeEnabled(
            EncryptionBorder(),
            Context->EncryptionEnabled != FALSE);

        EncryptHeadersCheck().IsChecked(BoxBool(Context->EncryptHeaders != FALSE));
        EncryptHeadersCheck().IsEnabled(Context->EncryptHeadersAllowed != FALSE);

        ShowPasswordCheck().IsChecked(BoxBool(Context->ShowPassword != FALSE));
        PasswordBox().Password(winrt::hstring(Context->Password));
        ReenterPasswordBox().Password(winrt::hstring(Context->PasswordConfirmation));
        UpdatePasswordControl();

        MemoryValueText().Text(winrt::hstring(Context->MemoryValueText));
        DecompressMemoryText().Text(winrt::hstring(Context->DecompressMemoryText));
        DecompressMemoryText().Visibility(Context->DecompressMemoryVisible
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        HardwareThreadsText().Text(winrt::hstring(Context->HardwareThreadsText));
        ArchiveFolderText().Text(winrt::hstring(Context->ArchiveFolderText));
        OptionsSummaryText().Text(winrt::hstring(Context->OptionsSummaryText));
        OptionsButton().IsEnabled(Context->OptionsEnabled != FALSE);

        VolumeCombo().Visibility(Context->VolumeVisible
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        VolumeText().Visibility(Context->VolumeVisible
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        ParametersBox().Visibility(Context->ParametersVisible
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        ParametersText().Visibility(Context->ParametersVisible
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);

        ErrorTextBlock().Text(winrt::hstring(Context->ErrorText));
        ErrorTextBlock().Visibility(Context->ErrorText[0]
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
    }

    void CompressPage::RefreshFromSnapshot()
    {
        this->m_InitGuard = true;
        ApplySnapshotToUi();
        this->m_InitGuard = false;
    }

    void CompressPage::ApplyLabels()
    {
        // The resource ids come from CompressDialogRes.h in the Universal
        // project. The fallbacks are the English UI strings.
        ArchiveFolderText().Text(Res(4001, L"&Archive:"));
        FormatText().Text(Res(4003, L"Archive &format:"));
        LevelText().Text(Res(4004, L"Compression &level:"));
        MethodText().Text(Res(4005, L"Compression &method:"));
        DictionaryText().Text(Res(4006, L"&Dictionary size:"));
        OrderText().Text(Res(4007, L"&Word size:"));
        SolidText().Text(Res(4008, L"&Solid Block size:"));
        ThreadsText().Text(Res(4009, L"Number of CPU &threads:"));
        MemoryText().Text(Res(4017, L"Memory usage for Compressing:"));
        DecompressMemoryText().Text(Res(4018, L"Memory usage for Decompressing:"));
        VolumeText().Text(Res(7302, L"Split to &volumes, bytes:"));
        ParametersText().Text(Res(4010, L"Parameters:"));
        UpdateModeText().Text(Res(4002, L"&Update mode:"));
        PathModeText().Text(Res(3410, L"Path mode:"));
        OptionsGroupText().Text(Res(4011, L"Options"));
        SfxCheck().Content(winrt::box_value(Res(4012, L"Create SF&X archive")));
        SharedCheck().Content(winrt::box_value(Res(4013, L"Compress shared files")));
        DeleteCheck().Content(winrt::box_value(Res(4019, L"Delete files after compression")));
        EncryptionGroupText().Text(Res(4014, L"Encryption"));
        PasswordText().Text(Res(3801, L"Enter &password:"));
        ReenterPasswordText().Text(Res(3802, L"Reenter password:"));
        ShowPasswordCheck().Content(winrt::box_value(Res(3803, L"Show Password")));
        EncryptionMethodText().Text(Res(4015, L"&Encryption method:"));
        EncryptHeadersCheck().Content(winrt::box_value(Res(4016, L"Encrypt file &names")));
        OptionsButton().Content(winrt::box_value(Res(4011, L"Options")));
        OkButton().Content(winrt::box_value(Res(401, L"OK")));
        CancelButton().Content(winrt::box_value(Res(402, L"Cancel")));
    }

    void CompressPage::UpdatePasswordControl()
    {
        const bool Show = GetBool(ShowPasswordCheck().IsChecked());
        PasswordBox().PasswordRevealMode(Show
            ? winrt::Windows::UI::Xaml::Controls::PasswordRevealMode::Visible
            : winrt::Windows::UI::Xaml::Controls::PasswordRevealMode::Hidden);
        ReenterPasswordBox().PasswordRevealMode(Show
            ? winrt::Windows::UI::Xaml::Controls::PasswordRevealMode::Visible
            : winrt::Windows::UI::Xaml::Controls::PasswordRevealMode::Hidden);
        ReenterPasswordBox().Visibility(
            Show ? winrt::Windows::UI::Xaml::Visibility::Collapsed :
            winrt::Windows::UI::Xaml::Visibility::Visible);
        ReenterPasswordText().Visibility(
            Show ? winrt::Windows::UI::Xaml::Visibility::Collapsed :
            winrt::Windows::UI::Xaml::Visibility::Visible);
    }

    bool CompressPage::SendCommand(
        UINT32 Command,
        INT64 Value,
        LPCWSTR SemanticText)
    {
        if (this->m_Context && this->m_Context->CommandCallback)
        {
            const bool Accepted = this->m_Context->CommandCallback(
                this->m_Context->CallbackContext,
                Command,
                Value,
                SemanticText);
            if (!Accepted)
            {
                return false;
            }
        }
        return true;
    }

    winrt::Windows::Foundation::Size CompressPage::PrepareForShow()
    {
        if (!this->m_Context)
        {
            return winrt::Windows::Foundation::Size(640, 560);
        }

        this->m_InitGuard = true;
        ApplyLabels();
        ApplySnapshotToUi();
        this->m_InitGuard = false;

        FillArchivePathHistory();

        ApplyDialogFont(this->m_Context->FontSizeDialog);

        // Size the window for the side-by-side layout: measure both
        // columns independently (a Grid with two star columns only needs
        // max(left, right) per column, otherwise the wider column gets
        // squeezed and the rows wrap immediately). The page is measured
        // at that content width to report the natural height.
        winrt::Windows::Foundation::Size Inf(100000.0f, 100000.0f);
        LeftColumnPanel().Measure(Inf);
        RightColumnPanel().Measure(Inf);
        AlignLeftLabelsColumn();
        LeftColumnPanel().Measure(Inf);
        winrt::Windows::Foundation::Size Left =
            LeftColumnPanel().DesiredSize();
        RightColumnPanel().Measure(Inf);
        winrt::Windows::Foundation::Size Right =
            RightColumnPanel().DesiredSize();
        const double ColW = (std::max)(Left.Width, Right.Width);
        const double ContentW = ColW * 2.0 + 24.0;
        winrt::Windows::Foundation::Size Constraint(
            (float)ContentW, 100000.0f);
        this->Measure(Constraint);
        winrt::Windows::Foundation::Size Desired = this->DesiredSize();
        return Desired;
    }

    // Move a label/control row's control between the same-row position
    // (Row 0 / Col 1) and the wrapped position (Row 1 / Col 0 spanning
    // both columns, indented by Indent so it aligns with the label).
    static void SetWrapRowLayout(
        winrt::Windows::UI::Xaml::FrameworkElement const& Element,
        bool Wrap,
        double Indent)
    {
        winrt::Windows::UI::Xaml::Controls::Grid::SetRow(
            Element, Wrap ? 1 : 0);
        winrt::Windows::UI::Xaml::Controls::Grid::SetColumn(
            Element, Wrap ? 0 : 1);
        winrt::Windows::UI::Xaml::Controls::Grid::SetColumnSpan(
            Element, Wrap ? 2 : 1);
        Element.Margin(Wrap
            ? winrt::Windows::UI::Xaml::Thickness(Indent, 8.0, 0.0, 0.0)
            : winrt::Windows::UI::Xaml::Thickness(6.0, 0.0, 0.0, 0.0));
    }

    void CompressPage::SetAllRows(bool LeftWrap, bool EncWrap)
    {
        SetWrapRowLayout(FormatCombo(), LeftWrap, 0.0);
        SetWrapRowLayout(LevelCombo(), LeftWrap, 0.0);
        SetWrapRowLayout(MethodCombo(), LeftWrap, 0.0);
        SetWrapRowLayout(DictionaryCombo(), LeftWrap, 0.0);
        SetWrapRowLayout(OrderCombo(), LeftWrap, 0.0);
        SetWrapRowLayout(SolidCombo(), LeftWrap, 0.0);
        SetWrapRowLayout(ThreadsCombo(), LeftWrap, 0.0);
        SetWrapRowLayout(MemoryCombo(), LeftWrap, 0.0);
        {
            // The memory usage detail always sits one row below the
            // "Memory" combo (label + combo side by side on top).
            const auto Value = MemoryValueText();
            winrt::Windows::UI::Xaml::Controls::Grid::SetRow(
                Value, LeftWrap ? 2 : 1);
            winrt::Windows::UI::Xaml::Controls::Grid::SetColumn(
                Value, LeftWrap ? 0 : 1);
            winrt::Windows::UI::Xaml::Controls::Grid::SetColumnSpan(
                Value, LeftWrap ? 2 : 1);
            Value.Margin(LeftWrap
                ? winrt::Windows::UI::Xaml::Thickness(0.0, 8.0, 0.0, 0.0)
                : winrt::Windows::UI::Xaml::Thickness(6.0, 4.0, 0.0, 0.0));
        }
        SetWrapRowLayout(VolumeCombo(), LeftWrap, 0.0);
        SetWrapRowLayout(ParametersBox(), LeftWrap, 0.0);
        SetWrapRowLayout(EncryptionMethodCombo(), EncWrap, 28.0);
    }

    void CompressPage::RecalcMinTrack()
    {
        if (!this->m_Context)
        {
            return;
        }

        // Measure the natural content sizes. The wrapped layout is the
        // narrowest the dialog can shrink to, so the track size and the
        // wrap thresholds come from these three measurements:
        //   1. everything wrapped        -> the minimum size
        //   2. left wrapped, encryption row still side by side
        //                                   -> encryption wrap threshold
        //   3. everything side by side   -> left column wrap threshold
        winrt::Windows::Foundation::Size Inf(100000.0f, 100000.0f);

        SetAllRows(true, true);
        this->Measure(Inf);
        const float WrappedW = this->DesiredSize().Width;
        const float WrappedH = this->DesiredSize().Height;

        SetAllRows(true, false);
        this->Measure(Inf);
        this->m_EncryptionWrapThresholdW =
            (double)this->DesiredSize().Width + 40.0;

        SetAllRows(false, false);
        this->Measure(Inf);
        this->m_LeftWrapThresholdW =
            (double)this->DesiredSize().Width + 40.0;

        // The minimum window size is the wrapped layout plus a little
        // slack, so the user can always squeeze the window far enough to
        // reach the wrap transition.
        const UINT Dpi = ::GetDpiForWindow(this->m_WindowHandle);
        const float Scale = (float)Dpi / (float)USER_DEFAULT_SCREEN_DPI;

        int MinClientW = (int)(((double)WrappedW + 32.0) * Scale + 0.5);
        int MinClientH = (int)((double)WrappedH * Scale + 0.5);
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
    }

    void CompressPage::AlignLeftLabelsColumn()
    {
        // Called after the panels have been measured: unify the label
        // column width of every left-column row to the widest label, so
        // all the combos start at the same x position.
        double MaxW = 0.0;
        for (auto const& Label : std::vector<winrt::Windows::UI::Xaml::Controls::TextBlock>{
                FormatText(), LevelText(), MethodText(), DictionaryText(),
                OrderText(), SolidText(), ThreadsText(), MemoryText(),
                VolumeText(), ParametersText() })
        {
            MaxW = (std::max)(MaxW, (double)Label.DesiredSize().Width);
        }
        if (MaxW <= 0.0)
        {
            return;
        }
        for (auto const& Row : std::vector<winrt::Windows::UI::Xaml::Controls::Grid>{
                FormatRow(), LevelRow(), MethodRow(), DictionaryRow(),
                OrderRow(), SolidRow(), ThreadsRow(), MemoryRow(),
                VolumeRow(), ParametersRow() })
        {
            const auto Defs = Row.ColumnDefinitions();
            if (Defs.Size() < 1)
            {
                continue;
            }
            Defs.GetAt(0).Width(
                winrt::Windows::UI::Xaml::GridLength(MaxW));
        }
    }

    void CompressPage::AdjustWindowHeightToContent()
    {
        if (!this->m_WindowHandle || !this->m_Context)
        {
            return;
        }
        if (this->ActualWidth() <= 0.0)
        {
            return;
        }

        // Measure the page under the current width: the height it wants is
        // the natural height of the current (wrapped or side-by-side)
        // layout, including the OK/Cancel row.
        try
        {
            winrt::Windows::Foundation::Size Constraint(
                (float)this->ActualWidth(), 100000.0f);
            this->Measure(Constraint);
            const float NeedH = this->DesiredSize().Height;

            RECT rcClient = {};
            ::GetClientRect(this->m_WindowHandle, &rcClient);
            const int CurClientH = rcClient.bottom - rcClient.top;
            const UINT Dpi = ::GetDpiForWindow(this->m_WindowHandle);
            const float Scale = (float)Dpi / (float)USER_DEFAULT_SCREEN_DPI;
            const int TargetClientH = (int)(NeedH * Scale + 0.5f);
            if (TargetClientH == CurClientH)
            {
                return;
            }

            RECT rcWin = {};
            ::GetWindowRect(this->m_WindowHandle, &rcWin);
            const LONG_PTR Style = ::GetWindowLongPtrW(
                this->m_WindowHandle, GWL_STYLE);
            const LONG_PTR ExStyle = ::GetWindowLongPtrW(
                this->m_WindowHandle, GWL_EXSTYLE);
            RECT rcAdj = { 0, 0, rcClient.right - rcClient.left, TargetClientH };
            ::AdjustWindowRectEx(&rcAdj, (DWORD)Style, FALSE, (DWORD)ExStyle);
            const int NewW = rcAdj.right - rcAdj.left;
            const int NewH = rcAdj.bottom - rcAdj.top;
            const int CurWinH = rcWin.bottom - rcWin.top;
            // Keep the bottom edge in place so the title bar never leaves the
            // screen when the window grows taller on wrap.
            const int NewY = rcWin.top - (NewH - CurWinH);
            ::SetWindowPos(
                this->m_WindowHandle,
                nullptr,
                rcWin.left,
                NewY,
                NewW,
                NewH,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        catch (...)
        {
            // Height snapping is a refinement; never let it kill the dialog.
        }
    }

    void CompressPage::UpdateRowLayouts()
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

        const bool LeftWrap = PageW < this->m_LeftWrapThresholdW;
        const bool EncWrap =
            PageW < this->m_EncryptionWrapThresholdW;

        // Recompute the thresholds and the minimum size only on the first
        // layout and on wrap-state changes, so dragging stays smooth.
        const bool StateChanged = this->m_FirstLayout ||
            LeftWrap != this->m_LeftWrapped ||
            EncWrap != this->m_EncryptionWrapped;
        if (StateChanged)
        {
            this->m_FirstLayout = false;
            this->RecalcMinTrack();
        }

        this->m_LeftWrapped = LeftWrap;
        this->m_EncryptionWrapped = EncWrap;
        SetAllRows(LeftWrap, EncWrap);

        // Snap the window height to the new content height after the new
        // layout is in place (bottom edge stays put).
        if (StateChanged)
        {
            this->AdjustWindowHeightToContent();
        }
    }

    void CompressPage::OnSizeChanged(
        winrt::IInspectable const& sender,
        winrt::SizeChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        this->UpdateRowLayouts();
    }

    void CompressPage::OnLoaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        this->m_InitGuard = false;

        // Esc must close the dialog even when the focus sits inside an
        // editable control (the control marks the key as handled, which
        // would otherwise swallow the routed event). handledEventsToo=true
        // catches it in the bubbling phase anyway. Registered here (after
        // the XAML tree exists) rather than in the constructor, and
        // guarded, so a registration failure can never abort startup.
        try
        {
            this->AddHandler(
                winrt::Windows::UI::Xaml::UIElement::KeyDownEvent(),
                winrt::Windows::UI::Xaml::Input::KeyEventHandler{
                    this, &CompressPage::OnPageKeyDown }
                    .as<winrt::Windows::Foundation::IInspectable>(),
                true);
        }
        catch (...)
        {
            // Non-fatal: the Win32 window subclass still handles Esc.
        }
    }

    void CompressPage::OnUnloaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        // The window can be closed with the X button or Alt+F4, which never
        // passes through OnCancelClicked. Treat every close that was not
        // confirmed by OK as a cancel.
        if (this->m_Context && !this->m_OkClicked)
        {
            this->m_Context->OK = FALSE;
        }
    }

    void CompressPage::PostClose(bool Ok)
    {
        if (this->m_Context)
        {
            this->m_Context->OK = Ok ? TRUE : FALSE;
        }
        this->m_OkClicked = Ok;
        ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }

    void CompressPage::OnPageKeyDown(
        winrt::IInspectable const& sender,
        winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        if (e.Key() == winrt::Windows::System::VirtualKey::Escape)
        {
            e.Handled(true);
            if (this->m_Context)
            {
                this->m_Context->OK = FALSE;
            }
            this->m_OkClicked = false;
            ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
        }
    }

    void CompressPage::OnFormatChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_FORMAT,
            this->m_Context->Formats.Items[Sel].Value);
        RefreshFromSnapshot();
    }

    void CompressPage::OnLevelChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_LEVEL,
            this->m_Context->Levels.Items[Sel].Value);
        RefreshFromSnapshot();
    }

    void CompressPage::OnMethodChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_METHOD,
            this->m_Context->Methods.Items[Sel].Value,
            this->m_Context->Methods.Items[Sel].SemanticText);
        RefreshFromSnapshot();
    }

    void CompressPage::OnDictionaryChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_DICTIONARY,
            this->m_Context->Dictionaries.Items[Sel].Value);
        RefreshFromSnapshot();
    }

    void CompressPage::OnOrderChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_ORDER,
            this->m_Context->Orders.Items[Sel].Value);
        RefreshFromSnapshot();
    }

    void CompressPage::OnSolidChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_SOLID,
            this->m_Context->SolidBlocks.Items[Sel].Value);
        RefreshFromSnapshot();
    }

    void CompressPage::OnThreadsChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_THREADS,
            this->m_Context->Threads.Items[Sel].Value);
        RefreshFromSnapshot();
    }

    void CompressPage::OnMemoryChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_MEMORY,
            this->m_Context->MemoryLimits.Items[Sel].Value,
            this->m_Context->MemoryLimits.Items[Sel].SemanticText);
        RefreshFromSnapshot();
    }

    void CompressPage::OnUpdateModeChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_UPDATE_MODE,
            this->m_Context->UpdateModes.Items[Sel].Value);
        RefreshFromSnapshot();
    }

    void CompressPage::OnPathModeChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_PATH_MODE,
            this->m_Context->PathModes.Items[Sel].Value);
        RefreshFromSnapshot();
    }

    void CompressPage::OnEncryptionMethodChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_ENCRYPTION_METHOD,
            this->m_Context->EncryptionMethods.Items[Sel].Value,
            this->m_Context->EncryptionMethods.Items[Sel].SemanticText);
        RefreshFromSnapshot();
    }

    // The current path goes first, followed by the history
    // (deduplicated), so the combo never hides what is shown in the box.
    void CompressPage::FillArchivePathHistory()
    {
        if (!this->m_Context)
        {
            return;
        }

        PK7_COMPRESS_DIALOG_CONTEXT Context = this->m_Context;

        std::wstring Current = ArchivePathCombo().Text().c_str();
        if (!Current.empty())
        {
            ArchivePathCombo().Items().Append(winrt::box_value(
                winrt::hstring(Current)));
        }
        for (UINT32 i = 0; i < Context->NumPaths && i < 16; i++)
        {
            if (Context->Paths[i][0] &&
                Context->Paths[i] != Current)
            {
                ArchivePathCombo().Items().Append(winrt::box_value(
                    winrt::hstring(Context->Paths[i])));
            }
        }
    }

    void CompressPage::OnArchivePathDropDownOpened(
        winrt::IInspectable const& sender,
        winrt::IInspectable const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        this->m_PathTextSnapshot =
            ArchivePathCombo().Text().c_str();

        // Show the "x" on every history entry once the drop-down is open,
        // but never on the first entry (the current path). Containers are
        // generated asynchronously, so defer one dispatch.
        this->Dispatcher().TryRunAsync(
            winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
            [this]()
        {
            const uint32_t Count = ArchivePathCombo().Items().Size();
            for (uint32_t i = 0; i < Count; i++)
            {
                auto Container =
                    ArchivePathCombo().ContainerFromIndex((int)i);
                if (!Container)
                {
                    continue;
                }
                std::vector<winrt::Windows::UI::Xaml::Controls::Button>
                    Buttons;
                FindVisualChildren(Container, Buttons);
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

    void CompressPage::OnArchivePathDropDownClosed(
        winrt::IInspectable const& sender,
        winrt::IInspectable const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (!this->m_PathTextSnapshot.empty() &&
            ArchivePathCombo().Text().empty())
        {
            ArchivePathCombo().Text(
                winrt::hstring(this->m_PathTextSnapshot));
        }

        // Hide every "x" again, including the one in the closed selection
        // renderer which would otherwise show in the box.
        std::vector<winrt::Windows::UI::Xaml::Controls::Button> Buttons;
        FindVisualChildren(ArchivePathCombo(), Buttons);
        for (auto& B : Buttons)
        {
            B.Visibility(winrt::Windows::UI::Xaml::Visibility::Collapsed);
        }
    }

    void CompressPage::OnDeleteHistoryPathClicked(
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
        std::wstring Path = winrt::unbox_value_or<
            winrt::hstring>(Data, winrt::hstring()).c_str();

        // The first entry mirrors the current path in the box; it is not a
        // history entry, so its "x" does nothing.
        if (Path.empty() ||
            Path == ArchivePathCombo().Text().c_str())
        {
            return;
        }

        // Remove the entry from the drop-down list.
        const auto& Items = ArchivePathCombo().Items();
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

    void CompressPage::OnArchivePathChanged(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        std::wstring Path = ArchivePathCombo().Text().c_str();
        wcscpy_s(this->m_Context->ArchivePath, Path.c_str());
        SendCommand(
            K7_COMPRESS_COMMAND_ARCHIVE_PATH,
            0,
            this->m_Context->ArchivePath);
    }

    void CompressPage::OnParametersChanged(
        winrt::IInspectable const& sender,
        winrt::TextChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        std::wstring Text = ParametersBox().Text().c_str();
        wcscpy_s(this->m_Context->Parameters, Text.c_str());
        SendCommand(
            K7_COMPRESS_COMMAND_PARAMETERS,
            0,
            this->m_Context->Parameters);
    }

    void CompressPage::OnVolumeChanged(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        std::wstring Text = VolumeCombo().Text().c_str();
        wcscpy_s(this->m_Context->VolumeText, Text.c_str());
        SendCommand(
            K7_COMPRESS_COMMAND_VOLUME,
            0,
            this->m_Context->VolumeText);
    }

    void CompressPage::OnShowPasswordClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (!this->m_Context)
        {
            return;
        }
        this->m_Context->ShowPassword = GetBool(ShowPasswordCheck().IsChecked())
            ? TRUE : FALSE;
        UpdatePasswordControl();
    }

    void CompressPage::OnSfxClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        this->m_Context->SfxMode = GetBool(SfxCheck().IsChecked()) ? TRUE : FALSE;
        SendCommand(K7_COMPRESS_COMMAND_SFX, this->m_Context->SfxMode);
        RefreshFromSnapshot();
    }

    void CompressPage::OnSharedClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        this->m_Context->OpenShareForWrite =
            GetBool(SharedCheck().IsChecked()) ? TRUE : FALSE;
        SendCommand(K7_COMPRESS_COMMAND_SHARED, this->m_Context->OpenShareForWrite);
    }

    void CompressPage::OnDeleteClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        this->m_Context->DeleteAfterCompressing =
            GetBool(DeleteCheck().IsChecked()) ? TRUE : FALSE;
        SendCommand(K7_COMPRESS_COMMAND_DELETE, this->m_Context->DeleteAfterCompressing);
    }

    void CompressPage::OnEncryptHeadersClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        this->m_Context->EncryptHeaders =
            GetBool(EncryptHeadersCheck().IsChecked()) ? TRUE : FALSE;
        SendCommand(K7_COMPRESS_COMMAND_ENCRYPT_HEADERS,
            this->m_Context->EncryptHeaders);
    }

    void CompressPage::OnBrowseClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (!this->m_Context)
        {
            return;
        }
        SendCommand(K7_COMPRESS_COMMAND_BROWSE_ARCHIVE, 0, this->m_Context->ArchivePath);
        RefreshFromSnapshot();
    }

    void CompressPage::OnOptionsClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (!this->m_Context)
        {
            return;
        }
        if (this->m_Context->OptionsCallback)
        {
            this->m_Context->OptionsCallback(this->m_Context->CallbackContext);
        }
        RefreshFromSnapshot();
    }

    void CompressPage::OnOkClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (!this->m_Context)
        {
            PostClose(false);
            return;
        }

        // Capture the password fields into the context before submitting.
        {
            std::wstring Password = PasswordBox().Password().c_str();
            std::wstring Confirmation = ReenterPasswordBox().Password().c_str();
            wcscpy_s(this->m_Context->Password, Password.c_str());
            wcscpy_s(this->m_Context->PasswordConfirmation, Confirmation.c_str());
        }

        if (this->m_Context->CommandCallback)
        {
            const bool Accepted = this->m_Context->CommandCallback(
                this->m_Context->CallbackContext,
                K7_COMPRESS_COMMAND_SUBMIT,
                0,
                nullptr);
            if (!Accepted)
            {
                // The callback wrote ErrorText; surface it and stay open.
                RefreshFromSnapshot();
                return;
            }
        }

        // Merge the archive path into the history: current first, unique,
        // capped at 16 entries, so the drop-down has content next time.
        {
            std::wstring PathText =
                ArchivePathCombo().Text().c_str();
            TrimString(PathText);
            std::vector<std::wstring> History;
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
                    std::wstring A = Item;
                    std::wstring B = T;
                    std::transform(A.begin(), A.end(), A.begin(), ::towlower);
                    std::transform(B.begin(), B.end(), B.begin(), ::towlower);
                    if (A == B)
                    {
                        return;
                    }
                }
                History.push_back(T);
            };
            AddUnique(PathText);
            for (UINT32 i = 0;
                i < this->m_Context->NumPaths && i < 16; i++)
            {
                AddUnique(this->m_Context->Paths[i]);
            }
            UINT32 Num = 0;
            for (auto const& Item : History)
            {
                if (Num >= 16)
                {
                    break;
                }
                wcscpy_s(this->m_Context->Paths[Num], Item.c_str());
                Num++;
            }
            this->m_Context->NumPaths = Num;
        }

        PostClose(true);
    }

    void CompressPage::OnCancelClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        PostClose(false);
    }
}
