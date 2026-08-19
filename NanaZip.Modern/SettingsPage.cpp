#include "pch.h"
#include "SettingsPage.h"
#if __has_include("SettingsPage.g.cpp")
#include "SettingsPage.g.cpp"
#endif

#include "NanaZip.Modern.h"

#include <commdlg.h>
#include <shlobj.h>

#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.h>

// Temporary diagnostics for the settings-dialog Esc/input routing. Appends
// to %TEMP%\sss_settings_diag.log; remove once the Esc issue is located.
static void SettingsPageDiagLog(const wchar_t* msg)
{
    wchar_t path[MAX_PATH];
    const DWORD n = ::GetTempPathW(MAX_PATH, path);
    if (n == 0 || n >= MAX_PATH)
    {
        return;
    }
    wcscat_s(path, L"sss_settings_diag.log");
    HANDLE h = ::CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        ::WriteFile(h, msg, (DWORD)(wcslen(msg) * sizeof(wchar_t)),
            &written, nullptr);
        ::WriteFile(h, L"\r\n", 4, &written, nullptr);
        ::CloseHandle(h);
    }
}

// Recursively collects all visual children of a given type.
template <typename T>
static void FindVisualChildren(
    winrt::Windows::UI::Xaml::DependencyObject const& Node,
    std::vector<T>& Out)
{
    const int Count =
        winrt::Windows::UI::Xaml::Media::VisualTreeHelper::
            GetChildrenCount(Node);
    for (int i = 0; i < Count; i++)
    {
        auto Child =
            winrt::Windows::UI::Xaml::Media::VisualTreeHelper::
                GetChild(Node, i);
        if (auto Tried = Child.try_as<T>())
        {
            Out.push_back(Tried);
        }
        FindVisualChildren(Child, Out);
    }
}

// Finds a template element by its x:Name.
static winrt::Windows::UI::Xaml::FrameworkElement FindNamedElement(
    winrt::Windows::UI::Xaml::DependencyObject const& Node,
    winrt::hstring const& Name)
{
    const int Count =
        winrt::Windows::UI::Xaml::Media::VisualTreeHelper::
            GetChildrenCount(Node);
    for (int i = 0; i < Count; i++)
    {
        auto Child = winrt::Windows::UI::Xaml::Media::VisualTreeHelper::
            GetChild(Node, i);
        if (auto Element =
            Child.try_as<winrt::Windows::UI::Xaml::FrameworkElement>())
        {
            if (Element.Name() == Name)
            {
                return Element;
            }
        }
        if (auto Found = FindNamedElement(Child, Name))
        {
            return Found;
        }
    }
    return nullptr;
}

// Finds the first Grid whose column definition count matches (the
// ComboBox template root has the text column plus the fixed arrow
// column).
static winrt::Windows::UI::Xaml::Controls::Grid FindTemplateGrid(
    winrt::Windows::UI::Xaml::DependencyObject const& Node,
    uint32_t ColumnCount)
{
    if (auto Grid = Node.try_as<winrt::Windows::UI::Xaml::Controls::Grid>())
    {
        if (Grid.ColumnDefinitions().Size() == ColumnCount)
        {
            return Grid;
        }
    }
    const int Count =
        winrt::Windows::UI::Xaml::Media::VisualTreeHelper::
            GetChildrenCount(Node);
    for (int i = 0; i < Count; i++)
    {
        if (auto Found = FindTemplateGrid(
                winrt::Windows::UI::Xaml::Media::VisualTreeHelper::
                    GetChild(Node, i),
                ColumnCount))
        {
            return Found;
        }
    }
    return nullptr;
}

// Finds the ComboBox drop arrow by its template name ("DropDownGlyph").
// In the SunValley theme the arrow is a FontIcon/TextBlock style element
// whose font size follows the dialog font, so its width grows with the
// font size.
static winrt::Windows::UI::Xaml::FrameworkElement FindDropDownGlyph(
    winrt::Windows::UI::Xaml::DependencyObject const& Node)
{
    return FindNamedElement(Node, L"DropDownGlyph");
}

// Applies the arrow layout three-step to one combo and returns the
// MinWidth it needs: text width + the enlarged arrow column width.
// The arrow's DesiredSize already includes its 14px right margin, so
// the new column width is simply the measured arrow width; sizing the
// arrow element itself is NOT needed (a FontIcon wraps its glyph and
// would center the glyph inside a widened box, which would detach the
// arrow from the combo's right edge).
static double ApplyComboArrowLayout(
    winrt::Windows::UI::Xaml::Controls::ComboBox const& Combo)
{
    winrt::Windows::Foundation::Size Inf(100000.0f, 100000.0f);
    Combo.Measure(Inf);

    double ArrowW = 24.0;
    if (auto Glyph = FindDropDownGlyph(Combo))
    {
        Glyph.Measure(Inf);
        ArrowW = Glyph.DesiredSize().Width;
    }
    if (ArrowW <= 0.0)
    {
        ArrowW = 24.0;
    }
    const double NewColW = ArrowW;  // includes the arrow's right margin
    if (auto Grid = FindTemplateGrid(Combo, 2))
    {
        Grid.ColumnDefinitions().GetAt(1).Width(
            winrt::Windows::UI::Xaml::GridLength(NewColW));
    }

    double TextW = 0.0;
    if (auto Presenter =
        FindNamedElement(Combo, L"ContentPresenter"))
    {
        Presenter.Measure(Inf);
        TextW = Presenter.DesiredSize().Width;
    }
    if (TextW <= 0.0)
    {
        // Fallback: combo width minus the template's original arrow
        // column width.
        TextW = Combo.DesiredSize().Width - 38.0;
    }

    const double Needed = TextW + NewColW;
    Combo.MinWidth(Needed);

    // Temporary diagnostics for the arrow-layout width tuning.
    {
        wchar_t buf[256];
        swprintf_s(buf,
            L"[Align] %s ArrowW=%.1f ColW=%.1f TextW=%.1f MinW=%.1f",
            Combo.Name().c_str(), ArrowW, NewColW, TextW, Needed);
        SettingsPageDiagLog(buf);
    }
    return Needed;
}

namespace winrt::NanaZip::Modern::implementation
{
    SettingsPage::SettingsPage(
        _In_opt_ HWND WindowHandle,
        _In_ PK7_SETTINGS_DIALOG_CONTEXT Context,
        bool HasSavedWindowRect) :
        m_WindowHandle(WindowHandle),
        m_Context(Context),
        m_HasSavedWindowRect(HasSavedWindowRect),
        m_InitGuard(false),
        m_ScrollBarW(0.0),
        m_ScrollBarMeasured(false)
    {
        // The Win32 host owns the final window-rectangle snapshot. The page
        // applies the dialog font once its visual tree exists, then measures
        // the live scrollbar exactly once to finish the minimum-width fit.
        this->Loaded({ this, &SettingsPage::OnLoaded });
        this->SizeChanged({ this, &SettingsPage::OnSizeChanged });
    }

    winrt::hstring SettingsPage::Res(
        UINT32 ResourceId,
        LPCWSTR Fallback)
    {
        LPCWSTR Content = ::K7ModernGetLegacyStringResource(ResourceId);
        if (Content && Content[0] != L'\0')
        {
            return winrt::hstring(Content);
        }
        return winrt::hstring(Fallback ? Fallback : L"");
    }

