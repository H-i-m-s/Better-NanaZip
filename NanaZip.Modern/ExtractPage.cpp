#include "pch.h"
#include "ExtractPage.h"
#if __has_include("ExtractPage.g.cpp")
#include "ExtractPage.g.cpp"
#endif

#include "NanaZip.Modern.h"

#include <shlobj.h>

#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.h>

#include <vector>
#include <algorithm>
#include <cwctype>

namespace winrt::NanaZip::Modern::implementation
{
    ExtractPage::ExtractPage(
        _In_opt_ HWND WindowHandle,
        _In_ PK7_EXTRACT_DIALOG_CONTEXT Context) :
        m_WindowHandle(WindowHandle),
        m_Context(Context),
        m_InitGuard(false),
        m_OkClicked(false)
    {
        this->Unloaded({ this, &ExtractPage::OnUnloaded });
        this->Loaded({ this, &ExtractPage::OnLoaded });
        this->SizeChanged({ this, &ExtractPage::OnSizeChanged });
    }

    void ExtractPage::InitializeComponent()
    {
        ExtractPageT::InitializeComponent();
    }

    winrt::hstring ExtractPage::Res(
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

    void ExtractPage::ApplyDialogFont(UINT32 Pt)
    {
        double FontSizePx = (Pt == 0) ? 0.0 : (double)Pt * 96.0 / 72.0;
        ApplyFontToTree(*this, FontSizePx);
    }

    void ExtractPage::ApplyFontToTree(
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

    bool ExtractPage::GetBoolsVal(
        BOOLEAN Def1, BOOLEAN Val1,
        BOOLEAN Def2, BOOLEAN Val2) const
    {
        if (Def1)
        {
            return Val1 != 0;
        }
        if (Def2)
        {
            return Val2 != 0;
        }
        return Val1 != 0;
    }

    void ExtractPage::SetBoolsResult(
        bool Value,
        BOOLEAN& Def1, BOOLEAN& Val1,
        BOOLEAN& Def2, BOOLEAN& Val2) const
    {
        const bool Old = GetBoolsVal(Def1, Val1, Def2, Val2);
        if (Value != Old)
        {
            Def1 = TRUE;
            Def2 = TRUE;
        }
        Val1 = Value ? TRUE : FALSE;
        Val2 = Value ? TRUE : FALSE;
    }

    static bool IsPathSeparator(wchar_t Ch)
    {
        return Ch == L'\\' || Ch == L'/';
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

    void ExtractPage::UpdatePasswordControl()
    {
        const bool Show =
            GetBool(ShowPasswordCheck().IsChecked());
        PasswordBox().PasswordRevealMode(Show
            ? winrt::Windows::UI::Xaml::Controls::PasswordRevealMode::Visible
            : winrt::Windows::UI::Xaml::Controls::PasswordRevealMode::Hidden);
    }

    static std::wstring NormalizeDirPathPrefix(std::wstring Path)
    {
        // Trim trailing spaces.
        size_t End = Path.find_last_not_of(L' ');
        if (End == std::wstring::npos)
        {
            Path.clear();
        }
        else
        {
            Path.erase(End + 1);
        }
        // NormalizeDirPathPrefix: ensure the path ends with a path separator.
        if (!Path.empty() && !IsPathSeparator(Path.back()))
        {
            Path += L'\\';
        }
        return Path;
    }

    static void SplitPathToPartsSmart(
        std::wstring const& Path,
        std::wstring& Prefix,
        std::wstring& Name)
    {
        std::wstring Path2 = Path;
        while (Path2.size() > 1 && IsPathSeparator(Path2.back()))
        {
            Path2.pop_back();
        }
        size_t Pos = Path2.find_last_of(L"\\/");
        if (Pos == std::wstring::npos)
        {
            Prefix.clear();
            Name = Path2;
        }
        else
        {
            Prefix = Path2.substr(0, Pos + 1);
            Name = Path2.substr(Pos + 1);
        }
    }

    static void TrimString(std::wstring& s)
    {
        size_t Start = s.find_first_not_of(L" \t\r\n");
        size_t End = s.find_last_not_of(L" \t\r\n");
        if (Start == std::wstring::npos)
        {
            s.clear();
        }
        else
        {
            s = s.substr(Start, End - Start + 1);
        }
    }

    void ExtractPage::OnLoaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        this->m_InitGuard = false;
    }

    winrt::Windows::Foundation::Size ExtractPage::PrepareForShow()
    {
        if (!this->m_Context)
        {
            return winrt::Windows::Foundation::Size(560, 460);
        }

        PK7_EXTRACT_DIALOG_CONTEXT Context = this->m_Context;

        // Dialog title: "Extract : <arc path>".
        {
            std::wstring Title = Res(3400, L"Extract").c_str();
            std::wstring ArcPath(Context->ArcPath);
            if (!ArcPath.empty())
            {
                Title += L" : ";
                Title += ArcPath;
            }
            ::SetWindowTextW(this->m_WindowHandle, Title.c_str());
        }

        ExtractToText().Text(Res(3401, L"E&xtract to:"));
        PathModeText().Text(Res(3410, L"Path mode:"));
        OverwriteModeText().Text(Res(3420, L"Overwrite mode:"));
        ElimDupCheck().Content(winrt::box_value(Res(3430, L"Eliminate duplication of root folder")));
        NtSecurityCheck().Content(winrt::box_value(Res(3431, L"Restore file security")));
        OpenFolderCheck().Content(winrt::box_value(Res(3433, L"&Open folder after extract")));
        DeleteAfterCheck().Content(winrt::box_value(Res(3435, L"Delete archive after extraction")));
        PasswordGroupText().Text(Res(3807, L"Password"));
        ShowPasswordCheck().Content(winrt::box_value(Res(3803, L"Show Password")));
        OkButton().Content(winrt::box_value(Res(401, L"OK")));
        CancelButton().Content(winrt::box_value(Res(402, L"Cancel")));

        // --- Path field: split the initial directory into prefix + name. ---
        std::wstring PathPrefix;
        std::wstring PathName;
        SplitPathToPartsSmart(Context->DirPath, PathPrefix, PathName);
        if (PathPrefix.empty())
        {
            PathPrefix = PathName;
        }
        else
        {
            NameBox().Text(winrt::hstring(PathName));
        }

        const bool SplitDest =
            GetBoolsVal(Context->SplitDestDef, Context->SplitDestVal,
                Context->SplitDestDef2, Context->SplitDestVal2);
        NameEnableCheck().IsChecked(BoxBool(SplitDest));
        NameBox().Visibility(SplitDest
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);

        PathCombo().Text(winrt::hstring(PathPrefix));

        for (UINT32 i = 0; i < Context->NumPaths && i < 16; i++)
        {
            if (Context->Paths[i][0])
            {
                PathCombo().Items().Append(winrt::box_value(
                    winrt::hstring(Context->Paths[i])));
            }
        }

        // --- Path mode combo ---
        UINT32 PathMode = Context->PathMode;
        if (!Context->PathMode_Force &&
            Context->PathModeDefault != 0xFFFFFFFF)
        {
            PathMode = Context->PathModeDefault;
        }
        {
            const wchar_t* Items[3] = {
                L"Full paths", L"No paths", L"Absolute paths" };
            const UINT32 IDs[3] = { 3411, 3412, 3413 };
            int CurSel = 0;
            for (int i = 0; i < 3; i++)
            {
                PathModeCombo().Items().Append(winrt::box_value(
                    Res(IDs[i], Items[i])));
                if ((UINT32)i == PathMode)
                {
                    CurSel = i;
                }
            }
            PathModeCombo().SelectedIndex(CurSel);
        }

        // --- Overwrite mode combo ---
        UINT32 OverwriteMode = Context->OverwriteMode;
        if (!Context->OverwriteMode_Force &&
            Context->OverwriteModeDefault != 0xFFFFFFFF)
        {
            OverwriteMode = Context->OverwriteModeDefault;
        }
        {
            const wchar_t* Items[5] = {
                L"Ask before overwrite", L"Overwrite without prompt",
                L"Skip existing files", L"Rename existing file",
                L"Auto rename existing file" };
            const UINT32 IDs[5] = { 3421, 3422, 3423, 3424, 3425 };
            int CurSel = 0;
            for (int i = 0; i < 5; i++)
            {
                OverwriteModeCombo().Items().Append(winrt::box_value(
                    Res(IDs[i], Items[i])));
                if ((UINT32)i == OverwriteMode)
                {
                    CurSel = i;
                }
            }
            OverwriteModeCombo().SelectedIndex(CurSel);
        }

        // --- Checkboxes ---
        ElimDupCheck().IsChecked(BoxBool(
            GetBoolsVal(Context->ElimDupDef, Context->ElimDupVal,
                Context->ElimDupDef2, Context->ElimDupVal2)));
        NtSecurityCheck().IsChecked(BoxBool(
            GetBoolsVal(Context->NtSecurityDef, Context->NtSecurityVal,
                Context->NtSecurityDef2, Context->NtSecurityVal2)));
        OpenFolderCheck().IsChecked(BoxBool(
            GetBoolsVal(Context->OpenFolderDef, Context->OpenFolderVal,
                Context->OpenFolderDef2, Context->OpenFolderVal2)));
        DeleteAfterCheck().IsChecked(BoxBool(
            Context->DeleteAfterExtract != FALSE));
        ShowPasswordCheck().IsChecked(BoxBool(
            GetBoolsVal(Context->ShowPasswordDef, Context->ShowPasswordVal,
                Context->ShowPasswordDef2, Context->ShowPasswordVal2)));

        // --- Password ---
        PasswordBox().Password(winrt::hstring(Context->Password));
        UpdatePasswordControl();

        ApplyDialogFont(Context->FontSizeDialog);

        // Measure the content with an infinite constraint so every control
        // reports its natural size (including the longest combo item and the
        // current dialog font size). The two columns are equal-width (1:1),
        // so the dialog width must fit the wider column twice, otherwise the
        // equal split would squeeze the wider column and clip its controls
        // (e.g. the path combo) or wrap its text. The caller sizes the window
        // from this before it is shown, so there is no visible resize after
        // the dialog appears. The returned size is the content size without
        // margin; the caller adds a comfortable margin for the default size
        // and uses a smaller value for the minimum track size.
        winrt::Windows::Foundation::Size Inf(
            100000.0f, 100000.0f);
        this->Measure(Inf);
        winrt::Windows::Foundation::Size Desired = this->DesiredSize();

        float LeftW = LeftColumnPanel().DesiredSize().Width;
        float RightW = RightColumnPanel().DesiredSize().Width;
        float Wide = LeftW > RightW ? LeftW : RightW;
        // Column gap (12+12) plus the page padding (12+12).
        Desired.Width = Wide * 2.0f + 48.0f;
        return Desired;
    }

    void ExtractPage::OnUnloaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        // The window can also be closed with the X button or Alt+F4, which
        // never passes through OnCancelClicked. Treat every close that was
        // not confirmed by OK as a cancel, otherwise the caller would start
        // extracting with a dialog the user never confirmed.
        if (this->m_Context && !this->m_OkClicked)
        {
            this->m_Context->OK = FALSE;
        }
    }

    void ExtractPage::OnSizeChanged(
        winrt::IInspectable const& sender,
        winrt::Windows::UI::Xaml::SizeChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        this->UpdateModeRowLayout();
    }

    void ExtractPage::UpdateModeRowLayout()
    {
        if (!this->m_Context)
        {
            return;
        }

        // Content width available inside the page padding (12 left + 12
        // right). The left column is half of it minus the column gap.
        const double PageW = this->ActualWidth();
        if (PageW <= 0.0)
        {
            return;
        }
        const double LeftColW = (PageW - 24.0 - 24.0) / 2.0;

        // The labels are indented so their text lines up with the checkbox
        // text ("eliminate duplication..."), not with the check box itself.
        const double Indent = 28.0;
        // Wrap with some slack so label + combo never look cramped.
        const double Slack = 24.0;

        const double PathNeed =
            Indent + PathModeText().ActualWidth() + 6.0 + 140.0 + Slack;
        const double OverNeed =
            Indent + OverwriteModeText().ActualWidth() + 6.0 + 160.0 + Slack;

        const bool PathWrap = PathNeed > LeftColW;
        const bool OverWrap = OverNeed > LeftColW;

        // Same row: label left, combo right. Wrapped: the combo sits below
        // the label, indented to the label's text start, with vertical
        // separation so it does not touch the label.
        auto SetRowLayout =
            [](
                winrt::Windows::UI::Xaml::Controls::ComboBox const& Combo,
                bool Wrap)
        {
            winrt::Windows::UI::Xaml::Controls::Grid::SetRow(
                Combo, Wrap ? 1 : 0);
            winrt::Windows::UI::Xaml::Controls::Grid::SetColumn(
                Combo, Wrap ? 0 : 1);
            winrt::Windows::UI::Xaml::Controls::Grid::SetColumnSpan(
                Combo, Wrap ? 2 : 1);
            Combo.Margin(Wrap
                ? winrt::Windows::UI::Xaml::Thickness(28.0, 8.0, 0.0, 0.0)
                : winrt::Windows::UI::Xaml::Thickness(6.0, 0.0, 0.0, 0.0));
        };

        SetRowLayout(PathModeCombo(), PathWrap);
        SetRowLayout(OverwriteModeCombo(), OverWrap);

        // The wrap state changed what the layout needs; refresh the minimum
        // track size so compressing further never clips the content.
        this->RecalcMinTrack();
    }

    void ExtractPage::RecalcMinTrack()
    {
        if (!this->m_Context)
        {
            return;
        }

        const double PageW = this->ActualWidth();
        if (PageW <= 0.0)
        {
            return;
        }

        // Measure with the current width so DesiredSize reflects the current
        // wrap state; that is the minimum the content needs right now.
        winrt::Windows::Foundation::Size Constraint(
            (float)PageW, 100000.0f);
        this->Measure(Constraint);
        winrt::Windows::Foundation::Size Desired = this->DesiredSize();

        const UINT Dpi = ::GetDpiForWindow(this->m_WindowHandle);
        const float Scale = (float)Dpi / (float)USER_DEFAULT_SCREEN_DPI;

        int MinClientW = (int)((Desired.Width + 32.0f) * Scale + 0.5f);
        int MinClientH = (int)(Desired.Height * Scale + 0.5f);
        if (MinClientW < 480) MinClientW = 480;
        if (MinClientH < 400) MinClientH = 400;

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
    }

    static int CALLBACK BrowseCallbackProc(
        HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
    {
        UNREFERENCED_PARAMETER(lParam);
        if (uMsg == BFFM_INITIALIZED && lpData)
        {
            ::SendMessageW(hwnd, BFFM_SETSELECTION, TRUE, lpData);
        }
        return 0;
    }

    void ExtractPage::OnBrowseClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        std::wstring Current =
            PathCombo().Text().c_str();

        BROWSEINFOW Bi = {};
        Bi.hwndOwner = this->m_WindowHandle;
        std::wstring Title = Res(3402, L"Extract to folder").c_str();
        Bi.lpszTitle = Title.c_str();
        Bi.ulFlags = BIF_NEWDIALOGSTYLE | BIF_RETURNONLYFSDIRS;
        Bi.lpfn = BrowseCallbackProc;
        Bi.lParam = reinterpret_cast<LPARAM>(Current.c_str());

        LPITEMIDLIST Pidl = ::SHBrowseForFolderW(&Bi);
        if (Pidl)
        {
            wchar_t Path[MAX_PATH];
            if (::SHGetPathFromIDListW(Pidl, Path))
            {
                PathCombo().Text(winrt::hstring(Path));
            }
            ::CoTaskMemFree(Pidl);
        }
    }

    void ExtractPage::OnNameEnableClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        const bool Enabled =
            GetBool(NameEnableCheck().IsChecked());
        NameBox().Visibility(Enabled
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
    }

    void ExtractPage::OnShowPasswordClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        UpdatePasswordControl();
    }

