/*
 * PROJECT:    NanaZip.Modern
 * FILE:       NanaZip.Modern.cpp
 * PURPOSE:    Implementation for NanaZip Modern Experience
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "pch.h"

#include "NanaZip.Modern.h"

#include <Mile.Helpers.h>
#include <Mile.Xaml.h>

#include "App.h"
#include "SponsorPage.h"
#include "AboutPage.h"
#include "ExtractPage.h"
#include "OverwritePage.h"
#include "InformationPage.h"
#include "ProgressPage.h"
#include "CopyLocationPage.h"
#include "SettingsPage.h"

#pragma comment(lib, "comctl32.lib")

#include <winrt/Windows.ApplicationModel.Resources.Core.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>

#include <mutex>
#include <map>

namespace winrt
{
    using Windows::ApplicationModel::Resources::Core::ResourceManager;
    using Windows::ApplicationModel::Resources::Core::ResourceMap;
}

namespace
{
    static winrt::ResourceMap GetMainResourceMap()
    {
        static winrt::ResourceMap CachedResult = ([]() -> winrt::ResourceMap
        {
            try
            {
                return winrt::ResourceManager::Current().MainResourceMap();
            }
            catch (...)
            {
                // Do nothing.
            }
            return nullptr;
        }());

        return CachedResult;
    }

    static std::mutex g_CachedLanguageStringResourcesMutex;
    static std::map<UINT32, winrt::hstring> g_CachedLanguageStringResources;
}

EXTERN_C LPCWSTR WINAPI K7ModernGetLegacyStringResource(
    _In_ UINT32 ResourceId)
{
    {
        std::lock_guard Lock(g_CachedLanguageStringResourcesMutex);
        auto Iterator = g_CachedLanguageStringResources.find(ResourceId);
        if (g_CachedLanguageStringResources.end() != Iterator)
        {
            return Iterator->second.c_str();
        }
    }

    static winrt::ResourceMap LegacyResourceMap = ([]() -> winrt::ResourceMap
    {
        winrt::ResourceMap MainResourceMap = ::GetMainResourceMap();
        if (MainResourceMap)
        {
            return MainResourceMap.GetSubtree(L"Legacy");
        }
        return nullptr;
    }());
    if (!LegacyResourceMap)
    {
        return nullptr;
    }

    winrt::hstring ResourceName = L"Resource" + winrt::to_hstring(ResourceId);
    if (!LegacyResourceMap.HasKey(ResourceName))
    {
        return nullptr;
    }

    winrt::hstring Content = LegacyResourceMap.Lookup(
        ResourceName).Candidates().GetAt(0).ValueAsString();
    std::lock_guard Lock(g_CachedLanguageStringResourcesMutex);
    auto Iterator = g_CachedLanguageStringResources.emplace(
        ResourceId,
        std::move(Content));
    return Iterator.first->second.c_str();
}

namespace
{
    static winrt::NanaZip::Modern::App g_AppInstance = nullptr;
}

EXTERN_C BOOL WINAPI K7ModernAvailable()
{
    return nullptr != g_AppInstance;
}

EXTERN_C HRESULT WINAPI K7ModernInitialize()
{
    if (g_AppInstance)
    {
        return S_OK;
    }
    if (!::GetMainResourceMap())
    {
        // NanaZip.Modern requires resources.pri to get XAML resources.
        return E_NOINTERFACE;
    }
    try
    {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        using Implementation = winrt::NanaZip::Modern::implementation::App;
        g_AppInstance = winrt::make<Implementation>();
    }
    catch (...)
    {
        return winrt::to_hresult();
    }
    return S_OK;
}

EXTERN_C HRESULT WINAPI K7ModernUninitialize()
{
    if (!g_AppInstance)
    {
        return S_OK;
    }
    try
    {
        g_AppInstance.Close();
        g_AppInstance = nullptr;
        winrt::uninit_apartment();
    }
    catch (...)
    {
        return winrt::to_hresult();
    }
    return S_OK;
}

namespace winrt
{
    using Windows::UI::Xaml::Hosting::DesktopWindowXamlSource;
}

namespace
{
    HWND K7ModernCreateXamlWindow(
        _In_opt_ HWND ParentWindowHandle,
        _In_ DWORD ExtendedWindowStyle,
        _In_ DWORD WindowStyle)
    {
        HWND WindowHandle = ::CreateWindowExW(
            ExtendedWindowStyle,
            L"Mile.Xaml.ContentWindow",
            nullptr,
            WindowStyle,
            CW_USEDEFAULT,
            0,
            CW_USEDEFAULT,
            0,
            ParentWindowHandle,
            nullptr,
            nullptr,
            nullptr);
        if (!WindowHandle)
        {
            return nullptr;
        }
        if (!::SetWindowSubclass(
            WindowHandle,
            [](
                _In_ HWND hWnd,
                _In_ UINT uMsg,
                _In_ WPARAM wParam,
                _In_ LPARAM lParam,
                _In_ UINT_PTR uIdSubclass,
                _In_ DWORD_PTR dwRefData) -> LRESULT
        {
            UNREFERENCED_PARAMETER(uIdSubclass);
            UNREFERENCED_PARAMETER(dwRefData);

            switch (uMsg)
            {
            case WM_CLOSE:
            {
                HWND ParentWindow = ::GetWindow(hWnd, GW_OWNER);
                if (ParentWindow)
                {
                    ::EnableWindow(ParentWindow, TRUE);
                }
                break;
            }
            default:
                break;
            }

            return ::DefSubclassProc(
                hWnd,
                uMsg,
                wParam,
                lParam);
        },
            0,
            0))
        {
            ::DestroyWindow(WindowHandle);
            return nullptr;
        }
        return WindowHandle;
    }

    HWND K7ModernCreateXamlDialog(
        _In_opt_ HWND ParentWindowHandle)
    {
        HWND WindowHandle = ::K7ModernCreateXamlWindow(
            ParentWindowHandle,
            WS_EX_STATICEDGE | WS_EX_DLGMODALFRAME,
            WS_CAPTION | WS_SYSMENU);

        MILE_WINDOW_SYSTEM_BACKDROP_TYPE SystemBackdropType =
            MILE_WINDOW_SYSTEM_BACKDROP_TYPE_AUTO;
        if (S_OK == ::MileGetWindowSystemBackdropTypeAttribute(
            WindowHandle,
            &SystemBackdropType))
        {
            if (MILE_WINDOW_SYSTEM_BACKDROP_TYPE_AUTO != SystemBackdropType &&
                MILE_WINDOW_SYSTEM_BACKDROP_TYPE_NONE != SystemBackdropType)
            {
                const COLORREF IgnoreAccentColor = static_cast<COLORREF>(-2);
                ::MileSetWindowCaptionColorAttribute(
                    WindowHandle,
                    IgnoreAccentColor);
            }
        }

        return WindowHandle;
    }

    int K7ModernShowXamlWindow(
        _In_opt_ HWND WindowHandle,
        _In_ int Width,
        _In_ int Height,
        _In_ HWND ParentWindowHandle,
        _In_opt_ const RECT *InitialRect = nullptr)
    {
        if (!WindowHandle)
        {
            return -1;
        }

        UINT DpiValue = ::GetDpiForWindow(WindowHandle);

        int ScaledWidth = ::MulDiv(Width, DpiValue, USER_DEFAULT_SCREEN_DPI);
        int ScaledHeight = ::MulDiv(Height, DpiValue, USER_DEFAULT_SCREEN_DPI);

        if (InitialRect &&
            InitialRect->right > InitialRect->left &&
            InitialRect->bottom > InitialRect->top)
        {
            // The caller provided a remembered position and size; it is
            // already in physical pixels, so no DPI scaling is applied.
            ::SetWindowPos(
                WindowHandle,
                nullptr,
                InitialRect->left,
                InitialRect->top,
                InitialRect->right - InitialRect->left,
                InitialRect->bottom - InitialRect->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        else
        {
            RECT ParentRect = {};
            if (ParentWindowHandle)
            {
                ::GetWindowRect(ParentWindowHandle, &ParentRect);
            }
            else
            {
                HMONITOR MonitorHandle = ::MonitorFromWindow(
                    WindowHandle,
                    MONITOR_DEFAULTTONEAREST);
                if (MonitorHandle)
                {
                    MONITORINFO MonitorInfo;
                    MonitorInfo.cbSize = sizeof(MONITORINFO);
                    if (::GetMonitorInfoW(MonitorHandle, &MonitorInfo))
                    {
                        ParentRect = MonitorInfo.rcWork;
                    }
                }
            }

            int ParentWidth = ParentRect.right - ParentRect.left;
            int ParentHeight = ParentRect.bottom - ParentRect.top;

            ::SetWindowPos(
                WindowHandle,
                nullptr,
                ParentRect.left + ((ParentWidth - ScaledWidth) / 2),
                ParentRect.top + ((ParentHeight - ScaledHeight) / 2),
                ScaledWidth,
                ScaledHeight,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }

        ::ShowWindow(WindowHandle, SW_SHOW);
        ::UpdateWindow(WindowHandle);

        return ::MileXamlContentWindowDefaultMessageLoop();
    }

    int K7ModernShowXamlDialog(
        _In_opt_ HWND WindowHandle,
        _In_ int Width,
        _In_ int Height,
        _In_ LPVOID Content,
        _In_ HWND ParentWindowHandle)
    {
        if (!WindowHandle)
        {
            return -1;
        }

        ::MileAllowNonClientDefaultDrawingForWindow(WindowHandle, FALSE);

        HMENU MenuHandle = ::GetSystemMenu(WindowHandle, FALSE);
        if (MenuHandle)
        {
            ::RemoveMenu(MenuHandle, 0, MF_SEPARATOR);
            ::RemoveMenu(MenuHandle, SC_RESTORE, MF_BYCOMMAND);
            ::RemoveMenu(MenuHandle, SC_SIZE, MF_BYCOMMAND);
            ::RemoveMenu(MenuHandle, SC_MINIMIZE, MF_BYCOMMAND);
            ::RemoveMenu(MenuHandle, SC_MAXIMIZE, MF_BYCOMMAND);
        }

        if (ParentWindowHandle)
        {
            ::EnableWindow(ParentWindowHandle, FALSE);
        }

        if (FAILED(::MileXamlSetXamlContentForContentWindow(
            WindowHandle,
            Content)))
        {
            ::DestroyWindow(WindowHandle);
            return -1;
        }

        int Result = ::K7ModernShowXamlWindow(
            WindowHandle,
            Width,
            Height,
            ParentWindowHandle);

        return Result;
    }

    winrt::DesktopWindowXamlSource K7ModernGetDesktopWindowXamlSource(
        _In_ HWND WindowHandle)
    {
        winrt::DesktopWindowXamlSource XamlSource = nullptr;
        winrt::copy_from_abi(
            XamlSource,
            ::GetPropW(WindowHandle, L"XamlWindowSource"));
        return XamlSource;
    }
}

EXTERN_C INT WINAPI K7ModernShowSponsorDialog(
    _In_opt_ HWND ParentWindowHandle)
{
    HWND WindowHandle = ::K7ModernCreateXamlDialog(ParentWindowHandle);
    if (!WindowHandle)
    {
        return -1;
    }

    using Interface =
        winrt::NanaZip::Modern::SponsorPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::SponsorPage;

    Interface Window = winrt::make<Implementation>(WindowHandle);

    int Result = ::K7ModernShowXamlDialog(
        WindowHandle,
        460,
        320,
        winrt::get_abi(Window),
        ParentWindowHandle);

    return Result;
}

EXTERN_C INT WINAPI K7ModernShowAboutDialog(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR ExtendedMessage)
{
    HWND WindowHandle = ::K7ModernCreateXamlDialog(ParentWindowHandle);
    if (!WindowHandle)
    {
        return -1;
    }

    using Interface =
        winrt::NanaZip::Modern::AboutPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::AboutPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        ExtendedMessage);

    int Result = ::K7ModernShowXamlDialog(
        WindowHandle,
        480,
        320,
        winrt::get_abi(Window),
        ParentWindowHandle);

    return Result;
}

EXTERN_C INT WINAPI K7ModernShowInformationDialog(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR Title,
    _In_opt_ LPCWSTR Content)
{
    HWND WindowHandle = ::K7ModernCreateXamlDialog(ParentWindowHandle);
    if (!WindowHandle)
    {
        return -1;
    }

    using Interface =
        winrt::NanaZip::Modern::InformationPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::InformationPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        Title,
        Content);

    int Result = ::K7ModernShowXamlDialog(
        WindowHandle,
        560,
        560,
        winrt::get_abi(Window),
        ParentWindowHandle);

    return Result;
}

EXTERN_C VOID WINAPI K7ModernUpdateProgressWindowStatus(
    _In_ HWND WindowHandle,
    _In_ PK7_PROGRESS_WINDOW_STATUS Status)
{
    if (!WindowHandle || !Status)
    {
        return;
    }

    winrt::DesktopWindowXamlSource XamlSource =
        ::K7ModernGetDesktopWindowXamlSource(WindowHandle);
    if (!XamlSource)
    {
        return;
    }

    using Interface =
        winrt::NanaZip::Modern::ProgressPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::ProgressPage;
    Interface InstanceObject = XamlSource.Content().as<Interface>();
    if (!InstanceObject)
    {
        return;
    }
    winrt::get_self<Implementation>(InstanceObject)->UpdateStatus(Status);
}

EXTERN_C VOID WINAPI K7ModernSetProgressWindowPausedMode(
    _In_ HWND WindowHandle,
    _In_ BOOL Paused)
{
    if (!WindowHandle)
    {
        return;
    }

    winrt::DesktopWindowXamlSource XamlSource =
        ::K7ModernGetDesktopWindowXamlSource(WindowHandle);
    if (!XamlSource)
    {
        return;
    }

    using Interface =
        winrt::NanaZip::Modern::ProgressPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::ProgressPage;
    Interface InstanceObject = XamlSource.Content().as<Interface>();
    if (!InstanceObject)
    {
        return;
    }
    winrt::get_self<Implementation>(InstanceObject)->SetPausedMode(Paused);
}

EXTERN_C INT WINAPI K7ModernShowProgressWindow(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR Title,
    _In_ BOOL ShowCompressionInformation,
    _In_ SUBCLASSPROC WindowSubclassHandler,
    _In_ LPVOID WindowSubclassContext)
{
    HWND WindowHandle = ::K7ModernCreateXamlWindow(
        ParentWindowHandle,
        WS_EX_STATICEDGE | WS_EX_DLGMODALFRAME,
        WS_OVERLAPPEDWINDOW);
    if (!WindowHandle)
    {
        return -1;
    }

    ::MileAllowNonClientDefaultDrawingForWindow(WindowHandle, FALSE);

    using Interface =
        winrt::NanaZip::Modern::ProgressPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::ProgressPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        Title,
        ShowCompressionInformation);

    if (FAILED(::MileXamlSetXamlContentForContentWindow(
        WindowHandle,
        winrt::get_abi(Window))))
    {
        ::DestroyWindow(WindowHandle);
        return -1;
    }

    if (WindowSubclassHandler)
    {
        if (!::SetWindowSubclass(
            WindowHandle,
            WindowSubclassHandler,
            1,
            reinterpret_cast<DWORD_PTR>(WindowSubclassContext)))
        {
            ::DestroyWindow(WindowHandle);
            return -1;
        }
    }

    int Result = ::K7ModernShowXamlWindow(
        WindowHandle,
        600,
        360,
        ParentWindowHandle);

    return Result;
}

EXTERN_C INT WINAPI K7ModernShowCopyLocationDialog(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR Title,
    _In_opt_ LPCWSTR Subtitle,
    _In_opt_ LPCWSTR AdditionalInformation,
    _In_opt_ LPCWSTR InitialPath,
    _In_ SUBCLASSPROC WindowSubclassHandler,
    _In_ LPVOID WindowSubclassContext)
{
    HWND WindowHandle = ::K7ModernCreateXamlDialog(ParentWindowHandle);
    if (!WindowHandle)
    {
        return -1;
    }

    using Interface =
        winrt::NanaZip::Modern::CopyLocationPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::CopyLocationPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        Title,
        Subtitle,
        AdditionalInformation,
        InitialPath);

    if (WindowSubclassHandler)
    {
        if (!::SetWindowSubclass(
            WindowHandle,
            WindowSubclassHandler,
            1,
            reinterpret_cast<DWORD_PTR>(WindowSubclassContext)))
        {
            ::DestroyWindow(WindowHandle);
            return -1;
        }
    }

    int Result = ::K7ModernShowXamlDialog(
        WindowHandle,
        600,
        400,
        winrt::get_abi(Window),
        ParentWindowHandle);

    return Result;
}

EXTERN_C LPCWSTR WINAPI K7ModernGetCopyLocationDialogPath(
    _In_ HWND WindowHandle)
{
    if (!WindowHandle)
    {
        return nullptr;
    }

    winrt::DesktopWindowXamlSource XamlSource =
        ::K7ModernGetDesktopWindowXamlSource(WindowHandle);
    if (!XamlSource)
    {
        return nullptr;
    }

    using Interface =
        winrt::NanaZip::Modern::CopyLocationPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::CopyLocationPage;
    Interface InstanceObject = XamlSource.Content().as<Interface>();
    if (!InstanceObject)
    {
        return nullptr;
    }
    return winrt::get_self<Implementation>(InstanceObject)->GetPath();
}

EXTERN_C INT WINAPI K7ModernShowOverwriteDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_OVERWRITE_DIALOG_CONTEXT Context)
{
    if (!Context)
    {
        return -1;
    }

    HWND WindowHandle = ::K7ModernCreateXamlDialog(ParentWindowHandle);
    if (!WindowHandle)
    {
        return -1;
    }

    using Interface =
        winrt::NanaZip::Modern::OverwritePage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::OverwritePage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        Context);

    ::MileAllowNonClientDefaultDrawingForWindow(WindowHandle, FALSE);

    if (ParentWindowHandle)
    {
        ::EnableWindow(ParentWindowHandle, FALSE);
    }

    if (FAILED(::MileXamlSetXamlContentForContentWindow(
        WindowHandle,
        winrt::get_abi(Window))))
    {
        ::DestroyWindow(WindowHandle);
        return -1;
    }

    int Result = ::K7ModernShowXamlWindow(
        WindowHandle,
        420,
        260,
        ParentWindowHandle,
        nullptr);

    return Result;
}

EXTERN_C INT WINAPI K7ModernShowExtractDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_EXTRACT_DIALOG_CONTEXT Context)
{
    if (!Context)
    {
        return -1;
    }

    HWND WindowHandle = ::K7ModernCreateXamlDialog(ParentWindowHandle);
    if (!WindowHandle)
    {
        return -1;
    }

    using Interface =
        winrt::NanaZip::Modern::ExtractPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::ExtractPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        Context);

    ::MileAllowNonClientDefaultDrawingForWindow(WindowHandle, FALSE);

    // The extract dialog adapts its size to the content, but the user should
    // still be able to enlarge it (e.g. with a larger dialog font size), so
    // give it a resize border and the maximize/minimize box.
    {
        LONG_PTR Style = ::GetWindowLongPtrW(WindowHandle, GWL_STYLE);
        Style |= WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
        ::SetWindowLongPtrW(WindowHandle, GWL_STYLE, Style);
    }

    // Enforce a minimum window size so that dragging the border can never
    // shrink the dialog below its measured content (which would clip the
    // options). The XAML page writes MinTrackW/MinTrackH from its content
    // during PrepareForShow; this subclass reads them in WM_GETMINMAXINFO.
    if (!::SetWindowSubclass(
        WindowHandle,
        [](
            _In_ HWND hWnd,
            _In_ UINT uMsg,
            _In_ WPARAM wParam,
            _In_ LPARAM lParam,
            _In_ UINT_PTR uIdSubclass,
            _In_ DWORD_PTR dwRefData) -> LRESULT
    {
        UNREFERENCED_PARAMETER(wParam);
        UNREFERENCED_PARAMETER(uIdSubclass);
        if (uMsg == WM_GETMINMAXINFO)
        {
            const LONG *MinTrack = reinterpret_cast<const LONG *>(dwRefData);
            if (MinTrack && MinTrack[0] > 0 && MinTrack[1] > 0)
            {
                MINMAXINFO *MinMaxInfo =
                    reinterpret_cast<MINMAXINFO *>(lParam);
                MinMaxInfo->ptMinTrackSize.x = MinTrack[0];
                MinMaxInfo->ptMinTrackSize.y = MinTrack[1];
                return 0;
            }
        }
        return ::DefSubclassProc(
            hWnd,
            uMsg,
            wParam,
            lParam);
    },
        1,
        reinterpret_cast<DWORD_PTR>(&Context->MinTrackW)))
    {
        ::DestroyWindow(WindowHandle);
        return -1;
    }

    if (FAILED(::MileXamlSetXamlContentForContentWindow(
        WindowHandle,
        winrt::get_abi(Window))))
    {
        ::DestroyWindow(WindowHandle);
        return -1;
    }

    if (ParentWindowHandle)
    {
        ::EnableWindow(ParentWindowHandle, FALSE);
    }

    // Apply the dialog font and measure the content before the window is
    // shown, so the initial size is final (no visible resize after show).
    // Desired is the raw content size; the default window gets a comfortable
    // margin so every line is fully visible on one row, while the minimum
    // track size is smaller (the user may compress the dialog, letting long
    // text wrap, without hiding the controls entirely).
    winrt::Windows::Foundation::Size Desired(560, 460);
    {
        auto Self = winrt::get_self<Implementation>(Window);
        Desired = Self->PrepareForShow();
    }

    const UINT Dpi = ::GetDpiForWindow(WindowHandle);
    const float Scale = (float)Dpi / (float)USER_DEFAULT_SCREEN_DPI;

    // Default size: content plus a comfortable margin, clamped to a sane
    // range so the dialog never gets absurdly wide or narrow.
    int ClientW = (int)((Desired.Width + 64.0f) * Scale + 0.5f);
    int ClientH = (int)((Desired.Height + 40.0f) * Scale + 0.5f);
    if (ClientW < 560) ClientW = 560;
    if (ClientH < 460) ClientH = 460;

    // Never exceed the work area (minus a small margin) even for very long
    // paths, so the dialog cannot open off-screen; 1400 is a hard backstop.
    RECT ParentRect = {};
    if (ParentWindowHandle)
    {
        ::GetWindowRect(ParentWindowHandle, &ParentRect);
    }
    else
    {
        HMONITOR MonitorHandle = ::MonitorFromWindow(
            WindowHandle,
            MONITOR_DEFAULTTONEAREST);
        if (MonitorHandle)
        {
            MONITORINFO MonitorInfo;
            MonitorInfo.cbSize = sizeof(MONITORINFO);
            if (::GetMonitorInfoW(MonitorHandle, &MonitorInfo))
            {
                ParentRect = MonitorInfo.rcWork;
            }
        }
    }
    if (ParentRect.right > ParentRect.left)
    {
        const int WorkW = ParentRect.right - ParentRect.left - 48;
        const int MaxClientW = (int)((float)WorkW / Scale + 0.5f);
        if (ClientW > MaxClientW)
        {
            ClientW = MaxClientW;
        }
    }
    if (ClientW > 1400) ClientW = 1400;
    if (ClientH > 900) ClientH = 900;

    // Minimum size: the width may shrink (text may wrap), but the height
    // stays at the full content height so the buttons and the lower controls
    // are never clipped when the user compresses the dialog vertically.
    int MinClientW = (int)((Desired.Width - 48.0f) * Scale + 0.5f);
    int MinClientH = (int)(Desired.Height * Scale + 0.5f);
    if (MinClientW < 480) MinClientW = 480;
    if (MinClientH < 400) MinClientH = 400;

    RECT rc = { 0, 0, ClientW, ClientH };
    {
        const LONG_PTR Style = ::GetWindowLongPtrW(WindowHandle, GWL_STYLE);
        const LONG_PTR ExStyle = ::GetWindowLongPtrW(WindowHandle, GWL_EXSTYLE);
        ::AdjustWindowRectEx(&rc, (DWORD)Style, FALSE, (DWORD)ExStyle);
    }

    RECT rcMin = { 0, 0, MinClientW, MinClientH };
    {
        const LONG_PTR Style = ::GetWindowLongPtrW(WindowHandle, GWL_STYLE);
        const LONG_PTR ExStyle = ::GetWindowLongPtrW(WindowHandle, GWL_EXSTYLE);
        ::AdjustWindowRectEx(&rcMin, (DWORD)Style, FALSE, (DWORD)ExStyle);
    }

    Context->MinTrackW = rcMin.right - rcMin.left;
    Context->MinTrackH = rcMin.bottom - rcMin.top;

    // Center the dialog on the parent window (or the nearest monitor).
    // ParentRect was already resolved above for the work-area limit.
    const int WindowW = rc.right - rc.left;
    const int WindowH = rc.bottom - rc.top;
    const int PosX = ParentRect.left +
        ((ParentRect.right - ParentRect.left - WindowW) / 2);
    const int PosY = ParentRect.top +
        ((ParentRect.bottom - ParentRect.top - WindowH) / 2);
    RECT InitialRect = { PosX, PosY, PosX + WindowW, PosY + WindowH };

    int Result = ::K7ModernShowXamlWindow(
        WindowHandle,
        ClientW,
        ClientH,
        ParentWindowHandle,
        &InitialRect);

    return Result;
}

EXTERN_C INT WINAPI K7ModernShowSettingsDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_SETTINGS_DIALOG_CONTEXT Context)
{
    if (!Context)
    {
        return -1;
    }

    HWND WindowHandle = ::K7ModernCreateXamlDialog(ParentWindowHandle);
    if (!WindowHandle)
    {
        return -1;
    }

    // The settings dialog is resizable so that the user can enlarge it when
    // a larger dialog font size is selected. The default dialog style has no
    // resize border, so add the thick frame and the maximize/minimize box.
    {
        LONG_PTR Style = ::GetWindowLongPtrW(WindowHandle, GWL_STYLE);
        Style |= WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
        ::SetWindowLongPtrW(WindowHandle, GWL_STYLE, Style);
    }

    // Enforce a minimum window size so the dialog cannot be resized into a
    // useless state (which matters when the user picks a huge font size).
    // Also snapshot the window rect when the dialog starts closing, while
    // the window is still alive (the XAML Unloaded event may fire after the
    // window is already destroyed, which would leave GetWindowRect empty).
    if (!::SetWindowSubclass(
        WindowHandle,
        [](
            _In_ HWND hWnd,
            _In_ UINT uMsg,
            _In_ WPARAM wParam,
            _In_ LPARAM lParam,
            _In_ UINT_PTR uIdSubclass,
            _In_ DWORD_PTR dwRefData) -> LRESULT
    {
        UNREFERENCED_PARAMETER(uIdSubclass);
        if (uMsg == WM_CLOSE)
        {
            if (dwRefData)
            {
                ::GetWindowRect(hWnd,
                    reinterpret_cast<RECT *>(dwRefData));
            }
        }
        else if (uMsg == WM_GETMINMAXINFO)
        {
            MINMAXINFO *MinMaxInfo =
                reinterpret_cast<MINMAXINFO *>(lParam);
            UINT DpiValue = ::GetDpiForWindow(hWnd);
            MinMaxInfo->ptMinTrackSize.x =
                ::MulDiv(480, DpiValue, USER_DEFAULT_SCREEN_DPI);
            MinMaxInfo->ptMinTrackSize.y =
                ::MulDiv(420, DpiValue, USER_DEFAULT_SCREEN_DPI);
            return 0;
        }
        return ::DefSubclassProc(
            hWnd,
            uMsg,
            wParam,
            lParam);
    },
        1,
        reinterpret_cast<DWORD_PTR>(&Context->WindowRect)))
    {
        ::DestroyWindow(WindowHandle);
        return -1;
    }

    using Interface =
        winrt::NanaZip::Modern::SettingsPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::SettingsPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        Context);

    // Dedicated flow: K7ModernShowXamlDialog is not used here because it
    // removes the SC_SIZE/SC_MAXIMIZE/SC_MINIMIZE system menu commands,
    // which would break the resize and maximize features of this dialog.
    ::MileAllowNonClientDefaultDrawingForWindow(WindowHandle, FALSE);

    if (ParentWindowHandle)
    {
        ::EnableWindow(ParentWindowHandle, FALSE);
    }

    if (FAILED(::MileXamlSetXamlContentForContentWindow(
        WindowHandle,
        winrt::get_abi(Window))))
    {
        ::DestroyWindow(WindowHandle);
        return -1;
    }

    int Result = ::K7ModernShowXamlWindow(
        WindowHandle,
        540,
        640,
        ParentWindowHandle,
        &Context->WindowRect);

    return Result;
}