    static winrt::Windows::Foundation::IReference<bool> BoxBool(bool value)
    {
        return winrt::box_value(value).as<
            winrt::Windows::Foundation::IReference<bool>>();
    }

    static bool GetBool(
        winrt::Windows::Foundation::IReference<bool> const& ref)
    {
        if (ref)
        {
            return ref.Value();
        }
        return false;
    }

    void SettingsPage::ApplyDialogFont(UINT32 Pt)
    {
        // UWP does not inherit FontSize down the visual tree from a Control,
        // so setting Page.FontSize alone would leave every child unchanged.
        // Walk the tree and set FontSize explicitly on every control.
        double FontSizePx = (Pt == 0) ? 0.0 : (double)Pt * 96.0 / 72.0;
        ApplyFontToTree(*this, FontSizePx);
    }

    void SettingsPage::ApplyFontToTree(
        winrt::Windows::UI::Xaml::DependencyObject const& Node,
        double FontSizePx)
    {
        if (FontSizePx > 0.0)
        {
            if (auto Control =
                Node.try_as<winrt::Windows::UI::Xaml::Controls::Control>())
            {
                Control.FontSize(FontSizePx);
            }
            else if (auto TextBlock =
                Node.try_as<winrt::Windows::UI::Xaml::Controls::TextBlock>())
            {
                TextBlock.FontSize(FontSizePx);
            }
        }
        else
        {
            if (auto Control =
                Node.try_as<winrt::Windows::UI::Xaml::Controls::Control>())
            {
                Control.ClearValue(
                    winrt::Windows::UI::Xaml::Controls::Control::FontSizeProperty());
            }
            else if (auto TextBlock =
                Node.try_as<winrt::Windows::UI::Xaml::Controls::TextBlock>())
            {
                TextBlock.ClearValue(
                    winrt::Windows::UI::Xaml::Controls::TextBlock::FontSizeProperty());
            }
        }

        int ChildCount =
            winrt::Windows::UI::Xaml::Media::VisualTreeHelper::GetChildrenCount(Node);
        for (int i = 0; i < ChildCount; i++)
        {
            ApplyFontToTree(
                winrt::Windows::UI::Xaml::Media::VisualTreeHelper::GetChild(Node, i),
                FontSizePx);
        }
    }

    void SettingsPage::InitFontCombo(
        winrt::Windows::UI::Xaml::Controls::ComboBox const& Combo,
        UINT32 Pt)
    {
        static const UINT32 kFontValues[] = { 0, 8, 9, 10, 11, 12, 14, 16, 18, 20, 24 };

        Combo.Items().Clear();

        winrt::hstring DefaultLabel = Res(2525, L"Default");
        Combo.Items().Append(winrt::box_value(DefaultLabel));
        for (unsigned i = 1; i < ARRAYSIZE(kFontValues); i++)
        {
            Combo.Items().Append(winrt::box_value(
                winrt::to_hstring(kFontValues[i])));
        }

        int SelectIndex = 0;
        for (unsigned i = 0; i < ARRAYSIZE(kFontValues); i++)
        {
            if (kFontValues[i] == Pt)
            {
                SelectIndex = (int)i;
                break;
            }
        }
        Combo.SelectedIndex(SelectIndex);
    }

    void SettingsPage::SwitchTab(int Index)
    {
        if (this->m_Context)
        {
            this->m_Context->LastTab = (UINT32)Index;
        }

        bool b0 = (Index == 0);
        bool b1 = (Index == 1);
        bool b2 = (Index == 2);
        bool b3 = (Index == 3);
        bool b4 = (Index == 4);

        TabMenuButton().IsChecked(BoxBool(b0));
        TabFoldersButton().IsChecked(BoxBool(b1));
        TabEditorButton().IsChecked(BoxBool(b2));
        TabSettingsButton().IsChecked(BoxBool(b3));
        TabExtractButton().IsChecked(BoxBool(b4));

        ContentMenu().Visibility(b0 ?
            winrt::Windows::UI::Xaml::Visibility::Visible :
            winrt::Windows::UI::Xaml::Visibility::Collapsed);
        ContentFolders().Visibility(b1 ?
            winrt::Windows::UI::Xaml::Visibility::Visible :
            winrt::Windows::UI::Xaml::Visibility::Collapsed);
        ContentEditor().Visibility(b2 ?
            winrt::Windows::UI::Xaml::Visibility::Visible :
            winrt::Windows::UI::Xaml::Visibility::Collapsed);
        ContentSettings().Visibility(b3 ?
            winrt::Windows::UI::Xaml::Visibility::Visible :
            winrt::Windows::UI::Xaml::Visibility::Collapsed);
        ContentExtract().Visibility(b4 ?
            winrt::Windows::UI::Xaml::Visibility::Visible :
            winrt::Windows::UI::Xaml::Visibility::Collapsed);
    }

    void SettingsPage::InitializeComponent()
    {
        SettingsPageT::InitializeComponent();

        m_InitGuard = true;

        // Window title.
        winrt::hstring WindowTitle = Res(2100, L"Options");
        ::SetWindowTextW(this->m_WindowHandle, WindowTitle.c_str());

        // Tab titles.
        TabMenuButton().Content(winrt::box_value(Res(12200, L"Integration")));
        TabFoldersButton().Content(winrt::box_value(Res(2400, L"Folders")));
        TabEditorButton().Content(winrt::box_value(Res(2103, L"Editor")));
        TabSettingsButton().Content(winrt::box_value(Res(2500, L"Settings")));
        TabExtractButton().Content(winrt::box_value(Res(12900, L"Extract")));

        // Bottom buttons. 401 = OK, 402 = Cancel, 403 = Apply (legacy IDs).
        OkButton().Content(winrt::box_value(Res(401, L"OK")));
        CancelButton().Content(winrt::box_value(Res(402, L"Cancel")));
        ApplyButton().Content(winrt::box_value(Res(403, L"Apply")));
        if (!this->m_Context->ApplyCallback)
        {
            ApplyButton().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Collapsed);
        }

