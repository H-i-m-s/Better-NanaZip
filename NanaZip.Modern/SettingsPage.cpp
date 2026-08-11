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
#include <winrt/Windows.UI.Xaml.Media.h>

namespace winrt::NanaZip::Modern::implementation
{
    SettingsPage::SettingsPage(
        _In_opt_ HWND WindowHandle,
        _In_ PK7_SETTINGS_DIALOG_CONTEXT Context) :
        m_WindowHandle(WindowHandle),
        m_Context(Context),
        m_InitGuard(false)
    {
        // Remember the final window position and size when the dialog is
        // closed (including via the system close button), and apply the
        // dialog font size once the visual tree is fully realized.
        this->Unloaded({ this, &SettingsPage::OnUnloaded });
        this->Loaded({ this, &SettingsPage::OnLoaded });
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

    void SettingsPage::UpdateWindowRect()
    {
        if (this->m_Context && this->m_WindowHandle)
        {
            ::GetWindowRect(this->m_WindowHandle, &this->m_Context->WindowRect);
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

        InitFontCombo(FontAddressBarCombo(), this->m_Context->FontSizeAddressBar);
        InitFontCombo(FontListCombo(), this->m_Context->FontSizeList);
        InitFontCombo(FontStatusBarCombo(), this->m_Context->FontSizeStatusBar);
        InitFontCombo(FontDialogCombo(), this->m_Context->FontSizeDialog);

        // ============ Integration (menu) page ============
        MenuAssociateButton().Content(winrt::box_value(Res(12201, L"Open Windows Settings app to associate files with NanaZip")));
        MenuElimDupCheck().Content(winrt::box_value(Res(3430, L"Eliminate duplication of root folder")));
        MenuElimDupCheck().IsChecked(BoxBool(this->m_Context->ElimDup != FALSE));
        MenuZoneLabel().Text(Res(3440, L"Propagate Zone.Id stream:"));
        MenuContextItemsLabel().Text(Res(2303, L"Context menu items:"));
        MenuExtractOnOpenCheck().Content(winrt::box_value(Res(3434, L"Extract on open")));
        MenuExtractOnOpenCheck().IsChecked(BoxBool(this->m_Context->ExtractOnOpen != FALSE));

        // Zone combo.
        MenuZoneCombo().Items().Clear();
        for (unsigned i = 0; i < 4; i++)
        {
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
        ExtractApiUrlLabel().Text(Res(2546, L"API URL:"));
        ExtractApiAppIdLabel().Text(Res(2547, L"AppID:"));
        ExtractApiAesKeyLabel().Text(Res(2548, L"AES key:"));
        ExtractApiSigningKeyLabel().Text(Res(2549, L"Signing key:"));
        ExtractApiPackageLabel().Text(Res(2550, L"Package:"));
        ExtractApiFingerprintLabel().Text(Res(2551, L"Fingerprint:"));
        ExtractApiUrlBox().Text(winrt::hstring(this->m_Context->ApiUrl));
        ExtractApiAppIdBox().Text(winrt::hstring(this->m_Context->ApiAppId));
        ExtractApiAesKeyBox().Text(winrt::hstring(this->m_Context->ApiAesKey));
        ExtractApiSigningKeyBox().Text(winrt::hstring(this->m_Context->ApiSigningKey));
        ExtractApiPackageBox().Text(winrt::hstring(this->m_Context->ApiPackageName));
        ExtractApiFingerprintBox().Text(winrt::hstring(this->m_Context->ApiFingerprint));

        ExtractAutoMatchLocalCheck().Content(winrt::box_value(Res(2531, L"Auto match local password")));
        ExtractAutoMatchLocalCheck().IsChecked(BoxBool(this->m_Context->AutoMatchLocal != FALSE));
        ExtractMatchPriorityLabel().Text(Res(2552, L"Match priority:"));
        ExtractMatchPriorityCombo().Items().Clear();
        ExtractMatchPriorityCombo().Items().Append(winrt::box_value(Res(2544, L"Local first")));
        ExtractMatchPriorityCombo().Items().Append(winrt::box_value(Res(2545, L"Cloud first")));
        ExtractMatchPriorityCombo().SelectedIndex(
            this->m_Context->MatchPriority != 0 ? 1 : 0);

        ExtractAutoShowPasswordCheck().Content(winrt::box_value(Res(2533, L"Auto show password")));
        ExtractAutoShowPasswordCheck().IsChecked(BoxBool(this->m_Context->AutoShowPassword != FALSE));

        ExtractBookGroupLabel().Text(Res(2543, L"Local password book"));
        ExtractPasswordBookBox().Text(winrt::hstring(this->m_Context->PasswordBook));
        ExtractImportBookButton().Content(winrt::box_value(Res(2535, L"Import password txt")));

        m_InitGuard = false;

        // Apply the dialog font size to this page (0 = follow system).
        ApplyDialogFont(this->m_Context->FontSizeDialog);

        // Default to the Settings tab.
        SwitchTab(3);
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
            UpdateWindowRect();
        }
        ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }

    void SettingsPage::CancelButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        UpdateWindowRect();
        ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }

    void SettingsPage::OnUnloaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        // Covers the system close button (the last chance to snapshot the
        // window rect before the window is destroyed).
        UpdateWindowRect();
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
                    break;
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
            this->m_Context->WriteZone = (UINT32)Index;
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
            this->m_Context->AutoQueryCloud = Checked ? TRUE : FALSE;
        else if (sender == ExtractAutoMatchLocalCheck())
            this->m_Context->AutoMatchLocal = Checked ? TRUE : FALSE;
        else if (sender == ExtractAutoShowPasswordCheck())
            this->m_Context->AutoShowPassword = Checked ? TRUE : FALSE;
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
            this->m_Context->MatchPriority = (Index == 1) ? 1 : 0;
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
