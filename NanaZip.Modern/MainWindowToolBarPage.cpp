#include "pch.h"
#include "MainWindowToolBarPage.h"
#if __has_include("MainWindowToolBarPage.g.cpp")
#include "MainWindowToolBarPage.g.cpp"
#endif

#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Media.h>

#include "NanaZip.Modern.h"

#include <Mile.Helpers.CppBase.h>
#include <Mile.Helpers.CppWinRT.h>

namespace winrt::Mile
{
    using namespace ::Mile;
}
namespace winrt
{
    using Windows::Foundation::Point;
    using Windows::UI::Xaml::Automation::AutomationProperties;
    using Windows::UI::Xaml::Controls::AppBarButton;
    using Windows::UI::Xaml::Controls::Grid;
    using Windows::UI::Xaml::Controls::ToolTipService;
    using Windows::UI::Xaml::Media::GeneralTransform;
}

namespace
{
    namespace ToolBarCommandID
    {
        enum
        {
            Add = 1070,
            Extract = 1071,
            Test = 1072,
            ExtractOneByOne = 1073,
            Copy = 546,
            Move = 547,
            Delete = 548,
            Info = 551,
            Options = 900,
            Benchmark = 901,
            About = 961
        };
    }

    namespace MenuIndex
    {
        enum
        {
            File = 0,
            Edit,
            View,
            Bookmarks
        };
    }
}

namespace winrt::NanaZip::Modern::implementation
{
    MainWindowToolBarPage::MainWindowToolBarPage(
        _In_ HWND WindowHandle,
        _In_ HMENU MoreMenu) :
        m_WindowHandle(WindowHandle),
        m_MoreMenu(MoreMenu)
    {

    }

    void MainWindowToolBarPage::InitializeComponent()
    {
        MainWindowToolBarPageT::InitializeComponent();

        winrt::AppBarButton ToolBarButtons[11] =
        {
            this->AddButton(),
            this->ExtractOneButton(),
            this->BatchExtractButton(),
            this->TestButton(),
            this->CopyButton(),
            this->MoveButton(),
            this->DeleteButton(),
            this->InfoButton(),
            this->OptionsButton(),
            this->BenchmarkButton(),
            this->AboutButton()
        };

        const UINT32 ToolBarLegacyStringResources[11] =
        {
            7200, // Add
            7201, // Extract (one by one)
            7201, // Extract (batch) - unused, button keeps its XAML label
            7202, // Test
            7203, // Copy
            7204, // Move
            7205, // Delete
            7206, // Info
            900, // Options
            901, // Benchmark
            961 // About
        };

        const std::size_t ToolBarButtonCount =
            sizeof(ToolBarButtons) / sizeof(*ToolBarButtons);

        for (size_t i = 0; i < ToolBarButtonCount; ++i)
        {
            // SSS: the batch-extract button keeps the Chinese label from
            // XAML ("批量提取") instead of the legacy language resource.
            if (i == 2)
                continue;
            winrt::hstring Resource = winrt::hstring(::K7ModernGetLegacyStringResource(
                ToolBarLegacyStringResources[i]));
            winrt::AutomationProperties::SetName(
                ToolBarButtons[i],
                Resource);
            winrt::ToolTipService::SetToolTip(
                ToolBarButtons[i],
                winrt::box_value(Resource));
        }

    }

    void MainWindowToolBarPage::PageLoaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
    }

    void MainWindowToolBarPage::AddButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Add,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::BatchExtractButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Extract,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::ExtractOneButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::ExtractOneByOne,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::TestButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Test,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::CopyButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Copy,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::MoveButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Move,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::DeleteButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Delete,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::InfoButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Info,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::OptionsButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Options,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::BenchmarkButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Benchmark,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::AboutButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::About,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::MoreButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        winrt::AppBarButton Button = sender.as<winrt::AppBarButton>();

        winrt::GeneralTransform Transform =
            Button.TransformToVisual(this->Content());
        winrt::Point LogicalPoint = Transform.TransformPoint(
            winrt::Point(0.0f, 0.0f));

        UINT DpiValue = ::GetDpiForWindow(this->m_WindowHandle);

        POINT MenuPosition = {};
        MenuPosition.x = ::MulDiv(
            static_cast<int>(LogicalPoint.X),
            DpiValue,
            USER_DEFAULT_SCREEN_DPI);
        MenuPosition.y = ::MulDiv(
            44,
            DpiValue,
            USER_DEFAULT_SCREEN_DPI);
        ::MapWindowPoints(
            this->m_WindowHandle,
            HWND_DESKTOP,
            &MenuPosition,
            1);

        ::SendMessageW(
            this->m_WindowHandle,
            WM_INITMENUPOPUP,
            reinterpret_cast<WPARAM>(::GetSubMenu(
                this->m_MoreMenu,
                MenuIndex::File)),
            MenuIndex::File);
        ::SendMessageW(
            this->m_WindowHandle,
            WM_INITMENUPOPUP,
            reinterpret_cast<WPARAM>(::GetSubMenu(
                this->m_MoreMenu,
                MenuIndex::Edit)),
            MenuIndex::Edit);
        ::SendMessageW(
            this->m_WindowHandle,
            WM_INITMENUPOPUP,
            reinterpret_cast<WPARAM>(::GetSubMenu(
                this->m_MoreMenu,
                MenuIndex::View)),
            MenuIndex::View);
        ::SendMessageW(
            this->m_WindowHandle,
            WM_INITMENUPOPUP,
            reinterpret_cast<WPARAM>(::GetSubMenu(
                this->m_MoreMenu,
                MenuIndex::Bookmarks)),
            MenuIndex::Bookmarks);

        WPARAM Command = ::TrackPopupMenuEx(
            this->m_MoreMenu,
            TPM_LEFTALIGN | TPM_NONOTIFY | TPM_RETURNCMD,
            MenuPosition.x,
            MenuPosition.y,
            this->m_WindowHandle,
            nullptr);
        if (Command)
        {
            ::PostMessageW(this->m_WindowHandle, WM_COMMAND, Command, 0);
        }
    }

}

EXTERN_C LPVOID WINAPI K7ModernCreateMainWindowToolBarPage(
    _In_ HWND ParentWindowHandle,
    _In_ HMENU MoreMenuHandle)
{
    using Interface =
        winrt::NanaZip::Modern::MainWindowToolBarPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::MainWindowToolBarPage;

    Interface Window = winrt::make<Implementation>(
        ParentWindowHandle,
        MoreMenuHandle);
    return winrt::detach_abi(Window);
}