        // ============ Settings page ============
        SettingsShowDotsCheck().Content(winrt::box_value(Res(2501, L"Show \"..\" item")));
        SettingsShowDotsCheck().IsChecked(BoxBool(this->m_Context->ShowDots != FALSE));
        SettingsShowRealFileIconsCheck().Content(winrt::box_value(Res(2502, L"Show real file icons")));
        SettingsShowRealFileIconsCheck().IsChecked(BoxBool(this->m_Context->ShowRealFileIcons != FALSE));
        SettingsFullRowCheck().Content(winrt::box_value(Res(2504, L"Full row select")));
        SettingsFullRowCheck().IsChecked(BoxBool(this->m_Context->FullRow != FALSE));
        SettingsShowGridCheck().Content(winrt::box_value(Res(2505, L"Show grid lines")));
        SettingsShowGridCheck().IsChecked(BoxBool(this->m_Context->ShowGrid != FALSE));
        SettingsSingleClickCheck().Content(winrt::box_value(Res(2506, L"Single-click to open an item")));
        SettingsSingleClickCheck().IsChecked(BoxBool(this->m_Context->SingleClick != FALSE));
        SettingsAlternativeSelectionCheck().Content(winrt::box_value(Res(2507, L"Alternative selection mode")));
        SettingsAlternativeSelectionCheck().IsChecked(BoxBool(this->m_Context->AlternativeSelection != FALSE));
        SettingsShowSystemMenuCheck().Content(winrt::box_value(Res(2503, L"Show system menu")));
        SettingsShowSystemMenuCheck().IsChecked(BoxBool(this->m_Context->ShowSystemMenu != FALSE));
        SettingsLargePagesCheck().Content(winrt::box_value(Res(2508, L"Use large memory pages")));
        SettingsLargePagesCheck().IsChecked(BoxBool(this->m_Context->LargePages != FALSE));
        SettingsLargePagesCheck().IsEnabled(this->m_Context->LargePagesSupported != FALSE);
        SettingsArcHistoryCheck().Content(winrt::box_value(Res(2509, L"Want ArcHistory")));
        SettingsArcHistoryCheck().IsChecked(BoxBool(this->m_Context->ArcHistory != FALSE));
        SettingsPathHistoryCheck().Content(winrt::box_value(Res(2510, L"Want PathHistory")));
        SettingsPathHistoryCheck().IsChecked(BoxBool(this->m_Context->PathHistory != FALSE));
        SettingsCopyHistoryCheck().Content(winrt::box_value(Res(2511, L"Want CopyHistory")));
        SettingsCopyHistoryCheck().IsChecked(BoxBool(this->m_Context->CopyHistory != FALSE));
        SettingsFolderHistoryCheck().Content(winrt::box_value(Res(2512, L"Want FolderHistory")));
        SettingsFolderHistoryCheck().IsChecked(BoxBool(this->m_Context->FolderHistory != FALSE));
        SettingsLowercaseHashesCheck().Content(winrt::box_value(Res(2513, L"Use Lowercase Hashes")));
        SettingsLowercaseHashesCheck().IsChecked(BoxBool(this->m_Context->LowercaseHashes != FALSE));
        SettingsSizeFormatCheck().Content(winrt::box_value(Res(2526, L"Use short sizes (K/M/G)")));
        SettingsSizeFormatCheck().IsChecked(BoxBool(this->m_Context->SizeFormat != FALSE));

        SettingsFontGroupLabel().Text(Res(2514, L"Font size"));
        FontAddressBarLabel().Text(Res(2517, L"Address bar:"));
        FontListLabel().Text(Res(2519, L"File list:"));
        FontStatusBarLabel().Text(Res(2521, L"Status bar:"));
        FontDialogLabel().Text(Res(2523, L"Dialogs:"));
        FontMoreMenuLabel().Text(L"顶部更多菜单:");
        FontContextMenuLabel().Text(L"右键菜单:");

        InitFontCombo(FontAddressBarCombo(), this->m_Context->FontSizeAddressBar);
        InitFontCombo(FontListCombo(), this->m_Context->FontSizeList);
        InitFontCombo(FontStatusBarCombo(), this->m_Context->FontSizeStatusBar);
        InitFontCombo(FontDialogCombo(), this->m_Context->FontSizeDialog);
        InitFontCombo(FontMoreMenuCombo(), this->m_Context->FontSizeMoreMenu);
        InitFontCombo(FontContextMenuCombo(), this->m_Context->FontSizeContextMenu);

        // ============ Integration (menu) page ============
        MenuAssociateButton().Content(winrt::box_value(Res(12201, L"Open Windows Settings app to associate files with NanaZip")));
        MenuElimDupCheck().Content(winrt::box_value(Res(3430, L"Eliminate duplication of root folder")));
        MenuElimDupCheck().IsChecked(BoxBool(this->m_Context->ElimDup != FALSE));
        MenuZoneLabel().Text(Res(3440, L"Propagate Zone.Id stream:"));
        MenuContextItemsLabel().Text(Res(2303, L"Context menu items:"));
        MenuFileContextItemsLabel().Text(
            Res(2560, L"File Manager context menu:"));
        MenuExtractOnOpenCheck().Content(winrt::box_value(Res(3434, L"Extract on open")));
        MenuExtractOnOpenCheck().IsChecked(BoxBool(this->m_Context->ExtractOnOpen != FALSE));

        // Zone combo. Skip empty entries: the caller only fills
        // ZoneItems[3] when the current zone is an explicit custom value,
        // leaving the fourth slot empty (an empty entry would otherwise
        // show a blank row in the drop-down).
        MenuZoneCombo().Items().Clear();
        for (unsigned i = 0; i < 4; i++)
        {
            if (this->m_Context->ZoneItems[i][0] == L'\0')
            {
                continue;
            }
            MenuZoneCombo().Items().Append(winrt::box_value(
                winrt::hstring(this->m_Context->ZoneItems[i])));
        }
        MenuZoneCombo().SelectedIndex((int)this->m_Context->ZoneSel);

        // Context menu item checkboxes.
        MenuContextListPanel().Children().Clear();
        for (unsigned i = 0; i < 13; i++)
        {
            winrt::Windows::UI::Xaml::Controls::CheckBox Item;
            Item.Content(winrt::box_value(
                winrt::hstring(this->m_Context->ContextNames[i])));
            Item.IsChecked(BoxBool(this->m_Context->ContextFlags[i] != FALSE));
            Item.Margin(winrt::Windows::UI::Xaml::Thickness(0, 4, 0, 0));
            Item.Click({ this, &SettingsPage::MenuCheckBoxClick });
            MenuContextListPanel().Children().Append(Item);
            m_MenuChecks[i] = Item;
        }

        // FileManager internal file-list context-menu item checkboxes.
        MenuFileContextListPanel().Children().Clear();
        for (unsigned i = 0; i < 28; i++)
        {
            winrt::Windows::UI::Xaml::Controls::CheckBox Item;
            Item.Content(winrt::box_value(
                winrt::hstring(this->m_Context->FileContextMenuNames[i])));
            Item.IsChecked(BoxBool(
                this->m_Context->FileContextMenuFlags[i] != FALSE));
            Item.Margin(winrt::Windows::UI::Xaml::Thickness(0, 4, 0, 0));
            Item.Click({ this, &SettingsPage::MenuCheckBoxClick });
            MenuFileContextListPanel().Children().Append(Item);
            m_FileContextMenuChecks[i] = Item;
        }

        // ============ Folders page ============
        FoldersWorkFolderLabel().Text(Res(2401, L"Working folder"));
        WorkModeSystemRadio().Content(winrt::box_value(Res(2402, L"System temp folder")));
        WorkModeCurrentRadio().Content(winrt::box_value(Res(2403, L"Current")));
        WorkModeSpecifiedRadio().Content(winrt::box_value(Res(2404, L"Specified:")));
        ForRemovableOnlyCheck().Content(winrt::box_value(Res(2405, L"Use for removable drives only")));
        ForRemovableOnlyCheck().IsChecked(BoxBool(this->m_Context->ForRemovableOnly != FALSE));

        switch (this->m_Context->WorkMode)
        {
            case 0: WorkModeSystemRadio().IsChecked(BoxBool(true)); break;
            case 1: WorkModeCurrentRadio().IsChecked(BoxBool(true)); break;
            default: WorkModeSpecifiedRadio().IsChecked(BoxBool(true)); break;
        }
        WorkPathTextBox().Text(winrt::hstring(this->m_Context->WorkPath));

        // ============ Editor page ============
        EditorViewerLabel().Text(Res(543, L"View:"));
        EditorEditorLabel().Text(Res(2104, L"Editor:"));
        EditorDiffLabel().Text(Res(2105, L"Diff:"));
        EditorViewerTextBox().Text(winrt::hstring(this->m_Context->EditorPaths[0]));
        EditorEditorTextBox().Text(winrt::hstring(this->m_Context->EditorPaths[1]));
        EditorDiffTextBox().Text(winrt::hstring(this->m_Context->EditorPaths[2]));

