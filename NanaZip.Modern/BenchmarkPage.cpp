#include "pch.h"
#include "BenchmarkPage.h"
#if __has_include("BenchmarkPage.g.cpp")
#include "BenchmarkPage.g.cpp"
#endif

#include "NanaZip.Modern.h"

#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.h>

#include <string>

namespace winrt::NanaZip::Modern::implementation
{
    // 7-Zip resource IDs used for the localized labels (the XAML side has
    // no other access to them). K7ModernGetLegacyStringResource is tried
    // first; the English fallback matches the Win32 dialog template.
    static const UINT32 kIdDictLabel = 4006;      // IDT_BENCH_DICTIONARY
    static const UINT32 kIdThreadsLabel = 4009;   // IDT_BENCH_NUM_THREADS
    static const UINT32 kIdPassesLabel = 7610;    // IDT_BENCH_PASSES
    static const UINT32 kIdCompressing = 7602;    // IDG_BENCH_COMPRESSING
    static const UINT32 kIdDecompressing = 7603;  // IDG_BENCH_DECOMPRESSING
    static const UINT32 kIdTotalRating = 7605;    // IDG_BENCH_TOTAL_RATING
    static const UINT32 kIdSpeed = 3903;          // IDT_BENCH_SPEED
    static const UINT32 kIdRating = 7604;         // IDT_BENCH_RATING_LABEL
    static const UINT32 kIdUsage = 7608;          // IDT_BENCH_USAGE_LABEL
    static const UINT32 kIdRpu = 7609;            // IDT_BENCH_RPU_LABEL
    static const UINT32 kIdSize = 1007;           // IDT_BENCH_SIZE
    static const UINT32 kIdCurrent = 7606;        // IDT_BENCH_CURRENT
    static const UINT32 kIdResulting = 7607;      // IDT_BENCH_RESULTING
    static const UINT32 kIdElapsed = 3900;        // IDT_BENCH_ELAPSED
    static const UINT32 kIdStop = 442;            // IDB_STOP
    static const UINT32 kIdRestart = 443;         // IDB_RESTART

    BenchmarkPage::BenchmarkPage(
        _In_opt_ HWND WindowHandle,
        _In_ PK7_BENCHMARK_DIALOG_CONTEXT Context) :
        m_WindowHandle(WindowHandle),
        m_Context(Context)
    {
        this->Loaded({ this, &BenchmarkPage::OnLoaded });
    }

    void BenchmarkPage::InitializeComponent()
    {
        BenchmarkPageT::InitializeComponent();
    }

