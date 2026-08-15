#pragma once

#include "ComboPage.g.h"

#include <Windows.h>

#include <string>

#include "NanaZip.Modern.h"

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct ComboPage : ComboPageT<ComboPage>
    {
    public:

        ComboPage(
            _In_opt_ HWND WindowHandle = nullptr,
            _In_ PK7_COMBO_DIALOG_CONTEXT Context = nullptr);

        void InitializeComponent();

        // Applies the dialog font and measures the content so the caller
        // can size the window before it is shown (avoids a visible resize
        // after the dialog appears). Returns the desired client size in DIPs.
        winrt::Windows::Foundation::Size PrepareForShow();

        void OnLoaded(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnUnloaded(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnComboDropDownOpened(
            winrt::IInspectable const& sender,
            winrt::IInspectable const& e);

        void OnComboDropDownClosed(
            winrt::IInspectable const& sender,
            winrt::IInspectable const& e);

        void OnComboKeyDown(
            winrt::IInspectable const& sender,
            winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        void OnOkClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnCancelClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnPageKeyDown(
            winrt::IInspectable const& sender,
            winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

    private:

        winrt::hstring Res(
            UINT32 ResourceId,
            LPCWSTR Fallback);

        winrt::hstring RemoveMnemonic(
            winrt::hstring const& Text);

        void ApplyDialogFont(UINT32 Pt);

        void ApplyFontToTree(
            winrt::Windows::UI::Xaml::DependencyObject const& Node,
            double FontSizePx);

        HWND m_WindowHandle;
        PK7_COMBO_DIALOG_CONTEXT m_Context;
        bool m_OkClicked;
        std::wstring m_TextSnapshot;
    };
}
