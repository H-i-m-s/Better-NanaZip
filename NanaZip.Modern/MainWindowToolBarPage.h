#pragma once

#include "MainWindowToolBarPage.g.h"

#include <Windows.h>
#include <winrt/Windows.UI.Xaml.Controls.h>



namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
}
namespace winrt::NanaZip::Modern::implementation
{
    struct MainWindowToolBarPage : MainWindowToolBarPageT<MainWindowToolBarPage>
    {
    public:

        MainWindowToolBarPage(
            _In_ HWND WindowHandle = nullptr,
            _In_ HMENU MoreMenu = nullptr);

        void InitializeComponent();

        void PageLoaded(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void AddButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void BatchExtractButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void ExtractOneButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void TestButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void CopyButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void MoveButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void DeleteButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void InfoButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OptionsButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void BenchmarkButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void AboutButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void MoreButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        BOOL ShowContextMenu(
            HMENU Menu,
            HMENU SystemMenu,
            HWND HostWindowHandle,
            HWND ParentWindowHandle,
            INT ScreenX,
            INT ScreenY,
            UINT ContextPanelIndex,
            UINT ContextGeneration);

    private:

        HWND m_WindowHandle;
        HMENU m_MoreMenu;
        HWND m_ContextMenuParentWindow;
        HMENU m_ContextMenuSystemMenu;
        UINT m_ContextMenuPanelIndex;
        UINT m_ContextMenuGeneration;
        INT m_ContextMenuScreenX;
        INT m_ContextMenuScreenY;
        winrt::Windows::UI::Xaml::Controls::MenuFlyout m_ContextMenuFlyout;
        winrt::event_token m_ContextMenuClosedToken;
    };
}

namespace winrt::NanaZip::Modern::factory_implementation
{
    struct MainWindowToolBarPage : MainWindowToolBarPageT<
        MainWindowToolBarPage, implementation::MainWindowToolBarPage>
    {
    };
}