    winrt::hstring BenchmarkPage::Res(
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

    winrt::hstring BenchmarkPage::RemoveMnemonic(
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

    void BenchmarkPage::ApplyDialogFont(UINT32 Pt)
    {
        double FontSizePx = (Pt == 0) ? 0.0 : (double)Pt * 96.0 / 72.0;
        ApplyFontToTree(*this, FontSizePx);
    }

    void BenchmarkPage::ApplyFontToTree(
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

    void BenchmarkPage::PostCommand(UINT32 CommandId)
    {
        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(CommandId, BN_CLICKED),
            0);
    }

    winrt::Windows::Foundation::Size BenchmarkPage::PrepareForShow()
    {
        if (!this->m_Context)
        {
            return winrt::Windows::Foundation::Size(560, 480);
        }

        PK7_BENCHMARK_DIALOG_CONTEXT Context = this->m_Context;

        // Dialog title comes from the caller (it owns the language strings).
        ::SetWindowTextW(
            this->m_WindowHandle,
            Context->TotalMode ? L"Benchmark" : L"Benchmark");

        // Labels (Res with the 7-Zip IDs, English fallback).
        DictLabelText().Text(RemoveMnemonic(
            Res(kIdDictLabel, L"Dictionary")));
        ThreadsLabelText().Text(RemoveMnemonic(
            Res(kIdThreadsLabel, L"Num Threads")));
        PassesLabelText().Text(RemoveMnemonic(
            Res(kIdPassesLabel, L"Passes")));
        CompressingLabelText().Text(RemoveMnemonic(
            Res(kIdCompressing, L"Compressing")));
        DecompressingLabelText().Text(RemoveMnemonic(
            Res(kIdDecompressing, L"Decompressing")));
        TotalRatingLabelText().Text(RemoveMnemonic(
            Res(kIdTotalRating, L"Total Rating")));
        SpeedLabelText().Text(RemoveMnemonic(
            Res(kIdSpeed, L"Speed")));
        RatingLabelText().Text(RemoveMnemonic(
            Res(kIdRating, L"Rating")));
        UsageLabelText().Text(RemoveMnemonic(
            Res(kIdUsage, L"Usage")));
        RpuLabelText().Text(RemoveMnemonic(
            Res(kIdRpu, L"RPU")));
        SizeLabelText().Text(RemoveMnemonic(
            Res(kIdSize, L"Size")));
        CurrentLabelText().Text(RemoveMnemonic(
            Res(kIdCurrent, L"Current")));
        ResultingLabelText().Text(RemoveMnemonic(
            Res(kIdResulting, L"Resulting")));
        StopButton().Content(winrt::box_value(RemoveMnemonic(
            Res(kIdStop, L"Stop"))));
        RestartButton().Content(winrt::box_value(RemoveMnemonic(
            Res(kIdRestart, L"Restart"))));

        // System information.
        VersionText().Text(winrt::hstring(Context->Version));
        CpuText().Text(winrt::hstring(Context->Cpu));
        Sys1Text().Text(winrt::hstring(Context->Sys1));
        Sys2Text().Text(winrt::hstring(Context->Sys2));
        FeaturesText().Text(winrt::hstring(Context->Features));
        HardwareText().Text(winrt::hstring(Context->HardwareThreads));

        // Combos.
        for (UINT32 i = 0; i < Context->DictItemsCount; i++)
        {
            DictCombo().Items().Append(winrt::box_value(
                winrt::hstring(Context->DictItems[i])));
        }
        DictCombo().SelectedIndex(Context->DictIndex);
        for (UINT32 i = 0; i < Context->ThreadItemsCount; i++)
        {
            ThreadsCombo().Items().Append(winrt::box_value(
                winrt::hstring(Context->ThreadItems[i])));
        }
        ThreadsCombo().SelectedIndex(Context->ThreadIndex);
        for (UINT32 i = 0; i < Context->PassesItemsCount; i++)
        {
            PassesCombo().Items().Append(winrt::box_value(
                winrt::hstring(Context->PassesItems[i])));
        }
        PassesCombo().SelectedIndex(Context->PassesIndex);

        // Memory usage for the initial dictionary selection.
        if (Context->DictIndex < Context->DictItemsCount)
        {
            MemoryText().Text(winrt::hstring(
                Context->DictMemoryItems[Context->DictIndex]));
        }
        ElapsedText().Text(L"");
        LogTextBox().Text(L"");

        // Total mode: log-only window.
        if (Context->TotalMode)
        {
            SystemInfoPanel().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Collapsed);
            OptionsGrid().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Collapsed);
            CompressSection().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Collapsed);
            DecompressSection().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Collapsed);
            TotalSection().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Collapsed);
            Divider1().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Collapsed);
            Divider2().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Collapsed);
            Divider3().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Collapsed);
            Divider4().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Collapsed);
            Divider5().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Collapsed);
            LogTextBox().MinHeight(300);
            LogTextBox().MaxHeight(600);
        }
        else
        {
            // The log is not part of the Win32 graphical dialog layout,
            // but keeping it visible shows the rating vector lines.
            LogLabelText().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Collapsed);
            LogTextBox().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Collapsed);
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
        if (MinClientW < 520) MinClientW = 520;
        if (MinClientH < 360) MinClientH = 360;

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

    void BenchmarkPage::ApplyStatus(
        _In_ PK7_BENCHMARK_STATUS Status)
    {
        if (!Status)
        {
            return;
        }

        if (Status->Elapsed[0])
        {
            ElapsedText().Text(winrt::hstring(Status->Elapsed));
        }

        EncSpeed1Text().Text(winrt::hstring(Status->EncSpeed1));
        EncSpeed2Text().Text(winrt::hstring(Status->EncSpeed2));
        EncRating1Text().Text(winrt::hstring(Status->EncRating1));
        EncRating2Text().Text(winrt::hstring(Status->EncRating2));
        EncUsage1Text().Text(winrt::hstring(Status->EncUsage1));
        EncUsage2Text().Text(winrt::hstring(Status->EncUsage2));
        EncRpu1Text().Text(winrt::hstring(Status->EncRpu1));
        EncRpu2Text().Text(winrt::hstring(Status->EncRpu2));
        EncSize1Text().Text(winrt::hstring(Status->EncSize1));
        EncSize2Text().Text(winrt::hstring(Status->EncSize2));

        DecSpeed1Text().Text(winrt::hstring(Status->DecSpeed1));
        DecSpeed2Text().Text(winrt::hstring(Status->DecSpeed2));
        DecRating1Text().Text(winrt::hstring(Status->DecRating1));
        DecRating2Text().Text(winrt::hstring(Status->DecRating2));
        DecUsage1Text().Text(winrt::hstring(Status->DecUsage1));
        DecUsage2Text().Text(winrt::hstring(Status->DecUsage2));
        DecRpu1Text().Text(winrt::hstring(Status->DecRpu1));
        DecRpu2Text().Text(winrt::hstring(Status->DecRpu2));
        DecSize1Text().Text(winrt::hstring(Status->DecSize1));
        DecSize2Text().Text(winrt::hstring(Status->DecSize2));

        TotalRatingText().Text(winrt::hstring(Status->TotalRating));
        TotalRpuText().Text(winrt::hstring(Status->TotalRpu));
        TotalUsageText().Text(winrt::hstring(Status->TotalUsage));

        if (Status->Log[0])
        {
            LogTextBox().Text(winrt::hstring(Status->Log));
        }

        // Stop is enabled while running, disabled when finished/stopped
        // (mirrors the Win32 Disable_Stop_Button).
        StopButton().IsEnabled(Status->Running);

        if (Status->HasError)
        {
            ErrorText().Text(winrt::hstring(Status->Error));
            ErrorText().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Visible);
        }
    }

    void BenchmarkPage::OnLoaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        // Auto-start like the Win32 dialog (RestartBenchmark in OnInit).
        PostCommand(K7_BENCH_WINDOW_COMMAND_RESTART);
    }

    void BenchmarkPage::OnDictSelectionChanged(
        winrt::IInspectable const& sender,
        winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        if (!this->m_Context)
        {
            return;
        }
        int Index = DictCombo().SelectedIndex();
        if (Index < 0)
        {
            return;
        }
        if ((UINT32)Index < this->m_Context->DictItemsCount)
        {
            MemoryText().Text(winrt::hstring(
                this->m_Context->DictMemoryItems[Index]));
        }
        ::PostMessageW(
            this->m_WindowHandle,
            K7_BENCH_WINDOW_MSG_SET_DICTIONARY,
            (WPARAM)Index,
            0);
    }

    void BenchmarkPage::OnThreadsSelectionChanged(
        winrt::IInspectable const& sender,
        winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        int Index = ThreadsCombo().SelectedIndex();
        if (Index < 0)
        {
            return;
        }
        ::PostMessageW(
            this->m_WindowHandle,
            K7_BENCH_WINDOW_MSG_SET_THREADS,
            (WPARAM)Index,
            0);
    }

    void BenchmarkPage::OnPassesSelectionChanged(
        winrt::IInspectable const& sender,
        winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        int Index = PassesCombo().SelectedIndex();
        if (Index < 0)
        {
            return;
        }
        ::PostMessageW(
            this->m_WindowHandle,
            K7_BENCH_WINDOW_MSG_SET_PASSES,
            (WPARAM)Index,
            0);
    }

    void BenchmarkPage::OnStopClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        PostCommand(K7_BENCH_WINDOW_COMMAND_STOP);
    }

    void BenchmarkPage::OnRestartClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        PostCommand(K7_BENCH_WINDOW_COMMAND_RESTART);
    }

    void BenchmarkPage::OnPageKeyDown(
        winrt::IInspectable const& sender,
        winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        if (e.Key() == winrt::Windows::System::VirtualKey::Escape)
        {
            // Esc asks to cancel (the 7-Zip side stops the thread and
            // closes the window after the cleanup, like the Win32 OnCancel).
            e.Handled(true);
            PostCommand(K7_BENCH_WINDOW_COMMAND_CANCEL);
        }
    }
}
