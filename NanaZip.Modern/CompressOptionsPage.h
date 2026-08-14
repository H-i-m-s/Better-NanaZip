#pragma once

#include "CompressOptionsPage.g.h"

#include <Windows.h>

#include "NanaZip.Modern.h"

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
    using Windows::UI::Xaml::Controls::SelectionChangedEventArgs;
    using Windows::UI::Xaml::Input::KeyRoutedEventArgs;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct CompressOptionsPage : CompressOptionsPageT<CompressOptionsPage>
    {
    public:

        CompressOptionsPage(
            _In_opt_ HWND WindowHandle = nullptr,
            _In_ PK7_COMPRESS_OPTIONS_DIALOG_CONTEXT Context = nullptr);

        void InitializeComponent();

        winrt::Windows::Foundation::Size PrepareForShow();

        void OnPageKeyDown(
            winrt::IInspectable const& sender,
            winrt::KeyRoutedEventArgs const& e);

        void OnLoaded(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnNtfsClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnOkClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnCancelClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnPrecSetClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnPrecComboChanged(
            winrt::IInspectable const& sender,
            winrt::SelectionChangedEventArgs const& e);

        void OnMTimeSetClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnCTimeSetClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnATimeSetClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnZTimeSetClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

    private:

        HWND m_WindowHandle;
        PK7_COMPRESS_OPTIONS_DIALOG_CONTEXT m_Context;
        bool m_Filled = false;

        void ApplyDialogFont(UINT32 Pt);
        void ApplyFontToTree(
            winrt::Windows::UI::Xaml::DependencyObject const& Node,
            double FontSizePx);

        void FillFromContext();
        void ApplyTimeMAC();
        void UpdateBoolBox(
            winrt::Windows::UI::Xaml::Controls::CheckBox const& SetCheck,
            winrt::Windows::UI::Xaml::Controls::CheckBox const& Check,
            bool supported,
            bool isSet,
            bool val,
            bool defaultVal);
        void ReadBoolBox(
            winrt::Windows::UI::Xaml::Controls::CheckBox const& SetCheck,
            winrt::Windows::UI::Xaml::Controls::CheckBox const& Check,
            bool& isSet,
            bool& val);
        UINT32 GetPrecValue();
    };
}