        // ============ Extract settings page ============
        ExtractDeleteAfterCheck().Content(winrt::box_value(Res(2527, L"Delete archive after extraction")));
        ExtractDeleteAfterCheck().IsChecked(BoxBool(this->m_Context->DeleteAfterExtract != FALSE));
        ExtractDeletePermanentlyCheck().Content(winrt::box_value(Res(2528, L"Permanent deletion")));
        ExtractDeletePermanentlyCheck().IsChecked(BoxBool(this->m_Context->DeletePermanently != FALSE));

        ExtractAutoMatchGroupLabel().Text(Res(2542, L"Automatic matching"));
        ExtractAutoQueryCloudCheck().Content(winrt::box_value(Res(2530, L"Auto query cloud password")));
        ExtractAutoQueryCloudCheck().IsChecked(BoxBool(this->m_Context->AutoQueryCloud != FALSE));
        // Cloud API credentials stay visible so they can be configured
        // before automatic cloud querying is enabled.
        ExtractApiConfigGrid().Visibility(
            winrt::Windows::UI::Xaml::Visibility::Visible);
        ExtractApiUrlLabel().Text(Res(2546, L"API URL:"));
        ExtractApiAppIdLabel().Text(Res(2547, L"AppID:"));
        ExtractApiAesKeyLabel().Text(Res(2548, L"AES key:"));
        ExtractApiSigningKeyLabel().Text(Res(2549, L"Signing key:"));
        ExtractApiPackageLabel().Text(Res(2550, L"Package:"));
        ExtractApiFingerprintLabel().Text(Res(2551, L"Fingerprint:"));
        ExtractApiProtocolVersionLabel().Text(Res(2582, L"Protocol version:"));
        ExtractApiTimeoutLabel().Text(Res(2583, L"Timeout seconds:"));
        ExtractApiUrlBox().Text(winrt::hstring(this->m_Context->ApiUrl));
        ExtractApiAppIdBox().Text(winrt::hstring(this->m_Context->ApiAppId));
        ExtractApiAesKeyBox().Text(winrt::hstring(this->m_Context->ApiAesKey));
        ExtractApiSigningKeyBox().Text(winrt::hstring(this->m_Context->ApiSigningKey));
        ExtractApiPackageBox().Text(winrt::hstring(this->m_Context->ApiPackageName));
        ExtractApiFingerprintBox().Text(winrt::hstring(this->m_Context->ApiFingerprint));
        ExtractApiProtocolVersionBox().Text(winrt::hstring(this->m_Context->ApiProtocolVersion));
        ExtractApiTimeoutBox().Text(winrt::to_hstring(this->m_Context->ApiTimeoutSeconds));

        ExtractAutoMatchLocalCheck().Content(winrt::box_value(Res(2531, L"Auto match local password")));
        ExtractAutoMatchLocalCheck().IsChecked(BoxBool(this->m_Context->AutoMatchLocal != FALSE));
        ExtractMatchPriorityLabel().Text(Res(2552, L"Match priority:"));
        ExtractMatchPriorityCombo().Items().Clear();
        ExtractMatchPriorityCombo().Items().Append(winrt::box_value(Res(2544, L"Local first")));
        ExtractMatchPriorityCombo().Items().Append(winrt::box_value(Res(2545, L"Cloud first")));
        ExtractMatchPriorityCombo().Items().Append(winrt::box_value(Res(2559, L"Mixed (parallel)")));
        DWORD PriorityIndex = 0;
        if (this->m_Context->MatchPriority == 1)
        {
            PriorityIndex = 1;
        }
        else if (this->m_Context->MatchPriority >= 2)
        {
            PriorityIndex = 2;
        }
        ExtractMatchPriorityCombo().SelectedIndex(PriorityIndex);

        ExtractAutoShowPasswordCheck().Content(winrt::box_value(Res(2533, L"Auto show password")));
        ExtractAutoShowPasswordCheck().IsChecked(BoxBool(this->m_Context->AutoShowPassword != FALSE));
        // "自动分享密码": 2553 is a NanaZip-specific free ID (no 7-Zip
        // string); resolves like every other settings label, English
        // fallback when no resource is present.
        ExtractAutoSharePasswordCheck().Content(winrt::box_value(Res(2553, L"Auto share password")));
        ExtractAutoSharePasswordCheck().IsChecked(BoxBool(this->m_Context->AutoSharePassword != FALSE));

        ExtractBookGroupLabel().Text(Res(2543, L"Local password book"));
        ExtractPasswordBookBox().Text(winrt::hstring(this->m_Context->PasswordBook));
        ExtractImportBookButton().Content(winrt::box_value(Res(2535, L"Import password txt")));

        m_InitGuard = false;

        // Apply the dialog font size to this page (0 = follow system).
        ApplyDialogFont(this->m_Context->FontSizeDialog);

