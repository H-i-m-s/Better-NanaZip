#include "pch.h"
#include "MainWindowToolBarPage.h"
#if __has_include("MainWindowToolBarPage.g.cpp")
#include "MainWindowToolBarPage.g.cpp"
#endif

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include "NanaZip.Modern.h"

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.Controls.h>

#include <string>
#include <utility>
#include <vector>

#include <Mile.Helpers.CppBase.h>
#include <Mile.Helpers.CppWinRT.h>

namespace winrt::Mile
{
    using namespace ::Mile;
}
namespace winrt
{
    using Windows::UI::Xaml::Automation::AutomationProperties;
    using Windows::UI::Xaml::Controls::AppBarButton;
    using Windows::UI::Xaml::Controls::MenuFlyout;
    using Windows::UI::Xaml::Controls::MenuFlyoutItem;
    using Windows::UI::Xaml::Controls::MenuFlyoutSeparator;
    using Windows::UI::Xaml::Controls::MenuFlyoutSubItem;
    using Windows::UI::Xaml::Controls::ToggleMenuFlyoutItem;
    using Windows::UI::Xaml::Controls::ToolTipService;
}

namespace
{
    using MenuFlyout =
        winrt::Windows::UI::Xaml::Controls::MenuFlyout;
    using MenuFlyoutItem =
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItem;
    using MenuFlyoutSeparator =
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutSeparator;
    using MenuFlyoutSubItem =
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutSubItem;
    using ToggleMenuFlyoutItem =
        winrt::Windows::UI::Xaml::Controls::ToggleMenuFlyoutItem;
    using MenuFlyoutItemBase =
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItemBase;
    using MenuFlyoutItemBaseVector =
        winrt::Windows::Foundation::Collections::IVector<MenuFlyoutItemBase>;

    static winrt::hstring GetMenuItemText(HMENU Menu, UINT Position)
    {
        MENUITEMINFOW Info = {};
        Info.cbSize = sizeof(Info);
        Info.fMask = MIIM_STRING;
        if (!::GetMenuItemInfoW(Menu, Position, TRUE, &Info))
        {
            return {};
        }

        std::vector<wchar_t> Buffer(Info.cch + 1, L'\0');
        Info.dwTypeData = Buffer.data();
        Info.cch = static_cast<UINT>(Buffer.size() - 1);
        if (!::GetMenuItemInfoW(Menu, Position, TRUE, &Info))
        {
            return {};
        }
        return winrt::hstring(Buffer.data());
    }

    static winrt::hstring StripMenuPrefix(winrt::hstring Text)
    {
        std::wstring Value(Text.c_str());
        std::wstring Display;
        Display.reserve(Value.size());
        for (std::size_t Index = 0; Index < Value.size(); ++Index)
        {
            if (Value[Index] == L'&' && Index + 1 < Value.size() &&
                Value[Index + 1] == L'&')
            {
                Display.push_back(L'&');
                ++Index;
            }
            else if (Value[Index] != L'&')
            {
                Display.push_back(Value[Index]);
            }
        }
        Value = std::move(Display);
        const std::size_t Tab = Value.find(L'\t');
        if (Tab != std::wstring::npos)
        {
            Value.resize(Tab);
        }
        return winrt::hstring(Value);
    }

    static winrt::hstring GetMenuShortcut(winrt::hstring Text)
    {
        const std::wstring Value(Text.c_str());
        const std::size_t Tab = Value.find(L'\t');
        return Tab == std::wstring::npos
            ? winrt::hstring()
            : winrt::hstring(Value.substr(Tab + 1));
    }

    static void SetMenuCommandTag(
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItemBase const& Item,
        UINT Command)
    {
        if (auto Element = Item.try_as<winrt::Windows::UI::Xaml::FrameworkElement>())
        {
            Element.Tag(winrt::box_value(Command));
        }
    }

    static UINT GetMenuCommandTag(
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItemBase const& Item)
    {
        if (auto Element = Item.try_as<winrt::Windows::UI::Xaml::FrameworkElement>())
        {
            if (auto Value = Element.Tag().try_as<winrt::Windows::Foundation::IPropertyValue>())
            {
                return Value.GetUInt32();
            }
        }
        return 0;
    }

