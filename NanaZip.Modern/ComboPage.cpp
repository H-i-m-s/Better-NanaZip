#include "pch.h"
#include "ComboPage.h"
#if __has_include("ComboPage.g.cpp")
#include "ComboPage.g.cpp"
#endif

#include "NanaZip.Modern.h"

#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.h>

#include <string>

namespace winrt::NanaZip::Modern::implementation
{
    ComboPage::ComboPage(
        _In_opt_ HWND WindowHandle,
        _In_ PK7_COMBO_DIALOG_CONTEXT Context) :
        m_WindowHandle(WindowHandle),
        m_Context(Context),
        m_OkClicked(false)
    {
        this->Loaded({ this, &ComboPage::OnLoaded });
        this->Unloaded({ this, &ComboPage::OnUnloaded });
    }

    void ComboPage::InitializeComponent()
    {
        ComboPageT::InitializeComponent();
    }

    winrt::hstring ComboPage::Res(
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

    winrt::hstring ComboPage::RemoveMnemonic(
        winrt::hstring const& Text)
    {
        std::wstring Result;
        std::wstring Source(Text.c_str());
        Result.reserve(Source.size());
        for (size_t i = 0; i < Source.size(); i++)
        {
            if (Source[i] == L'&')
            {
                if (i + 1 < Source.size() && Source[i + 1] == L'&')
                {
                    Result += L'&';
                    i++;
                }
                continue;
            }
            Result += Source[i];
        }
        return winrt::hstring(Result);
    }

    void ComboPage::ApplyDialogFont(UINT32 Pt)
    {
        double FontSizePx = (Pt == 0) ? 0.0 : (double)Pt * 96.0 / 72.0;
        ApplyFontToTree(*this, FontSizePx);
    }

    void ComboPage::ApplyFontToTree(
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

    winrt::Windows::Foundation::Size ComboPage::PrepareForShow()
    {
        if (!this->m_Context)
        {
            return winrt::Windows::Foundation::Size(360, 120);
        }

        PK7_COMBO_DIALOG_CONTEXT Context = this->m_Context;

        // Dialog title comes from the caller (it owns the language strings,
        // e.g. LangString in the 7-Zip side), like the Win32 CComboDialog.
        ::SetWindowTextW(this->m_WindowHandle, Context->Title);

        PromptText().Text(RemoveMnemonic(
            winrt::hstring(Context->Static)));
        OkButton().Content(winrt::box_value(RemoveMnemonic(Res(401, L"OK"))));
        CancelButton().Content(winrt::box_value(RemoveMnemonic(Res(402, L"Cancel"))));

        // Initial value and history items. The editable combo blanks its text
        // when the drop-down opens if the text does not match any item, so
        // the current value is also kept as the first item (same as the
        // extract and split dialogs).
        ValueCombo().Text(winrt::hstring(Context->Value));
        if (Context->Value[0])
        {
            ValueCombo().Items().Append(winrt::box_value(
                winrt::hstring(Context->Value)));
        }
        for (UINT32 i = 0; i < Context->HistoryCount; i++)
        {
            ValueCombo().Items().Append(winrt::box_value(
                winrt::hstring(Context->History[i])));
        }

        // Guard against the editable combo blanking the text when its
        // drop-down is opened/closed (same as the extract and split dialogs).
        ValueCombo().DropDownOpened(
            { this, &ComboPage::OnComboDropDownOpened });
        ValueCombo().DropDownClosed(
            { this, &ComboPage::OnComboDropDownClosed });

        ApplyDialogFont(Context->FontSizeDialog);

        // Measure the content with an infinite constraint so every control
        // reports its natural size (with the current dialog font size). The
        // caller sizes the window from this before it is shown, so there is
        // no visible resize after the dialog appears.
        winrt::Windows::Foundation::Size Inf(
            100000.0f, 100000.0f);
        this->Measure(Inf);
        winrt::Windows::Foundation::Size Desired = this->DesiredSize();

        // Minimum window size: the measured content (plus a small margin), so
        // dragging the border can never clip the controls.
        const UINT Dpi = ::GetDpiForWindow(this->m_WindowHandle);
        const float Scale = (float)Dpi / (float)USER_DEFAULT_SCREEN_DPI;

        int MinClientW = (int)((Desired.Width + 32.0f) * Scale + 0.5f);
        int MinClientH = (int)((Desired.Height + 4.0f) * Scale + 0.5f);
        if (MinClientW < 300) MinClientW = 300;
        if (MinClientH < 120) MinClientH = 120;

        RECT rc = { 0, 0, MinClientW, MinClientH };
        {
            const LONG_PTR Style = ::GetWindowLongPtrW(
                this->m_WindowHandle, GWL_STYLE);
            const LONG_PTR ExStyle = ::GetWindowLongPtrW(
                this->m_WindowHandle, GWL_EXSTYLE);
            ::AdjustWindowRectEx(&rc, (DWORD)Style, FALSE, (DWORD)ExStyle);
        }

        Context->MinTrackW = rc.right - rc.left;
        Context->MinTrackH = rc.bottom - rc.top;

        return Desired;
    }

    void ComboPage::OnLoaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        // Focus the combo like the Win32 dialog (first control).
        ValueCombo().Focus(
            winrt::Windows::UI::Xaml::FocusState::Programmatic);
    }

    void ComboPage::OnUnloaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        // The window can also be closed with the X button or Alt+F4, which
        // never passes through OnCancelClicked. Treat every close that was
        // not confirmed by OK as a cancel, otherwise the caller would use a
        // value the user never confirmed.
        if (this->m_Context && !this->m_OkClicked)
        {
            this->m_Context->OK = FALSE;
        }
    }