        // Default to the tab the user was on last time (0..4, defensively
        // clamped; 0xFF means unknown).
        UINT32 InitialTab = this->m_Context->LastTab;
        if (InitialTab > 4)
        {
            InitialTab = 3;
        }
        SwitchTab((int)InitialTab);
    }

    void SettingsPage::TabButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);

        int Index = -1;
        if (sender == TabMenuButton())
            Index = 0;
        else if (sender == TabFoldersButton())
            Index = 1;
        else if (sender == TabEditorButton())
            Index = 2;
        else if (sender == TabSettingsButton())
            Index = 3;
        else if (sender == TabExtractButton())
            Index = 4;
        if (Index >= 0)
        {
            SwitchTab(Index);
        }
    }

    void SettingsPage::ApplyButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        if (this->m_Context && this->m_Context->ApplyCallback)
        {
            this->m_Context->ApplyCallback(this->m_Context->ApplyContext);
        }
    }

    void SettingsPage::OkButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        if (this->m_Context)
        {
            this->m_Context->OK = TRUE;
        }
        ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }

    void SettingsPage::CancelButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }

    void SettingsPage::OnPageKeyDown(
        winrt::IInspectable const& sender,
        winrt::KeyRoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        if (e.Key() == winrt::Windows::System::VirtualKey::Escape)
        {
            // Esc closes the dialog like the X button (a cancel). The page
            // hooks both KeyDown and PreviewKeyDown: PreviewKeyDown
            // intercepts Esc in the tunnelling phase before a focused
            // editable control can consume it.
            SettingsPageDiagLog(L"[S4] XAML OnPageKeyDown ESC");
            e.Handled(true);
            ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
        }
    }

    void SettingsPage::OnLoaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        // The visual tree is complete here; (re)apply the dialog font size.
        if (this->m_Context)
        {
            ApplyDialogFont(this->m_Context->FontSizeDialog);
        }
    }

    winrt::Windows::Foundation::Size SettingsPage::PrepareForShow()
    {
        if (!this->m_Context)
        {
            return winrt::Windows::Foundation::Size(540, 640);
        }

        // Unify the label column widths (font group and API group) and the
        // font combo widths from the current font size.
        AlignLabels();

        // Compute the pre-show minimum from the same established formula.
        // The XAML scrollbar has no ActualWidth before the first live layout,
        // so only this provisional pass uses the Win32 fallback; it must not
        // be written into m_ScrollBarW, which is reserved for an actual
        // XAML measurement.
        RecalcMinTrack();

        const double ScrollBarW = this->m_ScrollBarMeasured
            ? this->m_ScrollBarW
            : (double)::GetSystemMetrics(SM_CXVSCROLL);
        winrt::Windows::Foundation::Size Inf(100000.0f, 100000.0f);
        TabBar().Measure(Inf);
        const double TabBarW = TabBar().DesiredSize().Width;
        MenuAssociateButton().Measure(Inf);
        const double ButtonW = MenuAssociateButton().DesiredSize().Width;
        const double MaxW = (std::max)(
            TabBarW, ButtonW + ScrollBarW + 32.0);

        // The default height follows the tallest tab content: temporarily
        // reveal every tab and measure at the candidate width (the window
        // is not shown yet, so this cannot flicker). The host caps this at
        // 75% of the work area.
        const winrt::Windows::UI::Xaml::Visibility Prev[5] = {
            ContentMenu().Visibility(),
            ContentFolders().Visibility(),
            ContentEditor().Visibility(),
            ContentSettings().Visibility(),
            ContentExtract().Visibility() };
        ContentMenu().Visibility(winrt::Windows::UI::Xaml::Visibility::Visible);
        ContentFolders().Visibility(winrt::Windows::UI::Xaml::Visibility::Visible);
        ContentEditor().Visibility(winrt::Windows::UI::Xaml::Visibility::Visible);
        ContentSettings().Visibility(winrt::Windows::UI::Xaml::Visibility::Visible);
        ContentExtract().Visibility(winrt::Windows::UI::Xaml::Visibility::Visible);

        this->Measure(winrt::Windows::Foundation::Size(
            (float)MaxW, 100000.0f));
        const double ContentH = this->DesiredSize().Height;

        ContentMenu().Visibility(Prev[0]);
        ContentFolders().Visibility(Prev[1]);
        ContentEditor().Visibility(Prev[2]);
        ContentSettings().Visibility(Prev[3]);
        ContentExtract().Visibility(Prev[4]);

        return winrt::Windows::Foundation::Size((float)MaxW, (float)ContentH);
    }

    void SettingsPage::OnSizeChanged(
        winrt::IInspectable const& sender,
        winrt::Windows::UI::Xaml::SizeChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        if (!this->m_Context)
        {
            return;
        }

        if (!this->m_ScrollBarMeasured)
        {
            // First layout: the window is visible, so the real scrollbar
            // width can be measured (the system auto-hide setting changes
            // it) and the width candidate / minimum can be corrected.
            this->m_ScrollBarMeasured = true;
            this->MeasureScrollBarWidth();
            this->RecalcMinTrack();

            // A first-open default has no user-selected width, therefore it
            // must settle exactly on the actual XAML scrollbar measurement.
            // A remembered rectangle remains the user's size unless it no
            // longer satisfies the newly measured minimum.
            if (this->m_WindowHandle)
            {
                RECT Current = {};
                if (::GetWindowRect(this->m_WindowHandle, &Current))
                {
                    const int NeedW = this->m_Context->MinTrackW;
                    const int CurrentW = Current.right - Current.left;
                    const bool NeedResize = !this->m_HasSavedWindowRect
                        ? (NeedW > 0 && CurrentW != NeedW)
                        : (NeedW > 0 && CurrentW < NeedW);
                    if (NeedResize)
                    {
                        ::SetWindowPos(
                            this->m_WindowHandle,
                            nullptr,
                            Current.left,
                            Current.top,
                            NeedW,
                            Current.bottom - Current.top,
                            SWP_NOZORDER | SWP_NOACTIVATE);
                    }
                }
            }
        }
        else if (this->m_Context->MinTrackW <= 0)
        {
            this->RecalcMinTrack();
        }
    }

    void SettingsPage::AlignLabels()
    {
        // Clear the widths applied by a previous pass first: a TextBlock
        // with an explicit Width measures back that width instead of its
        // natural text width, which would freeze the label column at the
        // old size when the dialog font grows (the wider text would then
        // spill under the combo). Same for the combo MinWidth.
        for (auto const& Label : std::vector<winrt::Windows::UI::Xaml::Controls::TextBlock>{
                FontAddressBarLabel(), FontListLabel(),
                FontStatusBarLabel(), FontDialogLabel() })
        {
            Label.ClearValue(
                winrt::Windows::UI::Xaml::FrameworkElement::WidthProperty());
        }
        for (auto const& Combo : std::vector<winrt::Windows::UI::Xaml::Controls::ComboBox>{
                FontAddressBarCombo(), FontListCombo(),
                FontStatusBarCombo(), FontDialogCombo() })
        {
            Combo.ClearValue(
                winrt::Windows::UI::Xaml::FrameworkElement::MinWidthProperty());
        }
        for (auto const& Label : std::vector<winrt::Windows::UI::Xaml::Controls::TextBlock>{
                ExtractApiUrlLabel(), ExtractApiAppIdLabel(),
                ExtractApiAesKeyLabel(), ExtractApiSigningKeyLabel(),
                ExtractApiPackageLabel(), ExtractApiFingerprintLabel(),
                ExtractApiProtocolVersionLabel(), ExtractApiTimeoutLabel() })
        {
            Label.ClearValue(
                winrt::Windows::UI::Xaml::FrameworkElement::WidthProperty());
        }

        // Font group: unify the label column to the widest label and the
        // combo width to the widest combo, so every row shares the same
        // left edge and the same combo width (measured after the dialog
        // font has been applied).
        double MaxLabelW = 0.0;
        for (auto const& Label : std::vector<winrt::Windows::UI::Xaml::Controls::TextBlock>{
                FontAddressBarLabel(), FontListLabel(),
                FontStatusBarLabel(), FontDialogLabel() })
        {
            winrt::Windows::Foundation::Size Inf(100000.0f, 100000.0f);
            Label.Measure(Inf);
            MaxLabelW = (std::max)(MaxLabelW, (double)Label.DesiredSize().Width);
        }
        if (MaxLabelW > 0.0)
        {
            for (auto const& Label : std::vector<winrt::Windows::UI::Xaml::Controls::TextBlock>{
                    FontAddressBarLabel(), FontListLabel(),
                    FontStatusBarLabel(), FontDialogLabel() })
            {
                Label.Width(MaxLabelW);
            }
        }

        double MaxComboW = 0.0;
        for (auto const& Combo : std::vector<winrt::Windows::UI::Xaml::Controls::ComboBox>{
                FontAddressBarCombo(), FontListCombo(),
                FontStatusBarCombo(), FontDialogCombo() })
        {
            MaxComboW = (std::max)(MaxComboW, ApplyComboArrowLayout(Combo));
        }
        if (MaxComboW > 0.0)
        {
            for (auto const& Combo : std::vector<winrt::Windows::UI::Xaml::Controls::ComboBox>{
                    FontAddressBarCombo(), FontListCombo(),
                    FontStatusBarCombo(), FontDialogCombo() })
            {
                Combo.MinWidth(MaxComboW);
            }
        }

        // The Zone and extract-match-priority combos get the same arrow
        // layout treatment (enlarged arrow + dynamic arrow column + text
        // width), each sized to its own content.
        ApplyComboArrowLayout(MenuZoneCombo());
        ApplyComboArrowLayout(ExtractMatchPriorityCombo());

        // Extract API group: unify the label column to the widest label.
        double MaxApiLabelW = 0.0;
        for (auto const& Label : std::vector<winrt::Windows::UI::Xaml::Controls::TextBlock>{
                ExtractApiUrlLabel(), ExtractApiAppIdLabel(),
                ExtractApiAesKeyLabel(), ExtractApiSigningKeyLabel(),
                ExtractApiPackageLabel(), ExtractApiFingerprintLabel(),
                ExtractApiProtocolVersionLabel(), ExtractApiTimeoutLabel() })
        {
            winrt::Windows::Foundation::Size Inf(100000.0f, 100000.0f);
            Label.Measure(Inf);
            MaxApiLabelW = (std::max)(MaxApiLabelW, (double)Label.DesiredSize().Width);
        }
        if (MaxApiLabelW > 0.0)
        {
            for (auto const& Label : std::vector<winrt::Windows::UI::Xaml::Controls::TextBlock>{
                    ExtractApiUrlLabel(), ExtractApiAppIdLabel(),
                    ExtractApiAesKeyLabel(), ExtractApiSigningKeyLabel(),
                    ExtractApiPackageLabel(), ExtractApiFingerprintLabel(),
                    ExtractApiProtocolVersionLabel(), ExtractApiTimeoutLabel() })
            {
                Label.Width(MaxApiLabelW);
            }
        }
    }

    void SettingsPage::MeasureScrollBarWidth()
    {
        // The system auto-hide setting changes the XAML scrollbar template's
        // actual width. This first-live-layout measurement deliberately
        // replaces the pre-show Win32 fallback used by PrepareForShow().
        auto Scroller = ContentSettings();
        const auto PrevVisibility = Scroller.VerticalScrollBarVisibility();
        Scroller.VerticalScrollBarVisibility(
            winrt::Windows::UI::Xaml::Controls::ScrollBarVisibility::Visible);
        Scroller.UpdateLayout();

        std::vector<
            winrt::Windows::UI::Xaml::Controls::Primitives::ScrollBar> Bars;
        FindVisualChildren(Scroller, Bars);
        double MeasuredWidth = 0.0;
        for (auto const& Bar : Bars)
        {
            const double Width = Bar.ActualWidth();
            if (Bar.Orientation() ==
                winrt::Windows::UI::Xaml::Controls::Orientation::Vertical &&
                Width > 0.0)
            {
                MeasuredWidth = Width;
                break;
            }
        }

        Scroller.VerticalScrollBarVisibility(PrevVisibility);
        this->m_ScrollBarW = (MeasuredWidth > 0.0)
            ? MeasuredWidth
            : (double)::GetSystemMetrics(SM_CXVSCROLL);
    }

    void SettingsPage::RecalcMinTrack()
    {
        if (!this->m_Context || !this->m_WindowHandle)
        {
            return;
        }

        // The controls always stay on one line with their labels, so the
        // smallest the dialog can shrink to is the side-by-side layout of
        // the widest tab (the label columns already carry the current font
        // size). Every tab is measured so the minimum fits all of them.
        // The measure constraint is effectively unbounded: the current
        // window width must not cap the result, otherwise a narrow window
        // (or a font-size increase) could never grow the minimum back to
        // the real content width.
        // Same candidate as the default width: the wider of the tab bar
        // and the association button. Every other row is narrower, so this
        // is also the smallest the dialog may shrink to without clipping.
        // The natural widths already carry their symmetric paddings; no
        // extra margin is added.
        // Before the first live layout, use a local fallback without
        // claiming it is a measured XAML scrollbar width.
        const double ScrollBarW = this->m_ScrollBarMeasured
            ? this->m_ScrollBarW
            : (double)::GetSystemMetrics(SM_CXVSCROLL);

        winrt::Windows::Foundation::Size Inf(100000.0f, 100000.0f);
        TabBar().Measure(Inf);
        const double TabBarW = TabBar().DesiredSize().Width;
        MenuAssociateButton().Measure(Inf);
        const double ButtonW = MenuAssociateButton().DesiredSize().Width;
        // Same candidate as the default width: the tab bar (outside the
        // ScrollViewer, no scrollbar) or the association button plus the
        // real scrollbar width and the content area's 16px left/right
        // padding, so dragging the border can never squeeze the button
        // text.
        const double MaxW = (std::max)(
            TabBarW, ButtonW + ScrollBarW + 32.0);

        const UINT Dpi = ::GetDpiForWindow(this->m_WindowHandle);
        const float Scale = (float)Dpi / (float)USER_DEFAULT_SCREEN_DPI;

        // The content mostly scrolls vertically (every tab sits in a
        // ScrollViewer and the button row is Auto), so the vertical minimum
        // stays at a fixed floor while the horizontal minimum follows the
        // same width candidate as the default size (a larger font widens
        // it, so dragging can never clip the tab bar or the button).
        // The horizontal track minimum is exactly the established content
        // formula above. Do not impose a separate fixed pixel floor here:
        // it creates unused right-side room and prevents the user from
        // dragging to the true content minimum.
        int MinClientW = (int)(MaxW * Scale + 0.5);
        int MinClientH = (int)(420.0f * Scale + 0.5);
        if (MinClientH < 420) MinClientH = 420;

        RECT rc = { 0, 0, MinClientW, MinClientH };
        {
            const LONG_PTR Style = ::GetWindowLongPtrW(
                this->m_WindowHandle, GWL_STYLE);
            const LONG_PTR ExStyle = ::GetWindowLongPtrW(
                this->m_WindowHandle, GWL_EXSTYLE);
            ::AdjustWindowRectEx(&rc, (DWORD)Style, FALSE, (DWORD)ExStyle);
        }

        this->m_Context->MinTrackW = rc.right - rc.left;
        this->m_Context->MinTrackH = rc.bottom - rc.top;

        wchar_t Buffer[256] = {};
        swprintf_s(
            Buffer,
            L"[S5] MinTrack: Tab=%.1f Button=%.1f Scroll=%.1f Max=%.1f W=%ld H=%ld",
            TabBarW,
            ButtonW,
            ScrollBarW,
            MaxW,
            this->m_Context->MinTrackW,
            this->m_Context->MinTrackH);
        SettingsPageDiagLog(Buffer);
    }

    void SettingsPage::RefreshAfterFontChange()
    {
        if (!this->m_Context || !this->m_WindowHandle)
        {
            return;
        }

        // A larger dialog font widens the tab bar and the association
        // button; a smaller one narrows them. Re-align the columns,
        // recompute the minimum track size from the same width candidate,
        // then resize the window width to the new candidate (both wider
        // and narrower) so the dialog always follows the text; the height
        // is left untouched (the user may have dragged it).
        AlignLabels();
        RecalcMinTrack();

        if (::IsZoomed(this->m_WindowHandle))
        {
            return;
        }

        RECT Current = {};
        ::GetWindowRect(this->m_WindowHandle, &Current);
        RECT rc = { 0, 0, this->m_Context->MinTrackW, this->m_Context->MinTrackH };
        const int NeedW = rc.right - rc.left;
        ::SetWindowPos(
            this->m_WindowHandle,
            nullptr,
            Current.left,
            Current.top,
            NeedW,
            Current.bottom - Current.top,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }

    // ============ Settings page ============

    void SettingsPage::SettingsCheckBoxClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);

        if (this->m_InitGuard)
        {
            return;
        }
        bool Checked = GetBool(
            sender.as<winrt::Windows::UI::Xaml::Controls::CheckBox>().
            IsChecked());
        if (sender == SettingsShowDotsCheck())
            this->m_Context->ShowDots = Checked ? TRUE : FALSE;
        else if (sender == SettingsShowRealFileIconsCheck())
            this->m_Context->ShowRealFileIcons = Checked ? TRUE : FALSE;
        else if (sender == SettingsFullRowCheck())
            this->m_Context->FullRow = Checked ? TRUE : FALSE;
        else if (sender == SettingsShowGridCheck())
            this->m_Context->ShowGrid = Checked ? TRUE : FALSE;
        else if (sender == SettingsSingleClickCheck())
            this->m_Context->SingleClick = Checked ? TRUE : FALSE;
        else if (sender == SettingsAlternativeSelectionCheck())
            this->m_Context->AlternativeSelection = Checked ? TRUE : FALSE;
        else if (sender == SettingsShowSystemMenuCheck())
            this->m_Context->ShowSystemMenu = Checked ? TRUE : FALSE;
        else if (sender == SettingsLargePagesCheck())
            this->m_Context->LargePages = Checked ? TRUE : FALSE;
        else if (sender == SettingsArcHistoryCheck())
            this->m_Context->ArcHistory = Checked ? TRUE : FALSE;
        else if (sender == SettingsPathHistoryCheck())
            this->m_Context->PathHistory = Checked ? TRUE : FALSE;
        else if (sender == SettingsCopyHistoryCheck())
            this->m_Context->CopyHistory = Checked ? TRUE : FALSE;
        else if (sender == SettingsFolderHistoryCheck())
            this->m_Context->FolderHistory = Checked ? TRUE : FALSE;
        else if (sender == SettingsLowercaseHashesCheck())
            this->m_Context->LowercaseHashes = Checked ? TRUE : FALSE;
        else if (sender == SettingsSizeFormatCheck())
            this->m_Context->SizeFormat = Checked ? TRUE : FALSE;
    }

    void SettingsPage::FontComboChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);

        if (this->m_InitGuard)
        {
            return;
        }
        int Index = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Index < 0)
        {
            return;
        }
        static const UINT32 kFontValues[] = { 0, 8, 9, 10, 11, 12, 14, 16, 18, 20, 24 };
        UINT32 Pt = (Index < (int)ARRAYSIZE(kFontValues)) ?
            kFontValues[Index] : 0;
        if (sender == FontAddressBarCombo())
            this->m_Context->FontSizeAddressBar = Pt;
        else if (sender == FontListCombo())
            this->m_Context->FontSizeList = Pt;
        else if (sender == FontStatusBarCombo())
            this->m_Context->FontSizeStatusBar = Pt;
        else if (sender == FontMoreMenuCombo())
            this->m_Context->FontSizeMoreMenu = Pt;
        else if (sender == FontContextMenuCombo())
            this->m_Context->FontSizeContextMenu = Pt;
    }

    void SettingsPage::FontDialogComboChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);

        if (this->m_InitGuard)
        {
            return;
        }
        int Index = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Index < 0)
        {
            return;
        }
        static const UINT32 kFontValues[] = { 0, 8, 9, 10, 11, 12, 14, 16, 18, 20, 24 };
        UINT32 Pt = (Index < (int)ARRAYSIZE(kFontValues)) ?
            kFontValues[Index] : 0;
        this->m_Context->FontSizeDialog = Pt;
        ApplyDialogFont(Pt);
        RefreshAfterFontChange();
    }

    // ============ Integration (menu) page ============

    void SettingsPage::MenuAssociateButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        SHELLEXECUTEINFOW ExecInfo = {};
        ExecInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
        ExecInfo.lpVerb = L"open";
        ExecInfo.lpFile = L"ms-settings:defaultapps";
        ExecInfo.nShow = SW_SHOWNORMAL;
        ::ShellExecuteExW(&ExecInfo);
    }

    void SettingsPage::MenuCheckBoxClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);

        if (this->m_InitGuard)
        {
            return;
        }
        bool Checked = GetBool(
            sender.as<winrt::Windows::UI::Xaml::Controls::CheckBox>().
            IsChecked());
        if (sender == MenuElimDupCheck())
        {
            this->m_Context->ElimDup = Checked ? TRUE : FALSE;
        }
        else if (sender == MenuExtractOnOpenCheck())
        {
            this->m_Context->ExtractOnOpen = Checked ? TRUE : FALSE;
        }
        else
        {
            for (unsigned i = 0; i < 13; i++)
            {
                if (sender == m_MenuChecks[i])
                {
                    this->m_Context->ContextFlags[i] = Checked ? TRUE : FALSE;
                    return;
                }
            }
            for (unsigned i = 0; i < 28; i++)
            {
                if (sender == m_FileContextMenuChecks[i])
                {
                    this->m_Context->FileContextMenuFlags[i] =
                        Checked ? TRUE : FALSE;
                    return;
                }
            }
        }
    }

    void SettingsPage::MenuZoneComboChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);

        if (this->m_InitGuard)
        {
            return;
        }
        int Index = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Index >= 0 && Index < 4)
        {
            // The list index is stored in ZoneSel; the caller maps it
            // back to the NZoneIdMode value on save (the visible order is
            // "Yes/No/Office/custom", which no longer equals the enum
            // values).
            this->m_Context->ZoneSel = (UINT32)Index;
        }
    }

    // ============ Folders page ============

    void SettingsPage::WorkModeRadioClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);

        if (this->m_InitGuard)
        {
            return;
        }
        if (sender == WorkModeSystemRadio())
            this->m_Context->WorkMode = 0;
        else if (sender == WorkModeCurrentRadio())
            this->m_Context->WorkMode = 1;
        else if (sender == WorkModeSpecifiedRadio())
            this->m_Context->WorkMode = 2;
    }

    void SettingsPage::WorkPathTextChanged(
        winrt::IInspectable const& sender,
        winrt::TextChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);

        if (this->m_InitGuard)
        {
            return;
        }
        std::wstring Path = sender.as<winrt::Windows::UI::Xaml::Controls::TextBox>().
            Text().c_str();
        wcsncpy_s(this->m_Context->WorkPath, Path.c_str(), _TRUNCATE);
    }

    void SettingsPage::WorkPathBrowseButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        BROWSEINFOW BrowseInfo = {};
        BrowseInfo.hwndOwner = this->m_WindowHandle;
        BrowseInfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        BrowseInfo.lpszTitle = Res(2406, L"Select the working folder").c_str();
        LPITEMIDLIST Pidl = ::SHBrowseForFolderW(&BrowseInfo);
        if (Pidl)
        {
            wchar_t Path[MAX_PATH] = {};
            if (::SHGetPathFromIDListW(Pidl, Path))
            {
                WorkPathTextBox().Text(winrt::hstring(Path));
                wcsncpy_s(this->m_Context->WorkPath, Path, _TRUNCATE);
            }
            ::CoTaskMemFree(Pidl);
        }
    }

    void SettingsPage::FoldersCheckBoxClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);

        if (this->m_InitGuard)
        {
            return;
        }
        bool Checked = GetBool(
            sender.as<winrt::Windows::UI::Xaml::Controls::CheckBox>().
            IsChecked());
        if (sender == ForRemovableOnlyCheck())
        {
            this->m_Context->ForRemovableOnly = Checked ? TRUE : FALSE;
        }
    }

    // ============ Editor page ============

    void SettingsPage::EditorPathTextChanged(
        winrt::IInspectable const& sender,
        winrt::TextChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);

        if (this->m_InitGuard)
        {
            return;
        }
        std::wstring Path = sender.as<winrt::Windows::UI::Xaml::Controls::TextBox>().
            Text().c_str();
        int Index = -1;
        if (sender == EditorViewerTextBox())
            Index = 0;
        else if (sender == EditorEditorTextBox())
            Index = 1;
        else if (sender == EditorDiffTextBox())
            Index = 2;
        if (Index >= 0)
        {
            wcsncpy_s(this->m_Context->EditorPaths[Index], Path.c_str(), _TRUNCATE);
        }
    }

    void SettingsPage::EditorBrowseButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);

        int Index = -1;
        if (sender == EditorViewerBrowseButton())
            Index = 0;
        else if (sender == EditorEditorBrowseButton())
            Index = 1;
        else if (sender == EditorDiffBrowseButton())
            Index = 2;
        if (Index < 0)
        {
            return;
        }

        wchar_t FileName[MAX_PATH * 2] = {};
        OPENFILENAMEW Ofn = {};
        Ofn.lStructSize = sizeof(Ofn);
        Ofn.hwndOwner = this->m_WindowHandle;
        Ofn.lpstrFilter = L"Executable files (*.exe)\0*.exe\0All files (*.*)\0*.*\0";
        Ofn.lpstrFile = FileName;
        Ofn.nMaxFile = ARRAYSIZE(FileName);
        Ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
        if (!::GetOpenFileNameW(&Ofn))
        {
            return;
        }

        if (Index == 0)
        {
            EditorViewerTextBox().Text(winrt::hstring(FileName));
        }
        else if (Index == 1)
        {
            EditorEditorTextBox().Text(winrt::hstring(FileName));
        }
        else
        {
            EditorDiffTextBox().Text(winrt::hstring(FileName));
        }
        wcsncpy_s(this->m_Context->EditorPaths[Index], FileName, _TRUNCATE);
    }

    // ============ Extract settings page ============

    void SettingsPage::ExtractCheckBoxClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);

        if (this->m_InitGuard)
        {
            return;
        }
        bool Checked = GetBool(
            sender.as<winrt::Windows::UI::Xaml::Controls::CheckBox>().
            IsChecked());
        if (sender == ExtractDeleteAfterCheck())
            this->m_Context->DeleteAfterExtract = Checked ? TRUE : FALSE;
        else if (sender == ExtractDeletePermanentlyCheck())
            this->m_Context->DeletePermanently = Checked ? TRUE : FALSE;
        else if (sender == ExtractAutoQueryCloudCheck())
        {
            // This controls automatic requests only; saved cloud credentials
            // remain visible and editable regardless of its checked state.
            this->m_Context->AutoQueryCloud = Checked ? TRUE : FALSE;
        }
        else if (sender == ExtractAutoMatchLocalCheck())
            this->m_Context->AutoMatchLocal = Checked ? TRUE : FALSE;
        else if (sender == ExtractAutoShowPasswordCheck())
            this->m_Context->AutoShowPassword = Checked ? TRUE : FALSE;
        else if (sender == ExtractAutoSharePasswordCheck())
            this->m_Context->AutoSharePassword = Checked ? TRUE : FALSE;
    }

    void SettingsPage::ExtractApiTextChanged(
        winrt::IInspectable const& sender,
        winrt::TextChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);

        if (this->m_InitGuard)
        {
            return;
        }
        std::wstring Value = sender.as<winrt::Windows::UI::Xaml::Controls::TextBox>().
            Text().c_str();
        wchar_t* Target = nullptr;
        if (sender == ExtractApiUrlBox())
            Target = this->m_Context->ApiUrl;
        else if (sender == ExtractApiAppIdBox())
            Target = this->m_Context->ApiAppId;
        else if (sender == ExtractApiAesKeyBox())
            Target = this->m_Context->ApiAesKey;
        else if (sender == ExtractApiSigningKeyBox())
            Target = this->m_Context->ApiSigningKey;
        else if (sender == ExtractApiPackageBox())
            Target = this->m_Context->ApiPackageName;
        else if (sender == ExtractApiFingerprintBox())
            Target = this->m_Context->ApiFingerprint;
        else if (sender == ExtractApiProtocolVersionBox())
            Target = this->m_Context->ApiProtocolVersion;
        else if (sender == ExtractApiTimeoutBox())
        {
            const std::wstring TimeoutText = ExtractApiTimeoutBox().Text().c_str();
            const unsigned long Timeout = wcstoul(TimeoutText.c_str(), nullptr, 10);
            if (Timeout >= 1 && Timeout <= 30)
                this->m_Context->ApiTimeoutSeconds = static_cast<UINT32>(Timeout);
            else
                this->m_Context->ApiTimeoutSeconds = 5;
            this->m_Context->DirtyApi = TRUE;
            return;
        }
        if (Target)
        {
            wcsncpy_s(Target, 256, Value.c_str(), _TRUNCATE);
            this->m_Context->DirtyApi = TRUE;
        }
    }

    void SettingsPage::ExtractMatchPriorityChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);

        if (this->m_InitGuard)
        {
            return;
        }
        int Index = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Index >= 0)
        {
            this->m_Context->MatchPriority = (Index == 1) ? 1
                : (Index >= 2) ? 2 : 0;
        }
    }

    void SettingsPage::ExtractBookTextChanged(
        winrt::IInspectable const& sender,
        winrt::TextChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);

        if (this->m_InitGuard)
        {
            return;
        }
        std::wstring Value = sender.as<winrt::Windows::UI::Xaml::Controls::TextBox>().
            Text().c_str();
        wcsncpy_s(this->m_Context->PasswordBook, 4096, Value.c_str(), _TRUNCATE);
    }

    void SettingsPage::ExtractImportBookClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        wchar_t FileName[MAX_PATH * 4] = {};
        OPENFILENAMEW Ofn = {};
        Ofn.lStructSize = sizeof(Ofn);
        Ofn.hwndOwner = this->m_WindowHandle;
        Ofn.lpstrFilter = L"Text files (*.txt)\0*.txt\0All files (*.*)\0*.*\0";
        Ofn.lpstrFile = FileName;
        Ofn.nMaxFile = ARRAYSIZE(FileName);
        Ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
        if (!::GetOpenFileNameW(&Ofn))
        {
            return;
        }

        HANDLE File = ::CreateFileW(
            FileName,
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (File == INVALID_HANDLE_VALUE)
        {
            return;
        }
        DWORD FileSize = ::GetFileSize(File, nullptr);
        std::string Buffer;
        if (FileSize > 0 && FileSize < 1024 * 1024)
        {
            Buffer.resize(FileSize);
            DWORD Read = 0;
            ::ReadFile(File, &Buffer[0], FileSize, &Read, nullptr);
            Buffer.resize(Read);
        }
        ::CloseHandle(File);

        // Try UTF-8 first, fall back to ANSI.
        std::wstring Text;
        if (!Buffer.empty())
        {
            int WLength = ::MultiByteToWideChar(
                CP_UTF8, 0, Buffer.c_str(), (int)Buffer.size(), nullptr, 0);
            if (WLength > 0)
            {
                Text.resize(WLength);
                ::MultiByteToWideChar(
                    CP_UTF8, 0, Buffer.c_str(), (int)Buffer.size(), &Text[0], WLength);
            }
        }
        ExtractPasswordBookBox().Text(winrt::hstring(Text));
        wcsncpy_s(this->m_Context->PasswordBook, 4096, Text.c_str(), _TRUNCATE);
    }
}
