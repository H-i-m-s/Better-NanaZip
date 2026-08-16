#pragma once

#include "BenchmarkPage.g.h"

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
    struct BenchmarkPage : BenchmarkPageT<BenchmarkPage>
    {
    public:

        BenchmarkPage(
            _In_opt_ HWND WindowHandle = nullptr,
            _In_ PK7_BENCHMARK_DIALOG_CONTEXT Context = nullptr);

        void InitializeComponent();

        // Applies the dialog font and measures the content so the caller
        // can size the window before it is shown (avoids a visible resize
        // after the dialog appears). Returns the desired client size in DIPs.
        winrt::Windows::Foundation::Size PrepareForShow();

        // Applies one status refresh from the 7-Zip side.
        void ApplyStatus(
            _In_ PK7_BENCHMARK_STATUS Status);

        void OnLoaded(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnDictSelectionChanged(
            winrt::IInspectable const& sender,
            winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& e);

        void OnThreadsSelectionChanged(
            winrt::IInspectable const& sender,
            winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& e);

        void OnPassesSelectionChanged(
            winrt::IInspectable const& sender,
            winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& e);

        void OnStopClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnRestartClicked(
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

        void PostCommand(UINT32 CommandId);

        HWND m_WindowHandle;
        PK7_BENCHMARK_DIALOG_CONTEXT m_Context;
    };
}
