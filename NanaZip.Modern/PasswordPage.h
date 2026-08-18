#pragma once

#include "PasswordPage.g.h"

#include <Windows.h>

#include <string>

#include "NanaZip.Modern.h"

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct PasswordPage : PasswordPageT<PasswordPage>
    {
    public:

        PasswordPage(
            _In_opt_ HWND WindowHandle = nullptr,
            _In_ PK7_PASSWORD_DIALOG_CONTEXT Context = nullptr);

        void InitializeComponent();

        // Applies the dialog font and measures the content so the caller
        // can size the window before it is shown (avoids a visible resize
        // after the dialog appears). Returns the desired client size in DIPs.
        winrt::Windows::Foundation::Size PrepareForShow();

        void OnLoaded(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnUnloaded(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnShowPasswordClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnCloudPasswordClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnLocalPasswordClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnSharePasswordClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        // Called by the window subclass (UI thread) when the async local
        // password match finishes. Status is one of the
        // K7_PASSWORD_MATCH_STATUS_* values; Password is the accepted
        // candidate for MATCHED, otherwise empty.
        void SetPasswordFromMatch(
            INT Status,
            LPCWSTR Password);

        void OnPasswordKeyDown(
            winrt::IInspectable const& sender,
            winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        void OnPasswordChanged(
            winrt::IInspectable const& sender,
            winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        void OnOkClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnCancelClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnPageKeyDown(
            winrt::IInspectable const& sender,
            winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

    private:

        winrt::hstring Res(
            UINT32 ResourceId,
            LPCWSTR Fallback);

        winrt::hstring RemoveMnemonic(
            winrt::hstring const& Text);

        void ApplyDialogFont(UINT32 Pt);

        void ApplyFontToTree(
            winrt::Windows::UI::Xaml::DependencyObject const& Node,
            double FontSizePx);

        void UpdatePasswordControl();

        HWND m_WindowHandle;
        PK7_PASSWORD_DIALOG_CONTEXT m_Context;
        bool m_OkClicked;
        bool m_ProgrammaticPasswordChange;
        // True while the async local password match is running; the button
        // switches to the cancelling state while it is set.
        bool m_PasswordMatchRunning;
    };
}
