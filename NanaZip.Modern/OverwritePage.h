#pragma once

#include "OverwritePage.g.h"

#include <Windows.h>

#include "NanaZip.Modern.h"

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct OverwritePage : OverwritePageT<OverwritePage>
    {
    public:

        OverwritePage(
            _In_opt_ HWND WindowHandle = nullptr,
            _In_ PK7_OVERWRITE_DIALOG_CONTEXT Context = nullptr);

        void InitializeComponent();

        void OnUnloaded(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnLoaded(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnYesClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnYesToAllClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnAutoRenameClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnNoClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnNoToAllClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnCancelClicked(
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

        winrt::hstring BuildFileInfoText(
            BOOLEAN SizeDefined,
            BOOLEAN TimeDefined,
            UINT64 Size,
            FILETIME const& Time,
            LPCWSTR Name);

        void LoadFileIcon(
            winrt::Windows::UI::Xaml::Controls::Image const& Target,
            LPCWSTR Name);

        void SetResult(UINT32 Result);

        HWND m_WindowHandle;
        PK7_OVERWRITE_DIALOG_CONTEXT m_Context;
    };
}
