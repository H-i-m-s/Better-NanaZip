#include "pch.h"
#include "CompressPage.h"
#if __has_include("CompressPage.g.cpp")
#include "CompressPage.g.cpp"
#endif

#include "NanaZip.Modern.h"

#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.h>

#include <algorithm>
#include <cwctype>
#include <string>

namespace winrt::NanaZip::Modern::implementation
{
    // Border has no IsEnabled (it is not a Control). Recursively apply
    // IsEnabled to every Control inside a container subtree instead.
    static void SetTreeEnabled(
        winrt::Windows::UI::Xaml::UIElement const& Element,
        bool Enabled)
    {
        if (auto Control =
            Element.try_as<winrt::Windows::UI::Xaml::Controls::Control>())
        {
            Control.IsEnabled(Enabled);
        }
        if (auto Panel =
            Element.try_as<winrt::Windows::UI::Xaml::Controls::Panel>())
        {
            for (auto const& Child : Panel.Children())
            {
                SetTreeEnabled(Child, Enabled);
            }
        }
        if (auto Border =
            Element.try_as<winrt::Windows::UI::Xaml::Controls::Border>())
        {
            if (auto Child = Border.Child())
            {
                SetTreeEnabled(Child, Enabled);
            }
        }
    }

    CompressPage::CompressPage(
        _In_opt_ HWND WindowHandle,
        _In_ PK7_COMPRESS_DIALOG_CONTEXT Context) :
        m_WindowHandle(WindowHandle),
        m_Context(Context),
        m_InitGuard(false),
        m_OkClicked(false)
    {
        this->Unloaded({ this, &CompressPage::OnUnloaded });
        this->Loaded({ this, &CompressPage::OnLoaded });
    }

    void CompressPage::InitializeComponent()
    {
        CompressPageT::InitializeComponent();
    }

