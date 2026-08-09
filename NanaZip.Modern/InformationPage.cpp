#include "pch.h"
#include "InformationPage.h"
#if __has_include("InformationPage.g.cpp")
#include "InformationPage.g.cpp"
#endif

namespace winrt::NanaZip::Modern::implementation
{
    InformationPage::InformationPage(
        _In_opt_ HWND WindowHandle,
        _In_opt_ LPCWSTR Title,
        _In_opt_ LPCWSTR Content) :
        m_WindowHandle(WindowHandle),
        m_Title(Title),
        m_Content(Content)
    {
    }

    void InformationPage::InitializeComponent()
    {
        InformationPageT::InitializeComponent();

        ::SetWindowTextW(this->m_WindowHandle, this->m_Title.c_str());
        
        this->TitleTextBlock().Text(this->m_Title);
        this->InformationTextBox().Text(this->m_Content);

        // **************** SSS Modification Start ****************
        this->ApplyFontSettings();
        // **************** SSS Modification End ****************
    }

    // **************** SSS Modification Start ****************
    void InformationPage::ApplyFontSettings()
    {
        // Read "FontSizeDialog" from HKCU\Software\NanaZip\Options.
        DWORD value = 0;
        {
            HKEY key = nullptr;
            if (::RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\NanaZip\\Options", 0, KEY_READ, &key) == ERROR_SUCCESS)
            {
                DWORD size = sizeof(value);
                ::RegQueryValueExW(key, L"FontSizeDialog", nullptr, nullptr, (LPBYTE)&value, &size);
                ::RegCloseKey(key);
            }
        }

        if (value == 0)
            return;

        // XAML FontSize is in DIPs; convert from points at the 96 DPI baseline.
        this->ApplyFontSizeToElement(this->Content(), (double)value * 96.0 / 72.0);
    }

    void InformationPage::ApplyFontSizeToElement(
        winrt::Windows::UI::Xaml::DependencyObject const& element,
        double size)
    {
        if (!element)
            return;

        if (auto textBlock = element.try_as<winrt::Windows::UI::Xaml::Controls::TextBlock>())
            textBlock.FontSize(size);
        else if (auto textBox = element.try_as<winrt::Windows::UI::Xaml::Controls::TextBox>())
            textBox.FontSize(size);
        else if (auto control = element.try_as<winrt::Windows::UI::Xaml::Controls::Control>())
            control.FontSize(size);

        const uint32_t count = winrt::Windows::UI::Xaml::Media::VisualTreeHelper::GetChildrenCount(element);
        for (uint32_t i = 0; i < count; i++)
        {
            this->ApplyFontSizeToElement(
                winrt::Windows::UI::Xaml::Media::VisualTreeHelper::GetChild(element, i),
                size);
        }
    }
    // **************** SSS Modification End ****************

    void InformationPage::CloseButtonClickedHandler(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

		::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }
}
