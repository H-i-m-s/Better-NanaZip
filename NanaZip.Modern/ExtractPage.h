#pragma once

#include "ExtractPage.g.h"

#include <Windows.h>

#include "NanaZip.Modern.h"

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct ExtractPage : ExtractPageT<ExtractPage>
    {
    public:

        ExtractPage(
            _In_opt_ HWND WindowHandle = nullptr,
            _In_ PK7_EXTRACT_DIALOG_CONTEXT Context = nullptr);

        void InitializeComponent();

        // Applies the dialog font and measures the content so the caller
        // can size the window before it is shown (avoids a visible resize
        // after the dialog appears). Returns the desired client size in DIPs.
        winrt::Windows::Foundation::Size PrepareForShow();

        void OnUnloaded(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnLoaded(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnBrowseClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnNameEnableClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnShowPasswordClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnElimDupClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnNtSecurityClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnOpenFolderClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnDeleteAfterClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnOkClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnCancelClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

    private:

        winrt::hstring Res(
            UINT32 ResourceId,
            LPCWSTR Fallback);

        void ApplyDialogFont(UINT32 Pt);

        void ApplyFontToTree(
            winrt::Windows::UI::Xaml::DependencyObject const& Node,
            double FontSizePx);

        void UpdatePasswordControl();

        bool GetBoolsVal(
            BOOLEAN Def1, BOOLEAN Val1,
            BOOLEAN Def2, BOOLEAN Val2) const;

        void SetBoolsResult(
            bool Value,
            BOOLEAN& Def1, BOOLEAN& Val1,
            BOOLEAN& Def2, BOOLEAN& Val2) const;

        HWND m_WindowHandle;
        PK7_EXTRACT_DIALOG_CONTEXT m_Context;
        bool m_InitGuard;
        bool m_OkClicked;
    };
}
