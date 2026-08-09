#pragma once

#include "InformationPage.g.h"

#include <Windows.h>

#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.h>

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct InformationPage : InformationPageT<InformationPage>
    {
        InformationPage(
            _In_opt_ HWND WindowHandle = nullptr,
            _In_opt_ LPCWSTR Title = nullptr,
            _In_opt_ LPCWSTR Content = nullptr);

        void InitializeComponent();

        void CloseButtonClickedHandler(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& args);

        // **************** SSS Modification Start ****************
        void ApplyFontSettings();
        void ApplyFontSizeToElement(
            winrt::Windows::UI::Xaml::DependencyObject const& element,
            double size);
        // **************** SSS Modification End ****************

    private:

        HWND m_WindowHandle = nullptr;
        winrt::hstring m_Title;
        winrt::hstring m_Content;
    };
}
