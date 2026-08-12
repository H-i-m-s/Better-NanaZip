#pragma once

#include "CompressPage.g.h"

#include <Windows.h>

#include "NanaZip.Modern.h"

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
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
    };
}
