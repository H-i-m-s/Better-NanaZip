#pragma once

#include "CompressPage.g.h"

#include <string>

#include <Windows.h>

#include "NanaZip.Modern.h"

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
    using Windows::UI::Xaml::SizeChangedEventArgs;
    using Windows::UI::Xaml::Controls::SelectionChangedEventArgs;
    using Windows::UI::Xaml::Controls::TextChangedEventArgs;
    using Windows::UI::Xaml::Input::KeyRoutedEventArgs;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct CompressPage : CompressPageT<CompressPage>
    {
    public:

        CompressPage(
            _In_opt_ HWND WindowHandle = nullptr,
            _In_ PK7_COMPRESS_DIALOG_CONTEXT Context = nullptr);

        void InitializeComponent();

        winrt::Windows::Foundation::Size PrepareForShow();

        void OnLoaded(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnUnloaded(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        // Reacts to window resizing: the left column rows keep their label
        // and combo side by side while the window is wide enough, then all
        // switch together to the wrapped layout (combo below the label,
        // flush left) once the window is squeezed past the threshold. The
        // encryption-method row wraps only when the window gets very
        // narrow, as the user asked.
        void OnSizeChanged(
            winrt::IInspectable const& sender,
            winrt::SizeChangedEventArgs const& e);

        void OnFormatChanged(
            winrt::IInspectable const& sender,
            winrt::SelectionChangedEventArgs const& e);

        void OnLevelChanged(
            winrt::IInspectable const& sender,
            winrt::SelectionChangedEventArgs const& e);

        void OnMethodChanged(
            winrt::IInspectable const& sender,
            winrt::SelectionChangedEventArgs const& e);

        void OnDictionaryChanged(
            winrt::IInspectable const& sender,
            winrt::SelectionChangedEventArgs const& e);

        void OnOrderChanged(
            winrt::IInspectable const& sender,
            winrt::SelectionChangedEventArgs const& e);

        void OnSolidChanged(
            winrt::IInspectable const& sender,
            winrt::SelectionChangedEventArgs const& e);

        void OnThreadsChanged(
            winrt::IInspectable const& sender,
            winrt::SelectionChangedEventArgs const& e);

        void OnMemoryChanged(
            winrt::IInspectable const& sender,
            winrt::SelectionChangedEventArgs const& e);

        void OnUpdateModeChanged(
            winrt::IInspectable const& sender,
            winrt::SelectionChangedEventArgs const& e);

        void OnPathModeChanged(
            winrt::IInspectable const& sender,
            winrt::SelectionChangedEventArgs const& e);

        void OnEncryptionMethodChanged(
            winrt::IInspectable const& sender,
            winrt::SelectionChangedEventArgs const& e);

        void OnArchivePathChanged(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnArchivePathDropDownOpened(
            winrt::IInspectable const& sender,
            winrt::IInspectable const& e);

        void OnArchivePathDropDownClosed(
            winrt::IInspectable const& sender,
            winrt::IInspectable const& e);

        void OnDeleteHistoryPathClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnParametersChanged(
            winrt::IInspectable const& sender,
            winrt::TextChangedEventArgs const& e);

        void OnVolumeChanged(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnShowPasswordClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnSfxClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnSharedClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnDeleteClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnEncryptHeadersClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnBrowseClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnOptionsClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnOkClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnCancelClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnPageKeyDown(
            winrt::IInspectable const& sender,
            winrt::KeyRoutedEventArgs const& e);

    private:

        winrt::hstring Res(
            UINT32 ResourceId,
            LPCWSTR Fallback);

        void ApplyDialogFont(UINT32 Pt);

        void ApplyFontToTree(
            winrt::Windows::UI::Xaml::DependencyObject const& Node,
            double FontSizePx);

        void UpdatePasswordControl();

        // Fill the archive-path drop-down: the current path first, then the
        // history (deduplicated, capped at 16), mirroring the extract page.
        void FillArchivePathHistory();
        void ApplyOptionList(
            winrt::Windows::UI::Xaml::Controls::ComboBox const& Combo,
            _In_ const K7_COMPRESS_OPTION_LIST& List);
        bool SendCommand(
            UINT32 Command,
            INT64 Value,
            LPCWSTR SemanticText = nullptr);
        void PostClose(bool Ok);

        void ApplyLabels();

        // Fills the dialog from m_Context. The caller sets m_InitGuard so
        // the fill does not look like user input.
        void ApplySnapshotToUi();

        // Apply the wrap state of every label/control row: LeftWrap for the
        // left column rows (they wrap together), EncWrap for the
        // encryption-method row (it wraps much later), RightWrap for the
        // right column's Update mode / Path mode rows.
        void SetAllRows(bool LeftWrap, bool EncWrap, bool RightWrap);

        // Recompute the wrap thresholds from the natural content sizes and
        // refresh Context->MinTrackW/H from the fully wrapped layout, so
        // the minimum window size never blocks the wrap transition. Called
        // only on the first layout and on wrap-state changes, not on every
        // resize tick.
        void RecalcMinTrack();

        // Decide the wrap states from the current page width and apply
        // them; called from OnSizeChanged.
        void UpdateRowLayouts();

        // Unify the left column label column width to the widest label so
        // every combo starts at the same x position (measured after
        // Measure, applied before the final measure in PrepareForShow).
        void AlignLeftLabelsColumn();
        void AlignRightLabelsColumn();

        // Refresh the whole dialog from m_Context with the init guard set.
        // Event callbacks must use this instead of ApplySnapshotToUi():
        // refilling ComboBox items clears and re-selects, which re-fires
        // SelectionChanged; without the guard that recurses forever
        // (stack overflow, 0xc00000fd).
        void RefreshFromSnapshot();

        HWND m_WindowHandle;
        PK7_COMPRESS_DIALOG_CONTEXT m_Context;
        bool m_InitGuard;
        bool m_OkClicked;

        // Layout state for the wrap-on-shrink behavior.
        bool m_FirstLayout;
        bool m_LeftWrapped;
        bool m_EncryptionWrapped;
        bool m_RightWrapped;
        double m_LeftWrapThresholdW;
        double m_EncryptionWrapThresholdW;
        double m_RightWrapThresholdW;

        // Text shown in the editable archive-path combo when its drop-down
        // opens; restored if the drop-down clears it.
        std::wstring m_PathTextSnapshot;
    };
}