    void ComboPage::OnComboDropDownOpened(
        winrt::IInspectable const& sender,
        winrt::IInspectable const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        this->m_TextSnapshot = ValueCombo().Text().c_str();
    }

    void ComboPage::OnComboDropDownClosed(
        winrt::IInspectable const& sender,
        winrt::IInspectable const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        if (!this->m_TextSnapshot.empty() &&
            ValueCombo().Text().empty())
        {
            ValueCombo().Text(
                winrt::hstring(this->m_TextSnapshot));
        }
        this->m_TextSnapshot.clear();
    }

    void ComboPage::OnComboKeyDown(
        winrt::IInspectable const& sender,
        winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        if (e.Key() == winrt::Windows::System::VirtualKey::Enter)
        {
            // Enter confirms like the default button of the Win32 dialog.
            e.Handled(true);
            OnOkClicked(nullptr, nullptr);
        }
    }

    void ComboPage::OnPageKeyDown(
        winrt::IInspectable const& sender,
        winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        if (e.Key() == winrt::Windows::System::VirtualKey::Escape)
        {
            // Esc closes the dialog like the X button (a cancel).
            e.Handled(true);
            if (this->m_Context && !this->m_OkClicked)
            {
                this->m_Context->OK = FALSE;
            }
            ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
        }
    }

    void ComboPage::OnCancelClicked(
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

    void ComboPage::OnOkClicked(
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

        PK7_COMBO_DIALOG_CONTEXT Context = this->m_Context;

        // Read the typed value. Fixed-size ABI buffer: truncate before
        // writing so an over-long value can never trigger wcscpy_s failure
        // (which would clear the buffer silently).
        std::wstring Value = ValueCombo().Text().c_str();
        if (Value.size() >= K7_COMBO_MAX_TEXT_LENGTH)
        {
            Value.resize(K7_COMBO_MAX_TEXT_LENGTH - 1);
        }
        wcscpy_s(Context->Value, Value.c_str());

        this->m_OkClicked = true;
        Context->OK = TRUE;
        ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }
}
