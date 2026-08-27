#pragma once

#include "PropertiesPage.g.h"
#include "PropertyDetailItem.g.h"
#include "SignatureItem.g.h"
#include "SecurityItem.g.h"
#include "VersionItem.g.h"
#include "CustomPropertyItem.g.h"
#include "NanaZip.Modern.h"

#include <Windows.h>

#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct PropertiesPage : PropertiesPageT<PropertiesPage>
    {
        PropertiesPage(
            _In_ HWND WindowHandle,
            _In_ PK7_FILE_PROPERTIES_DIALOG_CONTEXT Context,
            _In_ UINT32 FontSizeDialog = 0);

        void InitializeComponent();

        // Fills all tabs, applies the dialog font, and writes MinTrackW /
        // MinTrackH so the host can size the window before it is shown.
        // Returns a default client size in DIPs.
        winrt::Windows::Foundation::Size PrepareForShow();

        // Tab bar
        void TabButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        // Details search box
        void OnDetailsSearchChanged(
            winrt::IInspectable const& sender,
            winrt::Windows::UI::Xaml::Controls::TextChangedEventArgs const& e);

        // General attribute check boxes
        void OnReadOnlyCheckClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);
        void OnHiddenCheckClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        // Security advanced button
        void OnSecurityAdvancedClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        // Previous versions
        void OnVersionSelectionChanged(
            winrt::IInspectable const& sender,
            winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
        void OnOpenVersionClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);
        void OnCopyVersionClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        // Custom properties
        void OnCustomSelectionChanged(
            winrt::IInspectable const& sender,
            winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
        void OnAddCustomClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);
        void OnEditCustomClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);
        void OnDeleteCustomClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);
        void OnCustomEditOkClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);
        void OnCustomEditCancelClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        // Bottom buttons
        void OnOkClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);
        void OnCancelClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);
        void OnApplyClicked(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OnPageKeyDown(
            winrt::IInspectable const& sender,
            winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        void OnPageSizeChanged(
            winrt::IInspectable const& sender,
            winrt::Windows::UI::Xaml::SizeChangedEventArgs const& e);

        // Physical-pixel minimum outer size, written by PrepareForShow and
        // read by the host window subclass.
        LONG MinTrackW;
        LONG MinTrackH;

    private:

        void CollectData();

        void CollectGeneralAndDetails();
        void CollectSecurity();
        void CollectCustom();

        winrt::Windows::Foundation::IAsyncAction OnSignatureTabEntered();
        winrt::Windows::Foundation::IAsyncAction OnVersionsTabEntered();

        // Called back on the UI thread from the worker queries.
        void OnSignatureLoaded(
            winrt::hstring const& StatusText,
            std::vector<std::tuple<
                winrt::hstring,
                winrt::hstring,
                winrt::hstring>> const& Rows);
        void OnVersionsLoaded(
            std::vector<std::pair<
                winrt::hstring,
                std::wstring>> const& Results);

        void SwitchTab(int Index);
        void ApplyDialogFont(UINT32 Pt);
        void ApplyFontToTree(
            winrt::Windows::UI::Xaml::DependencyObject const& Node,
            double FontSizePx);
        winrt::hstring Res(
            UINT32 ResourceId,
            LPCWSTR Fallback);
        winrt::hstring FormatTime(FILETIME const& Time);
        winrt::hstring FormatSize(UINT64 Size);
        winrt::hstring GetHeaderName();
        winrt::hstring GetHeaderType();
        void LoadHeaderIcon(std::wstring const& Path);
        void RefreshDetailsFilter();
        void MarkAttributeDirty();
        void RefreshCustomButtons();
        void LogLayoutSnapshot(LPCWSTR Reason);

        HWND m_WindowHandle;
        std::vector<std::wstring> m_Paths;
        UINT32 m_FontSizeDialog;
        bool m_SingleSelection;
        int m_CurrentTab;

        // General tab data
        winrt::hstring m_GeneralType;
        winrt::hstring m_GeneralLocation;
        winrt::hstring m_GeneralSize;
        winrt::hstring m_GeneralAllocSize;
        winrt::hstring m_GeneralCreated;
        winrt::hstring m_GeneralModified;
        winrt::hstring m_GeneralAccessed;
        winrt::hstring m_DisplayName;
        UINT64 m_GeneralSizeBytes;
        UINT32 m_FileAttributes;
        bool m_AttributeDirty;

        // Details tab data
        winrt::Windows::Foundation::Collections::IVector<
            winrt::Windows::Foundation::IInspectable> m_Details;
        winrt::Windows::Foundation::Collections::IVector<
            winrt::Windows::Foundation::IInspectable> m_DetailsFiltered;

        // Signature tab data
        winrt::Windows::Foundation::Collections::IVector<
            winrt::Windows::Foundation::IInspectable> m_Signatures;
        winrt::hstring m_SignatureStatusText;
        bool m_SignatureLoaded;
        winrt::Windows::Foundation::IAsyncAction m_SignatureTask;

        // Security tab data
        winrt::Windows::Foundation::Collections::IVector<
            winrt::Windows::Foundation::IInspectable> m_SecurityItems;
        winrt::hstring m_SecurityOwnerText;
        std::vector<BYTE> m_SecurityDescriptorBuffer;

        // Previous versions tab data
        winrt::Windows::Foundation::Collections::IVector<
            winrt::Windows::Foundation::IInspectable> m_VersionItems;
        std::vector<std::wstring> m_VersionPaths;
        bool m_VersionsLoaded;
        winrt::Windows::Foundation::IAsyncAction m_VersionsTask;

        // Custom tab data
        winrt::Windows::Foundation::Collections::IVector<
            winrt::Windows::Foundation::IInspectable> m_CustomItems;
        std::vector<PROPERTYKEY> m_CustomKeys;
        std::vector<std::wstring> m_CustomNames;
        std::vector<PROPERTYKEY> m_AllCustomKeys;
        bool m_CustomDirty;
        int m_CustomEditMode; // 0 = none, 1 = add, 2 = edit
        int m_CustomEditIndex;
    };
}