    static void AddMenuItems(
        HMENU Menu,
        MenuFlyoutItemBaseVector const& Items,
        winrt::Windows::UI::Xaml::RoutedEventHandler const& Handler)
    {
        const int Count = ::GetMenuItemCount(Menu);
        for (int Position = 0; Position < Count; ++Position)
        {
            MENUITEMINFOW Info = {};
            Info.cbSize = sizeof(Info);
            Info.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_SUBMENU;
            if (!::GetMenuItemInfoW(Menu, Position, TRUE, &Info))
            {
                continue;
            }

            if ((Info.fType & MFT_SEPARATOR) != 0)
            {
                Items.Append(MenuFlyoutSeparator());
                continue;
            }

            const winrt::hstring RawText = GetMenuItemText(Menu, Position);
            const winrt::hstring Label = StripMenuPrefix(RawText);
            if (Info.hSubMenu)
            {
                MenuFlyoutSubItem SubItem;
                SubItem.Text(Label);
                SubItem.IsEnabled((Info.fState & (MFS_DISABLED | MFS_GRAYED)) == 0);
                AddMenuItems(Info.hSubMenu, SubItem.Items(), Handler);
                Items.Append(SubItem);
                continue;
            }

            if ((Info.fState & MFS_CHECKED) != 0)
            {
                ToggleMenuFlyoutItem Item;
                Item.Text(Label);
                Item.IsChecked(true);
                Item.KeyboardAcceleratorTextOverride(GetMenuShortcut(RawText));
                SetMenuCommandTag(Item, Info.wID);
                Item.IsEnabled((Info.fState & (MFS_DISABLED | MFS_GRAYED)) == 0);
                Item.Click(Handler);
                Items.Append(Item);
            }
            else
            {
                MenuFlyoutItem Item;
                Item.Text(Label);
                Item.KeyboardAcceleratorTextOverride(GetMenuShortcut(RawText));
                SetMenuCommandTag(Item, Info.wID);
                Item.IsEnabled((Info.fState & (MFS_DISABLED | MFS_GRAYED)) == 0);
                Item.Click(Handler);
                Items.Append(Item);
            }
        }
    }

    static winrt::Windows::UI::Xaml::RoutedEventHandler CreateMenuClickHandler(
        HWND WindowHandle)
    {
        return [WindowHandle](
            winrt::IInspectable const& Sender,
            winrt::RoutedEventArgs const&)
        {
            auto Item = Sender.try_as<MenuFlyoutItemBase>();
            if (!Item)
            {
                return;
            }

            const UINT Command = GetMenuCommandTag(Item);
            if (Command != 0)
            {
                ::PostMessageW(WindowHandle, WM_COMMAND, Command, 0);
            }
        };
    }

    static void ApplyMenuFontSize(
        MenuFlyoutItemBase const& Item,
        double FontSize)
    {
        if (auto Control = Item.try_as<winrt::Windows::UI::Xaml::Controls::Control>())
        {
            Control.FontSize(FontSize);
        }

        if (auto SubItem = Item.try_as<MenuFlyoutSubItem>())
        {
            for (auto const& Child : SubItem.Items())
            {
                ApplyMenuFontSize(Child, FontSize);
            }
        }
    }

    static void PopulateMoreMenuFlyout(
        MenuFlyout const& Flyout,
        HMENU Menu,
        HWND WindowHandle,
        UINT32 FontSizePt)
    {
        auto Items = Flyout.Items();
        Items.Clear();
        auto Handler = CreateMenuClickHandler(WindowHandle);
        AddMenuItems(Menu, Items, Handler);

        if (FontSizePt != 0 && FontSizePt <= 36)
        {
            const double FontSize =
                static_cast<double>(FontSizePt) * 96.0 / 72.0;
            for (auto const& Item : Items)
            {
                ApplyMenuFontSize(Item, FontSize);
            }
        }
    }

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

        for (int Position = MenuIndex::File;
             Position <= MenuIndex::Bookmarks;
             ++Position)
        {
            ::SendMessageW(
                this->m_WindowHandle,
                WM_INITMENUPOPUP,
                reinterpret_cast<WPARAM>(::GetSubMenu(
                    this->m_MoreMenu,
                    Position)),
                Position);
        }

        PopulateMoreMenuFlyout(
            this->MoreMenuFlyout(),
            this->m_MoreMenu,
            this->m_WindowHandle,
            ::K7ModernGetMoreMenuFontSize());
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