    winrt::hstring CompressPage::Res(
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

    void CompressPage::ApplyDialogFont(UINT32 Pt)
    {
        double FontSizePx = (Pt == 0) ? 0.0 : (double)Pt * 96.0 / 72.0;
        ApplyFontToTree(*this, FontSizePx);
    }

    void CompressPage::ApplyFontToTree(
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

    void CompressPage::ApplyOptionList(
        winrt::Windows::UI::Xaml::Controls::ComboBox const& Combo,
        _In_ const K7_COMPRESS_OPTION_LIST& List)
    {
        Combo.Items().Clear();
        for (UINT32 i = 0; i < List.Count && i < K7_COMPRESS_MAX_OPTION_ITEMS; i++)
        {
            Combo.Items().Append(winrt::box_value(
                winrt::hstring(List.Items[i].DisplayText)));
        }
        if (List.Count > 0)
        {
            if (List.SelectedIndex >= 0 &&
                (UINT32)List.SelectedIndex < List.Count)
            {
                Combo.SelectedIndex(List.SelectedIndex);
            }
            else
            {
                Combo.SelectedIndex(0);
            }
        }
        else
        {
            Combo.SelectedIndex(-1);
        }
        // Editable combos (archive path, volume) must stay enabled even when
        // the option list is empty so the user can still type.
        if (!Combo.IsEditable())
        {
            Combo.IsEnabled(List.Enabled && List.Count > 0);
        }
    }

    void CompressPage::ApplySnapshotToUi()
    {
        if (!this->m_Context)
        {
            return;
        }

        PK7_COMPRESS_DIALOG_CONTEXT Context = this->m_Context;

        // Static labels are loaded once in ApplyLabels.
        ArchivePathCombo().Text(winrt::hstring(Context->ArchivePath));
        ParametersBox().Text(winrt::hstring(Context->Parameters));
        VolumeCombo().Text(winrt::hstring(Context->VolumeText));

        ApplyOptionList(FormatCombo(), Context->Formats);
        ApplyOptionList(LevelCombo(), Context->Levels);
        ApplyOptionList(MethodCombo(), Context->Methods);
        ApplyOptionList(DictionaryCombo(), Context->Dictionaries);
        ApplyOptionList(OrderCombo(), Context->Orders);
        ApplyOptionList(SolidCombo(), Context->SolidBlocks);
        ApplyOptionList(ThreadsCombo(), Context->Threads);
        ApplyOptionList(MemoryCombo(), Context->MemoryLimits);
        ApplyOptionList(UpdateModeCombo(), Context->UpdateModes);
        ApplyOptionList(PathModeCombo(), Context->PathModes);
        ApplyOptionList(EncryptionMethodCombo(), Context->EncryptionMethods);
        ApplyOptionList(VolumeCombo(), Context->Volumes);

        // Capability and visibility flags.
        SfxCheck().IsChecked(BoxBool(Context->SfxMode != FALSE));
        SfxCheck().IsEnabled(Context->SfxEnabled != FALSE);
        SfxCheck().Visibility(Context->SfxVisible
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);

        SharedCheck().IsChecked(BoxBool(Context->OpenShareForWrite != FALSE));
        DeleteCheck().IsChecked(BoxBool(Context->DeleteAfterCompressing != FALSE));

        EncryptionBorder().Visibility(Context->EncryptionVisible
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        SetTreeEnabled(
            EncryptionBorder(),
            Context->EncryptionEnabled != FALSE);

        EncryptHeadersCheck().IsChecked(BoxBool(Context->EncryptHeaders != FALSE));
        EncryptHeadersCheck().IsEnabled(Context->EncryptHeadersAllowed != FALSE);

        ShowPasswordCheck().IsChecked(BoxBool(Context->ShowPassword != FALSE));
        PasswordBox().Password(winrt::hstring(Context->Password));
        ReenterPasswordBox().Password(winrt::hstring(Context->PasswordConfirmation));
        UpdatePasswordControl();

        MemoryValueText().Text(winrt::hstring(Context->MemoryValueText));
        DecompressMemoryText().Text(winrt::hstring(Context->DecompressMemoryText));
        DecompressMemoryText().Visibility(Context->DecompressMemoryVisible
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        HardwareThreadsText().Text(winrt::hstring(Context->HardwareThreadsText));
        ArchiveFolderText().Text(winrt::hstring(Context->ArchiveFolderText));
        OptionsSummaryText().Text(winrt::hstring(Context->OptionsSummaryText));
        OptionsButton().IsEnabled(Context->OptionsEnabled != FALSE);

        VolumeCombo().Visibility(Context->VolumeVisible
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        VolumeText().Visibility(Context->VolumeVisible
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        ParametersBox().Visibility(Context->ParametersVisible
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        ParametersText().Visibility(Context->ParametersVisible
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);

        ErrorTextBlock().Text(winrt::hstring(Context->ErrorText));
        ErrorTextBlock().Visibility(Context->ErrorText[0]
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
    }

    void CompressPage::RefreshFromSnapshot()
    {
        this->m_InitGuard = true;
        ApplySnapshotToUi();
        this->m_InitGuard = false;
    }

    void CompressPage::ApplyLabels()
    {
        // The resource ids come from CompressDialogRes.h in the Universal
        // project. The fallbacks are the English UI strings.
        ArchiveFolderText().Text(Res(4001, L"&Archive:"));
        FormatText().Text(Res(4003, L"Archive &format:"));
        LevelText().Text(Res(4004, L"Compression &level:"));
        MethodText().Text(Res(4005, L"Compression &method:"));
        DictionaryText().Text(Res(4006, L"&Dictionary size:"));
        OrderText().Text(Res(4007, L"&Word size:"));
        SolidText().Text(Res(4008, L"&Solid Block size:"));
        ThreadsText().Text(Res(4009, L"Number of CPU &threads:"));
        MemoryText().Text(Res(4017, L"Memory usage for Compressing:"));
        DecompressMemoryText().Text(Res(4018, L"Memory usage for Decompressing:"));
        VolumeText().Text(Res(7302, L"Split to &volumes, bytes:"));
        ParametersText().Text(Res(4010, L"Parameters:"));
        UpdateModeText().Text(Res(4002, L"&Update mode:"));
        PathModeText().Text(Res(3410, L"Path mode:"));
        OptionsGroupText().Text(Res(4011, L"Options"));
        SfxCheck().Content(winrt::box_value(Res(4012, L"Create SF&X archive")));
        SharedCheck().Content(winrt::box_value(Res(4013, L"Compress shared files")));
        DeleteCheck().Content(winrt::box_value(Res(4019, L"Delete files after compression")));
        EncryptionGroupText().Text(Res(4014, L"Encryption"));
        PasswordText().Text(Res(3801, L"Enter &password:"));
        ReenterPasswordText().Text(Res(3802, L"Reenter password:"));
        ShowPasswordCheck().Content(winrt::box_value(Res(3803, L"Show Password")));
        EncryptionMethodText().Text(Res(4015, L"&Encryption method:"));
        EncryptHeadersCheck().Content(winrt::box_value(Res(4016, L"Encrypt file &names")));
        OptionsButton().Content(winrt::box_value(Res(4011, L"Options")));
        OkButton().Content(winrt::box_value(Res(401, L"OK")));
        CancelButton().Content(winrt::box_value(Res(402, L"Cancel")));
    }

    void CompressPage::UpdatePasswordControl()
    {
        const bool Show = GetBool(ShowPasswordCheck().IsChecked());
        PasswordBox().PasswordRevealMode(Show
            ? winrt::Windows::UI::Xaml::Controls::PasswordRevealMode::Visible
            : winrt::Windows::UI::Xaml::Controls::PasswordRevealMode::Hidden);
        ReenterPasswordBox().PasswordRevealMode(Show
            ? winrt::Windows::UI::Xaml::Controls::PasswordRevealMode::Visible
            : winrt::Windows::UI::Xaml::Controls::PasswordRevealMode::Hidden);
        ReenterPasswordBox().Visibility(
            Show ? winrt::Windows::UI::Xaml::Visibility::Collapsed :
            winrt::Windows::UI::Xaml::Visibility::Visible);
        ReenterPasswordText().Visibility(
            Show ? winrt::Windows::UI::Xaml::Visibility::Collapsed :
            winrt::Windows::UI::Xaml::Visibility::Visible);
    }

    bool CompressPage::SendCommand(
        UINT32 Command,
        INT64 Value,
        LPCWSTR SemanticText)
    {
        if (this->m_Context && this->m_Context->CommandCallback)
        {
            const bool Accepted = this->m_Context->CommandCallback(
                this->m_Context->CallbackContext,
                Command,
                Value,
                SemanticText);
            if (!Accepted)
            {
                return false;
            }
        }
        return true;
    }

    winrt::Windows::Foundation::Size CompressPage::PrepareForShow()
    {
        if (!this->m_Context)
        {
            return winrt::Windows::Foundation::Size(640, 560);
        }

        this->m_InitGuard = true;
        ApplyLabels();
        ApplySnapshotToUi();
        this->m_InitGuard = false;

        ApplyDialogFont(this->m_Context->FontSizeDialog);

        // Measure the content so the caller can size the window before it is
        // shown. The page wraps text, so an infinite height constraint lets
        // the scroll viewer report its natural height. The width is chosen
        // to fit the wider column without forcing wrap.
        winrt::Windows::Foundation::Size Inf(100000.0f, 100000.0f);
        this->Measure(Inf);
        winrt::Windows::Foundation::Size Desired = this->DesiredSize();
        return Desired;
    }

    void CompressPage::OnLoaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        this->m_InitGuard = false;
    }

    void CompressPage::OnUnloaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        // The window can be closed with the X button or Alt+F4, which never
        // passes through OnCancelClicked. Treat every close that was not
        // confirmed by OK as a cancel.
        if (this->m_Context && !this->m_OkClicked)
        {
            this->m_Context->OK = FALSE;
        }
    }

    void CompressPage::PostClose(bool Ok)
    {
        if (this->m_Context)
        {
            this->m_Context->OK = Ok ? TRUE : FALSE;
        }
        this->m_OkClicked = Ok;
        ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }

    void CompressPage::OnPageKeyDown(
        winrt::IInspectable const& sender,
        winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        if (e.Key() == winrt::Windows::System::VirtualKey::Escape)
        {
            e.Handled(true);
            if (this->m_Context)
            {
                this->m_Context->OK = FALSE;
            }
            this->m_OkClicked = false;
            ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
        }
    }

    void CompressPage::OnFormatChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_FORMAT,
            this->m_Context->Formats.Items[Sel].Value);
        RefreshFromSnapshot();
    }

    void CompressPage::OnLevelChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_LEVEL,
            this->m_Context->Levels.Items[Sel].Value);
        RefreshFromSnapshot();
    }

    void CompressPage::OnMethodChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_METHOD,
            this->m_Context->Methods.Items[Sel].Value,
            this->m_Context->Methods.Items[Sel].SemanticText);
        RefreshFromSnapshot();
    }