    void ExtractPage::OnElimDupClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
    }

    void ExtractPage::OnNtSecurityClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
    }

    void ExtractPage::OnOpenFolderClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
    }

    void ExtractPage::OnDeleteAfterClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
    }

    void ExtractPage::OnCancelClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        if (this->m_Context)
        {
            this->m_Context->OK = FALSE;
        }
        ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }

    void ExtractPage::OnOkClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        if (!this->m_Context)
        {
            ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
            return;
        }

        PK7_EXTRACT_DIALOG_CONTEXT Context = this->m_Context;

        // Path mode (0/1/2).
        {
            int Sel = PathModeCombo().SelectedIndex();
            Context->PathMode = (Sel >= 0 && Sel <= 2) ? (UINT32)Sel : 0;
        }

        // Overwrite mode (0..4).
        {
            int Sel = OverwriteModeCombo().SelectedIndex();
            Context->OverwriteMode = (Sel >= 0 && Sel <= 4) ? (UINT32)Sel : 0;
        }

        // Password.
        {
            std::wstring Password = PasswordBox().Password().c_str();
            wcscpy_s(Context->Password, Password.c_str());
        }

        // Checkbox pairs.
        SetBoolsResult(
            GetBool(ElimDupCheck().IsChecked()),
            Context->ElimDupDef, Context->ElimDupVal,
            Context->ElimDupDef2, Context->ElimDupVal2);
        SetBoolsResult(
            GetBool(NtSecurityCheck().IsChecked()),
            Context->NtSecurityDef, Context->NtSecurityVal,
            Context->NtSecurityDef2, Context->NtSecurityVal2);
        SetBoolsResult(
            GetBool(OpenFolderCheck().IsChecked()),
            Context->OpenFolderDef, Context->OpenFolderVal,
            Context->OpenFolderDef2, Context->OpenFolderVal2);
        SetBoolsResult(
            GetBool(ShowPasswordCheck().IsChecked()),
            Context->ShowPasswordDef, Context->ShowPasswordVal,
            Context->ShowPasswordDef2, Context->ShowPasswordVal2);
        Context->DeleteAfterExtract =
            GetBool(DeleteAfterCheck().IsChecked()) ? TRUE : FALSE;

        const bool SplitDest =
            GetBool(NameEnableCheck().IsChecked());
        SetBoolsResult(
            SplitDest,
            Context->SplitDestDef, Context->SplitDestVal,
            Context->SplitDestDef2, Context->SplitDestVal2);
        Context->SplitDestEnable = SplitDest ? TRUE : FALSE;

        // Path: the combo text (without sub path) goes into history.
        std::wstring PathText = PathCombo().Text().c_str();
        TrimString(PathText);
        std::wstring PathNoSub = NormalizeDirPathPrefix(PathText);

        // History: current path first, unique, max 16 entries.
        {
            struct Entry
            {
                std::wstring Value;
            };
            std::vector<Entry> History;
            auto AddUnique = [&History](std::wstring const& s)
            {
                std::wstring T = s;
                TrimString(T);
                if (T.empty())
                {
                    return;
                }
                for (auto const& Item : History)
                {
                    std::wstring A = Item.Value;
                    std::wstring B = T;
                    std::transform(A.begin(), A.end(), A.begin(), ::towlower);
                    std::transform(B.begin(), B.end(), B.begin(), ::towlower);
                    if (A == B)
                    {
                        return;
                    }
                }
                History.push_back(Entry{ T });
            };

            AddUnique(PathText);
            for (UINT32 i = 0; i < Context->NumPaths && i < 16; i++)
            {
                AddUnique(Context->Paths[i]);
            }

            UINT32 Num = 0;
            for (auto const& Item : History)
            {
                if (Num >= 16)
                {
                    break;
                }
                wcscpy_s(Context->Paths[Num], Item.Value.c_str());
                Num++;
            }
            Context->NumPaths = Num;
        }

        // Final directory: prefix + sub path when enabled.
        std::wstring FinalDir = PathNoSub;
        if (SplitDest)
        {
            std::wstring SubPath = NameBox().Text().c_str();
            TrimString(SubPath);
            if (!SubPath.empty())
            {
                FinalDir += SubPath;
                FinalDir = NormalizeDirPathPrefix(FinalDir);
            }
        }
        wcscpy_s(Context->OutDirPath, FinalDir.c_str());

        {
            std::wstring SubPath = NameBox().Text().c_str();
            wcscpy_s(Context->OutPathName, SubPath.c_str());
        }

        Context->OK = TRUE;
        this->m_OkClicked = true;
        ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }
}
