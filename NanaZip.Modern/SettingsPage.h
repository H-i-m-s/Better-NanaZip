#pragma once

#include "SettingsPage.g.h"

#include <Windows.h>

#include "NanaZip.Modern.h"

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
    using Windows::UI::Xaml::Controls::SelectionChangedEventArgs;
    using Windows::UI::Xaml::Controls::TextChangedEventArgs;
    using Windows::UI::Xaml::Input::KeyRoutedEventArgs;
    using Windows::UI::Xaml::SizeChangedEventArgs;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage>
    {
    public:

        SettingsPage(
            _In_opt_ HWND WindowHandle = nullptr,
            _In_ PK7_SETTINGS_DIALOG_CONTEXT Context = nullptr);

        void InitializeComponent();

        winrt::Windows::Foundation::Size PrepareForShow();

        void OnPageKeyDown(
            winrt::IInspectable const& sender,
            winrt::KeyRoutedEventArgs const& e);

        void TabButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void ApplyButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OkButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void CancelButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnUnloaded(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnLoaded(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        // Settings page
        void SettingsCheckBoxClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void FontComboChanged(
            winrt::IInspectable const& sender,
            winrt::SelectionChangedEventArgs const& e);

        void FontDialogComboChanged(
            winrt::IInspectable const& sender,
            winrt::SelectionChangedEventArgs const& e);

        // Integration (menu) page
        void MenuAssociateButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void MenuCheckBoxClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void MenuZoneComboChanged(
            winrt::IInspectable const& sender,
            winrt::SelectionChangedEventArgs const& e);

        // Folders page
        void WorkModeRadioClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void WorkPathTextChanged(
            winrt::IInspectable const& sender,
            winrt::TextChangedEventArgs const& e);

        void WorkPathBrowseButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void FoldersCheckBoxClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        // Editor page
        void EditorPathTextChanged(
            winrt::IInspectable const& sender,
            winrt::TextChangedEventArgs const& e);

        void EditorBrowseButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        // Extract settings page
        void ExtractCheckBoxClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void ExtractApiTextChanged(
            winrt::IInspectable const& sender,
            winrt::TextChangedEventArgs const& e);

        void ExtractMatchPriorityChanged(
            winrt::IInspectable const& sender,
            winrt::SelectionChangedEventArgs const& e);

        void ExtractBookTextChanged(
            winrt::IInspectable const& sender,
            winrt::TextChangedEventArgs const& e);

        void ExtractImportBookClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

    private:

        void OnSizeChanged(
            winrt::IInspectable const& sender,
            winrt::SizeChangedEventArgs const& e);

        HWND m_WindowHandle;
        PK7_SETTINGS_DIALOG_CONTEXT m_Context;
        bool m_InitGuard;

        winrt::Windows::UI::Xaml::Controls::CheckBox m_MenuChecks[13];

        static winrt::hstring Res(UINT32 ResourceId, LPCWSTR Fallback);
        void ApplyDialogFont(UINT32 Pt);
        void ApplyFontToTree(
            winrt::Windows::UI::Xaml::DependencyObject const& Node,
            double FontSizePx);
        void UpdateWindowRect();
        void SwitchTab(int Index);
        void InitFontCombo(
            winrt::Windows::UI::Xaml::Controls::ComboBox const& Combo,
            UINT32 Pt);

        // Layout helpers: the label/control rows always stay on one line
        // (the control follows its label); the label column widths are
        // re-measured whenever the dialog font size changes, so a larger
        // font widens the label column instead of covering the control.
        void AlignLabels();
        void RecalcMinTrack();
        void RefreshAfterFontChange();

        // Measures the actual vertical scrollbar width at runtime (the
        // system "auto-hide scrollbars" setting changes it) and caches it
        // for the width candidate: the association button needs the
        // scrollbar width added so the scrollbar never covers its right
        // edge.
        void MeasureScrollBarWidth();

        double m_ScrollBarW;
        bool m_ScrollBarMeasured;
    };
}