    void CompressPage::OnDictionaryChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_DICTIONARY,
            this->m_Context->Dictionaries.Items[Sel].Value);
        RefreshFromSnapshot();
    }

    void CompressPage::OnOrderChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_ORDER,
            this->m_Context->Orders.Items[Sel].Value);
        RefreshFromSnapshot();
    }

    void CompressPage::OnSolidChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_SOLID,
            this->m_Context->SolidBlocks.Items[Sel].Value);
        RefreshFromSnapshot();
    }

    void CompressPage::OnThreadsChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_THREADS,
            this->m_Context->Threads.Items[Sel].Value);
        RefreshFromSnapshot();
    }

    void CompressPage::OnMemoryChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_MEMORY,
            this->m_Context->MemoryLimits.Items[Sel].Value,
            this->m_Context->MemoryLimits.Items[Sel].SemanticText);
        RefreshFromSnapshot();
    }

    void CompressPage::OnUpdateModeChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_UPDATE_MODE,
            this->m_Context->UpdateModes.Items[Sel].Value);
        RefreshFromSnapshot();
    }

    void CompressPage::OnPathModeChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_PATH_MODE,
            this->m_Context->PathModes.Items[Sel].Value);
        RefreshFromSnapshot();
    }

    void CompressPage::OnEncryptionMethodChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        const int Sel = sender.as<winrt::Windows::UI::Xaml::Controls::ComboBox>().
            SelectedIndex();
        if (Sel < 0)
        {
            return;
        }
        SendCommand(
            K7_COMPRESS_COMMAND_ENCRYPTION_METHOD,
            this->m_Context->EncryptionMethods.Items[Sel].Value,
            this->m_Context->EncryptionMethods.Items[Sel].SemanticText);
        RefreshFromSnapshot();
    }

    void CompressPage::OnArchivePathChanged(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        std::wstring Path = ArchivePathCombo().Text().c_str();
        wcscpy_s(this->m_Context->ArchivePath, Path.c_str());
        SendCommand(
            K7_COMPRESS_COMMAND_ARCHIVE_PATH,
            0,
            this->m_Context->ArchivePath);
    }

    void CompressPage::OnParametersChanged(
        winrt::IInspectable const& sender,
        winrt::TextChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        std::wstring Text = ParametersBox().Text().c_str();
        wcscpy_s(this->m_Context->Parameters, Text.c_str());
        SendCommand(
            K7_COMPRESS_COMMAND_PARAMETERS,
            0,
            this->m_Context->Parameters);
    }

    void CompressPage::OnVolumeChanged(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        std::wstring Text = VolumeCombo().Text().c_str();
        wcscpy_s(this->m_Context->VolumeText, Text.c_str());
        SendCommand(
            K7_COMPRESS_COMMAND_VOLUME,
            0,
            this->m_Context->VolumeText);
    }

    void CompressPage::OnShowPasswordClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (!this->m_Context)
        {
            return;
        }
        this->m_Context->ShowPassword = GetBool(ShowPasswordCheck().IsChecked())
            ? TRUE : FALSE;
        UpdatePasswordControl();
    }

    void CompressPage::OnSfxClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        this->m_Context->SfxMode = GetBool(SfxCheck().IsChecked()) ? TRUE : FALSE;
        SendCommand(K7_COMPRESS_COMMAND_SFX, this->m_Context->SfxMode);
        RefreshFromSnapshot();
    }

    void CompressPage::OnSharedClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        this->m_Context->OpenShareForWrite =
            GetBool(SharedCheck().IsChecked()) ? TRUE : FALSE;
        SendCommand(K7_COMPRESS_COMMAND_SHARED, this->m_Context->OpenShareForWrite);
    }

    void CompressPage::OnDeleteClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        this->m_Context->DeleteAfterCompressing =
            GetBool(DeleteCheck().IsChecked()) ? TRUE : FALSE;
        SendCommand(K7_COMPRESS_COMMAND_DELETE, this->m_Context->DeleteAfterCompressing);
    }

    void CompressPage::OnEncryptHeadersClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (this->m_InitGuard || !this->m_Context)
        {
            return;
        }
        this->m_Context->EncryptHeaders =
            GetBool(EncryptHeadersCheck().IsChecked()) ? TRUE : FALSE;
        SendCommand(K7_COMPRESS_COMMAND_ENCRYPT_HEADERS,
            this->m_Context->EncryptHeaders);
    }

    void CompressPage::OnBrowseClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (!this->m_Context)
        {
            return;
        }
        SendCommand(K7_COMPRESS_COMMAND_BROWSE_ARCHIVE, 0, this->m_Context->ArchivePath);
        RefreshFromSnapshot();
    }

    void CompressPage::OnOptionsClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (!this->m_Context)
        {
            return;
        }
        if (this->m_Context->OptionsCallback)
        {
            this->m_Context->OptionsCallback(this->m_Context->CallbackContext);
        }
        RefreshFromSnapshot();
    }

    void CompressPage::OnOkClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (!this->m_Context)
        {
            PostClose(false);
            return;
        }

        // Capture the password fields into the context before submitting.
        {
            std::wstring Password = PasswordBox().Password().c_str();
            std::wstring Confirmation = ReenterPasswordBox().Password().c_str();
            wcscpy_s(this->m_Context->Password, Password.c_str());
            wcscpy_s(this->m_Context->PasswordConfirmation, Confirmation.c_str());
        }

        if (this->m_Context->CommandCallback)
        {
            const bool Accepted = this->m_Context->CommandCallback(
                this->m_Context->CallbackContext,
                K7_COMPRESS_COMMAND_SUBMIT,
                0,
                nullptr);
            if (!Accepted)
            {
                // The callback wrote ErrorText; surface it and stay open.
                RefreshFromSnapshot();
                return;
            }
        }

        PostClose(true);
    }

    void CompressPage::OnCancelClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        PostClose(false);
    }
}
