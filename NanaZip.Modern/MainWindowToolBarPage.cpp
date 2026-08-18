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
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>

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
    using Windows::UI::Xaml::Hosting::DesktopWindowXamlSource;
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
        // cch is the capacity of dwTypeData, including its terminator.
        Info.cch = static_cast<UINT>(Buffer.size());
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
        HMENU SystemMenu,
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
                if (SystemMenu && Info.hSubMenu == SystemMenu)
                {
                    MenuFlyoutItem SystemItem;
                    SystemItem.Text(Label);
                    SystemItem.IsEnabled((Info.fState & (MFS_DISABLED | MFS_GRAYED)) == 0);
                    SetMenuCommandTag(SystemItem, K7ModernContextMenuSystemCommand);
                    SystemItem.Click(Handler);
                    Items.Append(SystemItem);
                    continue;
                }

                MenuFlyoutSubItem SubItem;
                SubItem.Text(Label);
                SubItem.IsEnabled((Info.fState & (MFS_DISABLED | MFS_GRAYED)) == 0);
                AddMenuItems(Info.hSubMenu, SystemMenu, SubItem.Items(), Handler);
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

    static bool MenuContainsSubMenu(
        HMENU Menu,
        HMENU SubMenu)
    {
        if (!Menu || !SubMenu)
        {
            return false;
        }

        const int Count = ::GetMenuItemCount(Menu);
        for (int Position = 0; Position < Count; ++Position)
        {
            MENUITEMINFOW Info = {};
            Info.cbSize = sizeof(Info);
            Info.fMask = MIIM_SUBMENU;
            if (::GetMenuItemInfoW(Menu, Position, TRUE, &Info) &&
                Info.hSubMenu == SubMenu)
            {
                return true;
            }
        }
        return false;
    }

    static void PopulateMenuFlyout(
        MenuFlyout const& Flyout,
        HMENU Menu,
        HMENU SystemMenu,
        UINT32 FontSizePt,
        winrt::Windows::UI::Xaml::RoutedEventHandler const& Handler)
    {
        auto Items = Flyout.Items();
        Items.Clear();
        AddMenuItems(Menu, SystemMenu, Items, Handler);

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

    static void PopulateMoreMenuFlyout(
        MenuFlyout const& Flyout,
        HMENU Menu,
        HWND WindowHandle,
        UINT32 FontSizePt)
    {
        PopulateMenuFlyout(
            Flyout,
            Menu,
            nullptr,
            FontSizePt,
            CreateMenuClickHandler(WindowHandle));
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
        m_MoreMenu(MoreMenu),
        m_ContextMenuParentWindow(nullptr),
        m_ContextMenuSystemMenu(nullptr),
        m_ContextMenuPanelIndex(0),
        m_ContextMenuGeneration(0),
        m_ContextMenuScreenX(0),
        m_ContextMenuScreenY(0),
        m_ContextMenuFlyout(nullptr),
        m_ContextMenuAnchor(nullptr),
        m_ContextMenuClosedToken{}
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

    BOOL MainWindowToolBarPage::ShowContextMenu(
        HMENU Menu,
        HMENU SystemMenu,
        HWND HostWindowHandle,
        HWND ParentWindowHandle,
        INT ScreenX,
        INT ScreenY,
        UINT ContextPanelIndex,
        UINT ContextGeneration)
    {
        if (!Menu || !ParentWindowHandle)
        {
            const bool SystemMenuBelongsToMenu =
                MenuContainsSubMenu(Menu, SystemMenu);
            if (Menu)
                ::DestroyMenu(Menu);
            if (SystemMenu && !SystemMenuBelongsToMenu)
                ::DestroyMenu(SystemMenu);
            return FALSE;
        }

        if (this->m_ContextMenuFlyout)
            this->m_ContextMenuFlyout.Hide();
        if (this->m_ContextMenuAnchor)
        {
            auto Children = this->ContextMenuAnchorLayer().Children();
            std::uint32_t Index = 0;
            if (Children.IndexOf(this->m_ContextMenuAnchor, Index))
                Children.RemoveAt(Index);
            this->m_ContextMenuAnchor = nullptr;
        }
        this->m_ContextMenuFlyout = MenuFlyout();
        auto Flyout = this->m_ContextMenuFlyout;
        // The toolbar XAML island is only 48 DIP tall, while file-list right
        // clicks can be anywhere in the main window. Use a popup HWND so the
        // context menu is positioned against the desktop, not clipped to the
        // toolbar island's root bounds.
        Flyout.ShouldConstrainToRootBounds(false);
        // The target below is a 1x1 anchor at the right-click point, so this
        // placement now has its intended meaning: open downward from it.
        Flyout.Placement(
            winrt::Windows::UI::Xaml::Controls::Primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);
        auto Handler = [this, ContextGeneration](
            winrt::IInspectable const& Sender,
            winrt::RoutedEventArgs const&)
        {
            auto Item = Sender.try_as<MenuFlyoutItemBase>();
            if (!Item || !this->m_ContextMenuParentWindow ||
                ContextGeneration != this->m_ContextMenuGeneration)
                return;

            const UINT Command = GetMenuCommandTag(Item);
            if (Command != 0)
            {
                const WPARAM Value = MAKEWPARAM(
                    static_cast<WORD>(Command),
                    static_cast<WORD>(this->m_ContextMenuPanelIndex));
                ::SendMessageW(
                    this->m_ContextMenuParentWindow,
                    Command == K7ModernContextMenuSystemCommand
                        ? K7ModernContextMenuSystemMessage
                        : K7ModernContextMenuCommandMessage,
                    Value,
                    static_cast<LPARAM>(ContextGeneration));
            }
        };

        const UINT32 ContextMenuFontSize =
            ::K7ModernGetContextMenuFontSize();
        PopulateMenuFlyout(
            Flyout,
            Menu,
            SystemMenu,
            ContextMenuFontSize,
            Handler);

        bool SystemMenuDetached = false;
        if (SystemMenu)
        {
            const int Count = ::GetMenuItemCount(Menu);
            for (int Position = 0; Position < Count; ++Position)
            {
                MENUITEMINFOW Info = {};
                Info.cbSize = sizeof(Info);
                Info.fMask = MIIM_SUBMENU;
                if (::GetMenuItemInfoW(Menu, Position, TRUE, &Info) &&
                    Info.hSubMenu == SystemMenu)
                {
                    ::RemoveMenu(Menu, Position, MF_BYPOSITION);
                    SystemMenuDetached = true;
                    break;
                }
            }
        }
        ::DestroyMenu(Menu);
        if (SystemMenu && !SystemMenuDetached)
        {
            ::DestroyMenu(SystemMenu);
            SystemMenu = nullptr;
        }

        this->m_ContextMenuParentWindow = ParentWindowHandle;
        this->m_ContextMenuSystemMenu = SystemMenu;
        this->m_ContextMenuPanelIndex = ContextPanelIndex;
        this->m_ContextMenuGeneration = ContextGeneration;
        this->m_ContextMenuScreenX = ScreenX;
        this->m_ContextMenuScreenY = ScreenY;
        this->m_ContextMenuClosedToken = Flyout.Closed(
            [this, ContextGeneration](auto const&, auto const&)
            {
                const UINT PanelIndex = this->m_ContextMenuPanelIndex;
                if (ContextGeneration != this->m_ContextMenuGeneration)
                    return;
                if (this->m_ContextMenuAnchor)
                {
                    auto Children = this->ContextMenuAnchorLayer().Children();
                    std::uint32_t Index = 0;
                    if (Children.IndexOf(this->m_ContextMenuAnchor, Index))
                        Children.RemoveAt(Index);
                    this->m_ContextMenuAnchor = nullptr;
                }
                this->m_ContextMenuParentWindow = nullptr;
                this->m_ContextMenuSystemMenu = nullptr;
                this->m_ContextMenuPanelIndex = 0;
                ::PostMessageW(
                    this->m_WindowHandle,
                    K7ModernContextMenuClosedMessage,
                    PanelIndex,
                    static_cast<LPARAM>(ContextGeneration));
            });

        // ShowAt(nullptr, ...) is known to hang/crash in this XAML-island
        // host. Keep both the anchor and the flyout in the existing toolbar
        // XAML root so that its presenter, input routing, and light-dismiss
        // behavior remain in the same island.
        POINT Point = { ScreenX, ScreenY };
        if (!::ScreenToClient(HostWindowHandle, &Point))
        {
            if (SystemMenu && SystemMenuDetached)
                ::DestroyMenu(SystemMenu);
            this->m_ContextMenuParentWindow = nullptr;
            this->m_ContextMenuSystemMenu = nullptr;
            this->m_ContextMenuPanelIndex = 0;
            this->m_ContextMenuGeneration = 0;
            return FALSE;
        }

        const UINT Dpi = ::GetDpiForWindow(HostWindowHandle);
        const double Scale = Dpi == 0
            ? 1.0
            : static_cast<double>(Dpi) /
                static_cast<double>(USER_DEFAULT_SCREEN_DPI);
        winrt::Windows::UI::Xaml::Controls::Grid Anchor;
        Anchor.Width(1.0);
        Anchor.Height(1.0);
        winrt::Windows::UI::Xaml::Controls::Canvas::SetLeft(
            Anchor,
            static_cast<double>(Point.x) / Scale);
        winrt::Windows::UI::Xaml::Controls::Canvas::SetTop(
            Anchor,
            static_cast<double>(Point.y) / Scale);
        this->ContextMenuAnchorLayer().Children().Append(Anchor);
        this->m_ContextMenuAnchor = Anchor;
        try
        {
            // Use FlyoutBase's target-based overload. MenuFlyout's point
            // overload chooses context-menu placement automatically, which
            // can override the requested downward placement.
            Flyout.ShowAt(Anchor);
        }
        catch (...)
        {
            auto Children = this->ContextMenuAnchorLayer().Children();
            std::uint32_t Index = 0;
            if (Children.IndexOf(Anchor, Index))
                Children.RemoveAt(Index);
            this->m_ContextMenuAnchor = nullptr;
            if (SystemMenu && SystemMenuDetached)
                ::DestroyMenu(SystemMenu);
            this->m_ContextMenuParentWindow = nullptr;
            this->m_ContextMenuSystemMenu = nullptr;
            this->m_ContextMenuPanelIndex = 0;
            this->m_ContextMenuGeneration = 0;
            return FALSE;
        }
        return TRUE;
    }

    BOOL MainWindowToolBarPage::ShowColumnsContextMenu(
        HMENU Menu,
        HWND HostWindowHandle,
        HWND ParentWindowHandle,
        INT ScreenX,
        INT ScreenY,
        UINT ContextPanelIndex,
        UINT ContextGeneration)
    {
        if (!Menu || !ParentWindowHandle)
        {
            if (Menu)
                ::DestroyMenu(Menu);
            return FALSE;
        }

        try
        {
        if (this->m_ColumnsContextMenuFlyout)
            this->m_ColumnsContextMenuFlyout.Hide();
        if (this->m_ColumnsContextMenuAnchor)
        {
            auto Children = this->ContextMenuAnchorLayer().Children();
            std::uint32_t Index = 0;
            if (Children.IndexOf(this->m_ColumnsContextMenuAnchor, Index))
                Children.RemoveAt(Index);
            this->m_ColumnsContextMenuAnchor = nullptr;
        }

        this->m_ColumnsContextMenuFlyout = MenuFlyout();
        auto Flyout = this->m_ColumnsContextMenuFlyout;
        Flyout.ShouldConstrainToRootBounds(false);
        Flyout.Placement(
            winrt::Windows::UI::Xaml::Controls::Primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);

        auto Handler = [ParentWindowHandle, ContextPanelIndex, ContextGeneration](
            winrt::IInspectable const& Sender,
            winrt::RoutedEventArgs const&)
        {
            auto Item = Sender.try_as<MenuFlyoutItemBase>();
            if (!Item)
                return;

            const UINT Command = GetMenuCommandTag(Item);
            if (Command != 0)
            {
                ::SendMessageW(
                    ParentWindowHandle,
                    K7ModernColumnsContextMenuCommandMessage,
                    MAKEWPARAM(
                        static_cast<WORD>(Command),
                        static_cast<WORD>(ContextPanelIndex)),
                    static_cast<LPARAM>(ContextGeneration));
            }
        };

        PopulateMenuFlyout(
            Flyout,
            Menu,
            nullptr,
            ::K7ModernGetContextMenuFontSize(),
            Handler);
        ::DestroyMenu(Menu);
        Menu = nullptr;

        POINT Point = { ScreenX, ScreenY };
        if (!::ScreenToClient(HostWindowHandle, &Point))
        {
            return FALSE;
        }

        const UINT Dpi = ::GetDpiForWindow(HostWindowHandle);
        const double Scale = Dpi == 0
            ? 1.0
            : static_cast<double>(Dpi) /
                static_cast<double>(USER_DEFAULT_SCREEN_DPI);
        winrt::Windows::UI::Xaml::Controls::Grid Anchor;
        Anchor.Width(1.0);
        Anchor.Height(1.0);
        winrt::Windows::UI::Xaml::Controls::Canvas::SetLeft(
            Anchor,
            static_cast<double>(Point.x) / Scale);
        winrt::Windows::UI::Xaml::Controls::Canvas::SetTop(
            Anchor,
            static_cast<double>(Point.y) / Scale);
        this->ContextMenuAnchorLayer().Children().Append(Anchor);
        this->m_ColumnsContextMenuAnchor = Anchor;
        this->m_ColumnsContextMenuClosedToken = Flyout.Closed(
            [this, Anchor, ParentWindowHandle, ContextPanelIndex, ContextGeneration](
                auto const&, auto const&)
            {
                auto Children = this->ContextMenuAnchorLayer().Children();
                std::uint32_t Index = 0;
                if (Children.IndexOf(Anchor, Index))
                    Children.RemoveAt(Index);
                if (this->m_ColumnsContextMenuAnchor == Anchor)
                    this->m_ColumnsContextMenuAnchor = nullptr;
                ::PostMessageW(
                    ParentWindowHandle,
                    K7ModernColumnsContextMenuClosedMessage,
                    ContextPanelIndex,
                    static_cast<LPARAM>(ContextGeneration));
            });

        try
        {
            Flyout.ShowAt(Anchor);
        }
        catch (...)
        {
            auto Children = this->ContextMenuAnchorLayer().Children();
            std::uint32_t Index = 0;
            if (Children.IndexOf(Anchor, Index))
                Children.RemoveAt(Index);
            this->m_ColumnsContextMenuAnchor = nullptr;
            return FALSE;
        }
        return TRUE;
        }
        catch (...)
        {
            if (Menu)
                ::DestroyMenu(Menu);
            return FALSE;
        }
    }

}

EXTERN_C BOOL WINAPI K7ModernShowContextMenu(
    _In_ HWND ToolBarWindowHandle,
    _In_ HWND ParentWindowHandle,
    _In_ HMENU MenuHandle,
    _In_opt_ HMENU SystemMenuHandle,
    _In_ HWND HostWindowHandle,
    _In_ INT ScreenX,
    _In_ INT ScreenY,
    _In_ UINT ContextPanelIndex,
    _In_ UINT ContextGeneration)
{
    if (!ToolBarWindowHandle || !MenuHandle)
    {
        if (MenuHandle)
            ::DestroyMenu(MenuHandle);
        if (SystemMenuHandle)
            ::DestroyMenu(SystemMenuHandle);
        return FALSE;
    }

    winrt::DesktopWindowXamlSource XamlSource = nullptr;
    winrt::copy_from_abi(
        XamlSource,
        ::GetPropW(ToolBarWindowHandle, L"XamlWindowSource"));
    if (!XamlSource)
    {
        ::DestroyMenu(MenuHandle);
        if (SystemMenuHandle)
            ::DestroyMenu(SystemMenuHandle);
        return FALSE;
    }

    auto Page = XamlSource.Content().try_as<
        winrt::NanaZip::Modern::MainWindowToolBarPage>();
    if (!Page)
    {
        ::DestroyMenu(MenuHandle);
        if (SystemMenuHandle)
            ::DestroyMenu(SystemMenuHandle);
        return FALSE;
    }

    return winrt::get_self<
        winrt::NanaZip::Modern::implementation::MainWindowToolBarPage>(Page)
        ->ShowContextMenu(
            MenuHandle,
            SystemMenuHandle,
            HostWindowHandle,
            ParentWindowHandle,
            ScreenX,
            ScreenY,
            ContextPanelIndex,
            ContextGeneration);
}

EXTERN_C BOOL WINAPI K7ModernShowColumnsContextMenu(
    _In_ HWND ToolBarWindowHandle,
    _In_ HWND ParentWindowHandle,
    _In_ HMENU MenuHandle,
    _In_ HWND HostWindowHandle,
    _In_ INT ScreenX,
    _In_ INT ScreenY,
    _In_ UINT ContextPanelIndex,
    _In_ UINT ContextGeneration)
{
    if (!ToolBarWindowHandle || !MenuHandle)
    {
        if (MenuHandle)
            ::DestroyMenu(MenuHandle);
        return FALSE;
    }

    try
    {
        winrt::DesktopWindowXamlSource XamlSource = nullptr;
        winrt::copy_from_abi(
            XamlSource,
            ::GetPropW(ToolBarWindowHandle, L"XamlWindowSource"));
        if (!XamlSource)
        {
            ::DestroyMenu(MenuHandle);
            return FALSE;
        }

        auto Page = XamlSource.Content().try_as<
            winrt::NanaZip::Modern::MainWindowToolBarPage>();
        if (!Page)
        {
            ::DestroyMenu(MenuHandle);
            return FALSE;
        }

        return winrt::get_self<
            winrt::NanaZip::Modern::implementation::MainWindowToolBarPage>(Page)
            ->ShowColumnsContextMenu(
                MenuHandle,
                HostWindowHandle,
                ParentWindowHandle,
                ScreenX,
                ScreenY,
                ContextPanelIndex,
                ContextGeneration);
    }
    catch (...)
    {
        ::DestroyMenu(MenuHandle);
        return FALSE;
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
