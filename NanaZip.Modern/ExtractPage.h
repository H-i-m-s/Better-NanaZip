#pragma once

#include "ExtractPage.g.h"

#include <Windows.h>

#include "NanaZip.Modern.h"

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct ExtractPage : ExtractPageT<ExtractPage>
    {
    public:

        ExtractPage(
            _In_opt_ HWND WindowHandle = nullptr,
            _In_ PK7_EXTRACT_DIALOG_CONTEXT Context = nullptr);

        void InitializeComponent();

        // Applies the dialog font and measures the content so the caller
        // can size the window before it is shown (avoids a visible resize
        // after the dialog appears). Returns the desired client size in DIPs.
        winrt::Windows::Foundation::Size PrepareForShow();

        void OnUnloaded(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnLoaded(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void StartAutomaticPasswordQuery();

        // Called by the window subclass (UI thread) when the async
        // encrypted-content pre-check finishes (only the extract dialog
        // starts it). Automatic lookup is started only when the archive
        // has encrypted items.
        void SetEncryptionCheckResult(BOOLEAN HasEncryptedItems);

        void OnBrowseClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnNameEnableClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnShowPasswordClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnPasswordChanged(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnElimDupClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnNtSecurityClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnOpenFolderClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnDeleteAfterClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        // Placeholders: cloud / local password lookup is implemented later.
        void OnCloudPasswordClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnLocalPasswordClicked(
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

        void OnSharePasswordClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        // Called by the window subclass (UI thread) when an async password
        // query (local match or cloud lookup) finishes. Status is one of the
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

        void OnOkClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnCancelClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnSizeChanged(
            winrt::IInspectable const& sender,
            winrt::Windows::UI::Xaml::SizeChangedEventArgs const& e);

        void OnPageKeyDown(
            winrt::IInspectable const& sender,
            winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        // Moves the path mode / overwrite mode combos to the next row when
        // the dialog is too narrow to fit label + combo on one line.
        void UpdateModeRowLayout();

        // Recomputes the minimum track size from the current layout (the
        // wrap state may change what the content needs) and writes it back
        // to the context so the window subclass enforces it.
        void RecalcMinTrack();

        // Keeps the path text from being blanked out by the editable combo
        // when its drop-down is opened/closed.
        void OnPathComboDropDownOpened(
            winrt::IInspectable const& sender,
            winrt::IInspectable const& e);

        void OnPathComboDropDownClosed(
            winrt::IInspectable const& sender,
            winrt::IInspectable const& e);

        // Removes a history entry via the "x" in the path drop-down.
        void OnDeleteHistoryPathClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

    private:

        winrt::hstring Res(
            UINT32 ResourceId,
            LPCWSTR Fallback);

        void ApplyDialogFont(UINT32 Pt);

        void ApplyFontToTree(
            winrt::Windows::UI::Xaml::DependencyObject const& Node,
            double FontSizePx);

        void UpdatePasswordControl();

        bool TryCloudPassword(bool automatic);
        bool StartLocalPasswordMatch(bool automatic);
        void StartEncryptionCheck();

        bool GetBoolsVal(
            BOOLEAN Def1, BOOLEAN Val1,
            BOOLEAN Def2, BOOLEAN Val2) const;

        void SetBoolsResult(
            bool Value,
            BOOLEAN& Def1, BOOLEAN& Val1,
            BOOLEAN& Def2, BOOLEAN& Val2) const;

        HWND m_WindowHandle;
        PK7_EXTRACT_DIALOG_CONTEXT m_Context;
        bool m_InitGuard;
        bool m_OkClicked;
        bool m_FirstLayout;
        std::wstring m_PathTextSnapshot;
        // Page width (DIPs) below which the mode combos wrap below their
        // labels; computed from the wrapped layout in RecalcMinTrack.
        double m_WrapThresholdW;
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
