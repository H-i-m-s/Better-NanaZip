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

        void StartAutomaticPasswordQuery();

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

        // Adds the current password to the local password book ("+" button
        // next to the password box). On success the glyph becomes a check
        // mark on a translucent light-green background for a moment and
        // then fades back to '+'. Fails silently when the host has no
        // password book or the box is empty.
        void OnAddPasswordClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        // Called by the window subclass (UI thread) when the async local
        // password match finishes. Status is one of the
        // K7_PASSWORD_MATCH_STATUS_* values; Password is the accepted
        // candidate for MATCHED, otherwise empty; Source records where the
        // accepted password came from (K7_PASSWORD_QUERY_SOURCE_*). Results
        // whose RequestId does not match the page's outstanding request are
        // ignored.
        void SetPasswordFromMatch(
            UINT64 RequestId,
            INT Status,
            LPCWSTR Password,
            UINT32 Source);

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

        bool TryCloudPassword(bool automatic);
        bool StartLocalPasswordMatch(bool automatic);

        HWND m_WindowHandle;
        PK7_PASSWORD_DIALOG_CONTEXT m_Context;
        bool m_OkClicked;
        bool m_ProgrammaticPasswordChange;
        // True while at least one async lookup (local match or cloud query)
        // is running; the local button switches to the cancelling state
        // while it is set.
        bool m_PasswordMatchRunning;
        // Request ids of the outstanding lookups, one per source. Zero means
        // no task of that source is in flight. Results carrying any other id
        // belong to an older dialog/task and are ignored. Local-first and
        // cloud-first run one task at a time; mixed mode runs both.
        UINT64 m_LocalMatchRequestId;
        UINT64 m_CloudQueryRequestId;
        bool m_AutoQueryStarted;
        bool m_AutoQueryActive;
        // One-shot timer that restores the "+" glyph after a successful
        // add-to-password-book click.
        winrt::Windows::UI::Xaml::DispatcherTimer m_AddPasswordTimer{ nullptr };
        // The translucent light-green background shown while the check mark
        // is displayed; animated back to transparent when the timer fires.
        winrt::Windows::UI::Xaml::Media::SolidColorBrush m_AddPasswordBrush{ nullptr };

        // Recomputes m_PasswordMatchRunning from the two outstanding ids.
        void UpdatePasswordMatchRunning();
        // Fills the password box and records the source for a matched
        // result, ending the automatic chain.
        void FillPassword(LPCWSTR Password, UINT32 Source);
        // Restores the "+" glyph after a successful add: fades the button
        // back to its normal look and clears the green background.
        void RestoreAddPasswordButton();
    };
}
