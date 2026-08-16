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
#include <K7User.h>

#include <Mile.Helpers.h>
#include <Mile.Xaml.h>

#include "App.h"
#include "SponsorPage.h"
#include "AboutPage.h"
#include "ExtractPage.h"
#include "CompressPage.h"
#include "OverwritePage.h"
#include "InformationPage.h"
#include "ProgressPage.h"
#include "CopyLocationPage.h"
#include "SettingsPage.h"
#include "CompressOptionsPage.h"
#include "SplitPage.h"
#include "PasswordPage.h"
#include "ComboPage.h"
#include "LinkPage.h"
#include "BenchmarkPage.h"

#pragma comment(lib, "comctl32.lib")

#include <winrt/Windows.ApplicationModel.Resources.Core.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>

#include <algorithm>
#include <cmath>
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

    if (FAILED(::MileXamlSetXamlContentForContentWindow(
        WindowHandle,
        winrt::get_abi(Window))))
    {
        ::DestroyWindow(WindowHandle);
        return -1;
    }

    // Disable the owner so the progress window is modal and receives the
    // keyboard focus (Esc / Cancel). Without this the window is shown with
    // SWP_NOACTIVATE and never becomes the foreground, so keys keep going
    // to the owner window and the Esc-cancel path is never reached. The
    // owner is re-enabled when the window closes (the ContentWindow
    // subclass in K7ModernCreateXamlWindow handles WM_CLOSE) and again
    // after the message loop returns below.
    if (ParentWindowHandle)
    {
        ::EnableWindow(ParentWindowHandle, FALSE);
    }

    int Result = ::K7ModernShowXamlWindow(
        WindowHandle,
        600,
        360,
        ParentWindowHandle);

    if (ParentWindowHandle)
    {
        ::EnableWindow(ParentWindowHandle, TRUE);
    }

    return Result;
}

EXTERN_C INT WINAPI K7ModernShowCopyLocationDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_COPY_DIALOG_CONTEXT Context)
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
        winrt::NanaZip::Modern::CopyLocationPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::CopyLocationPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        Context);

    ::MileAllowNonClientDefaultDrawingForWindow(WindowHandle, FALSE);

    // Enforce a minimum window size so that dragging the border can never
    // shrink the dialog below its measured content (which would clip the
    // controls). The XAML page writes MinTrackW/MinTrackH during
    // PrepareForShow; this subclass reads them in WM_GETMINMAXINFO.
    if (!::SetWindowSubclass(
        WindowHandle,
        [](_In_ HWND hWnd,
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
        else if (uMsg == WM_KEYDOWN && wParam == VK_ESCAPE)
        {
            // Esc closes the dialog like the X button (a cancel: the page's
            // OnUnloaded clears the OK flag).
            ::PostMessageW(hWnd, WM_CLOSE, 0, 0);
            return 0;
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
    winrt::Windows::Foundation::Size Desired(400, 160);
    {
        auto Self = winrt::get_self<Implementation>(Window);
        Desired = Self->PrepareForShow();
    }

    const UINT Dpi = ::GetDpiForWindow(WindowHandle);
    const float Scale = (float)Dpi / (float)USER_DEFAULT_SCREEN_DPI;

    // Default size: content plus a tiny margin. The content rows are Auto,
    // so Desired already is the full height; keeping the window close to the
    // content avoids empty space both below the prompt row and below the
    // buttons.
    int ClientW = (int)((Desired.Width + 48.0f) * Scale + 0.5f);
    int ClientH = (int)((Desired.Height + 4.0f) * Scale + 0.5f);
    if (ClientW < 360) ClientW = 360;
    if (ClientH < 140) ClientH = 140;

    // Never exceed the work area (minus a small margin), so the dialog
    // cannot open off-screen; 1400 is a hard backstop.
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

    RECT rc = { 0, 0, ClientW, ClientH };
    {
        const LONG_PTR Style = ::GetWindowLongPtrW(WindowHandle, GWL_STYLE);
        const LONG_PTR ExStyle = ::GetWindowLongPtrW(WindowHandle, GWL_EXSTYLE);
        ::AdjustWindowRectEx(&rc, (DWORD)Style, FALSE, (DWORD)ExStyle);
    }

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

EXTERN_C INT WINAPI K7ModernShowOverwriteDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_OVERWRITE_DIALOG_CONTEXT Context)
{
    if (!Context)
    {
        return -1;
    }

    Context->Result = K7_OVERWRITE_DIALOG_RESULT_CANCEL;

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
        if (ParentWindowHandle)
        {
            ::EnableWindow(ParentWindowHandle, TRUE);
        }
        ::DestroyWindow(WindowHandle);
        return -1;
    }

    // Measure the initialized page before showing it. This keeps the footer
    // directly below the file information instead of forcing the page into a
    // fixed rectangle with unrelated empty space.
    using PageImplementation =
        winrt::NanaZip::Modern::implementation::OverwritePage;
    winrt::Windows::Foundation::Size Desired(420, 260);
    {
        auto Page = winrt::get_self<PageImplementation>(Window);
        Desired = Page->PrepareForShow();
    }

    const UINT Dpi = ::GetDpiForWindow(WindowHandle);
    const float Scale = (float)Dpi / (float)USER_DEFAULT_SCREEN_DPI;
    int ClientW = (int)(Desired.Width * Scale + 0.5f);
    int ClientH = (int)(Desired.Height * Scale + 0.5f);
    if (ClientW < (int)(360.0f * Scale))
    {
        ClientW = (int)(360.0f * Scale);
    }
    if (ClientH < (int)(180.0f * Scale))
    {
        ClientH = (int)(180.0f * Scale);
    }

    RECT WindowRect = { 0, 0, ClientW, ClientH };
    const LONG_PTR Style = ::GetWindowLongPtrW(WindowHandle, GWL_STYLE);
    const LONG_PTR ExStyle = ::GetWindowLongPtrW(WindowHandle, GWL_EXSTYLE);
    ::AdjustWindowRectEx(
        &WindowRect,
        (DWORD)Style,
        FALSE,
        (DWORD)ExStyle);

    RECT CenterRect = {};
    if (ParentWindowHandle)
    {
        ::GetWindowRect(ParentWindowHandle, &CenterRect);
    }
    if (CenterRect.right <= CenterRect.left ||
        CenterRect.bottom <= CenterRect.top)
    {
        HMONITOR MonitorHandle = ::MonitorFromWindow(
            WindowHandle,
            MONITOR_DEFAULTTONEAREST);
        if (MonitorHandle)
        {
            MONITORINFO MonitorInfo = {};
            MonitorInfo.cbSize = sizeof(MonitorInfo);
            if (::GetMonitorInfoW(MonitorHandle, &MonitorInfo))
            {
                CenterRect = MonitorInfo.rcWork;
            }
        }
    }

    if (CenterRect.right > CenterRect.left &&
        CenterRect.bottom > CenterRect.top)
    {
        const int MaxClientW = (std::max)(
            360,
            (int)(CenterRect.right - CenterRect.left - 48));
        const int MaxClientH = (std::max)(
            180,
            (int)(CenterRect.bottom - CenterRect.top - 48));
        ClientW = (std::min)(ClientW, MaxClientW);
        ClientH = (std::min)(ClientH, MaxClientH);
    }

    WindowRect = { 0, 0, ClientW, ClientH };
    ::AdjustWindowRectEx(
        &WindowRect,
        (DWORD)Style,
        FALSE,
        (DWORD)ExStyle);

    const int WindowW = WindowRect.right - WindowRect.left;
    const int WindowH = WindowRect.bottom - WindowRect.top;
    const int PosX = CenterRect.left +
        ((CenterRect.right - CenterRect.left - WindowW) / 2);
    const int PosY = CenterRect.top +
        ((CenterRect.bottom - CenterRect.top - WindowH) / 2);
    RECT InitialRect = {
        PosX,
        PosY,
        PosX + WindowW,
        PosY + WindowH };

    // K7ModernShowXamlWindow expects client dimensions when it has no saved
    // rectangle. InitialRect carries the already adjusted outer rectangle.
    return ::K7ModernShowXamlWindow(
        WindowHandle,
        ClientW,
        ClientH,
        ParentWindowHandle,
        &InitialRect);
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
        else if (uMsg == WM_KEYDOWN && wParam == VK_ESCAPE)
        {
            // Esc closes the dialog like the X button (a cancel: the page's
            // OnUnloaded clears the OK flag).
            ::PostMessageW(hWnd, WM_CLOSE, 0, 0);
            return 0;
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

// Temporary diagnostics for the compression-dialog startup crash. Appends
// to %TEMP%\k7compress_diag.log; remove once the crash is fixed.
static void DiagLog(const wchar_t* msg)
{
    wchar_t path[MAX_PATH];
    const DWORD n = ::GetTempPathW(MAX_PATH, path);
    if (n == 0 || n >= MAX_PATH)
    {
        return;
    }
    wcscat_s(path, L"k7compress_diag.log");
    HANDLE h = ::CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        ::WriteFile(h, msg, (DWORD)(wcslen(msg) * sizeof(wchar_t)),
            &written, nullptr);
        ::WriteFile(h, L"\r\n", 4, &written, nullptr);
        ::CloseHandle(h);
    }
}

EXTERN_C INT WINAPI K7ModernShowCompressDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_COMPRESS_DIALOG_CONTEXT Context)
{
    if (!Context)
    {
        return -1;
    }

    DiagLog(L"[M1] K7ModernShowCompressDialog enter");

    HWND WindowHandle = ::K7ModernCreateXamlDialog(ParentWindowHandle);
    if (!WindowHandle)
    {
        return -1;
    }
    DiagLog(L"[M2] CreateXamlDialog ok");

    // The compression dialog is resizable so the user can enlarge it when a
    // larger dialog font size is selected, and can compress it to let long
    // text wrap. Give it a thick frame and the maximize/minimize box.
    {
        LONG_PTR Style = ::GetWindowLongPtrW(WindowHandle, GWL_STYLE);
        Style |= WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
        ::SetWindowLongPtrW(WindowHandle, GWL_STYLE, Style);
    }

    // Enforce a minimum window size from the measured content so dragging
    // can never clip the controls. The page writes MinTrackW/MinTrackH.
    if (!::SetWindowSubclass(
        WindowHandle,
        [](_In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam,
            _In_ LPARAM lParam, _In_ UINT_PTR uIdSubclass,
            _In_ DWORD_PTR dwRefData) -> LRESULT
    {
        UNREFERENCED_PARAMETER(uIdSubclass);
        PK7_COMPRESS_DIALOG_CONTEXT Context =
            reinterpret_cast<PK7_COMPRESS_DIALOG_CONTEXT>(dwRefData);
        if (uMsg == WM_CLOSE)
        {
            if (Context)
            {
                ::GetWindowRect(hWnd, &Context->WindowRect);
            }
        }
        else if (uMsg == WM_GETMINMAXINFO)
        {
            // Preserve USER32's default max-track/work-area values, then
            // override only the content-driven minimum size.
            LRESULT Result = ::DefSubclassProc(
                hWnd, uMsg, wParam, lParam);
            if (Context && Context->MinTrackW > 0 && Context->MinTrackH > 0)
            {
                MINMAXINFO *MinMaxInfo =
                    reinterpret_cast<MINMAXINFO *>(lParam);
                MinMaxInfo->ptMinTrackSize.x = Context->MinTrackW;
                MinMaxInfo->ptMinTrackSize.y = Context->MinTrackH;
            }
            return Result;
        }
        else if (uMsg == WM_KEYDOWN && wParam == VK_ESCAPE)
        {
            ::PostMessageW(hWnd, WM_CLOSE, 0, 0);
            return 0;
        }
        else if (uMsg == K7_COMPRESS_OPTIONS_OPEN_MESSAGE)
        {
            // Open the options child dialog at the Win32 message level:
            // the dialog creates a second XAML island, which must not be
            // created synchronously inside the XAML event handler (the
            // single-threaded XAML core would deadlock when the island is
            // shown). This message is dispatched by the window's own
            // message loop after the current XAML callback has returned.
            if (Context && Context->OptionsCallback)
            {
                Context->OptionsCallback(Context->CallbackContext);
            }
            // The adapter updated the snapshot; refresh the page UI now
            // that the options dialog has closed.
            winrt::DesktopWindowXamlSource XamlSource =
                ::K7ModernGetDesktopWindowXamlSource(hWnd);
            if (XamlSource)
            {
                if (auto Page = XamlSource.Content().try_as<
                    winrt::NanaZip::Modern::CompressPage>())
                {
                    winrt::get_self<
                        winrt::NanaZip::Modern::implementation::CompressPage>(
                            Page)->OnOptionsClosed();
                }
            }
            return 0;
        }
        return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
    },
        1,
        reinterpret_cast<DWORD_PTR>(Context)))
    {
        ::DestroyWindow(WindowHandle);
        return -1;
    }
    DiagLog(L"[M3] subclass ok");

    using Interface =
        winrt::NanaZip::Modern::CompressPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::CompressPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        Context);
    DiagLog(L"[M4] make ok");

    ::MileAllowNonClientDefaultDrawingForWindow(WindowHandle, FALSE);

    if (FAILED(::MileXamlSetXamlContentForContentWindow(
        WindowHandle,
        winrt::get_abi(Window))))
    {
        ::DestroyWindow(WindowHandle);
        return -1;
    }
    DiagLog(L"[M5] SetXamlContent ok");

    if (ParentWindowHandle)
    {
        ::EnableWindow(ParentWindowHandle, FALSE);
    }

    winrt::Windows::Foundation::Size Desired(640, 560);
    {
        auto Self = winrt::get_self<Implementation>(Window);
        Desired = Self->PrepareForShow();
    }
    DiagLog(L"[M6] PrepareForShow ok");

    const UINT Dpi = ::GetDpiForWindow(WindowHandle);
    const float Scale = (float)Dpi / (float)USER_DEFAULT_SCREEN_DPI;

    // Default size: content plus a comfortable margin, so the dialog opens
    // with every label/combo row on one line (never wrapped).
    int ClientW = (int)((Desired.Width + 64.0f) * Scale + 0.5f);
    int ClientH = (int)((Desired.Height + 40.0f) * Scale + 0.5f);
    if (ClientW < 560) ClientW = 560;
    if (ClientH < 480) ClientH = 480;

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
        // Never let the dialog open taller than the screen work area (the
        // content scrolls inside the page's ScrollViewer and the OK/Cancel
        // row stays pinned, so the buttons are always reachable). This is
        // what actually stopped the dialog from running off the screen on
        // shorter displays.
        const int WorkH = ParentRect.bottom - ParentRect.top - 48;
        const int MaxClientH = (int)((float)WorkH / Scale + 0.5f);
        if (ClientH > MaxClientH)
        {
            ClientH = MaxClientH;
        }
    }
    if (ClientW > 1400) ClientW = 1400;
    if (ClientH > 900) ClientH = 900;

    int MinClientW = (int)((Desired.Width - 48.0f) * Scale + 0.5f);
    // The content is hosted in a ScrollViewer and the button row is Auto;
    // allow the user to make the window substantially shorter while keeping
    // OK/Cancel reachable.
    int MinClientH = (int)(168.0f * Scale + 0.5f);
    if (MinClientW < 480) MinClientW = 480;
    if (MinClientH < 168) MinClientH = 168;

    RECT rc = { 0, 0, ClientW, ClientH };
    {
        const LONG_PTR Style = ::GetWindowLongPtrW(WindowHandle, GWL_STYLE);
        const LONG_PTR ExStyle = ::GetWindowLongPtrW(WindowHandle, GWL_EXSTYLE);
        ::AdjustWindowRectEx(&rc, (DWORD)Style, FALSE, (DWORD)ExStyle);
    }

    // PrepareForShow/RecalcMinTrack has already calculated the minimum
    // window size from the fully wrapped layout. Do not replace it here with
    // the compact default measurement, or the resize gesture can be stopped
    // before the rows get a chance to wrap.
    if (Context->MinTrackW <= 0 || Context->MinTrackH <= 0)
    {
        RECT rcMin = { 0, 0, MinClientW, MinClientH };
        const LONG_PTR Style = ::GetWindowLongPtrW(
            WindowHandle, GWL_STYLE);
        const LONG_PTR ExStyle = ::GetWindowLongPtrW(
            WindowHandle, GWL_EXSTYLE);
        ::AdjustWindowRectEx(
            &rcMin, (DWORD)Style, FALSE, (DWORD)ExStyle);
        Context->MinTrackW = rcMin.right - rcMin.left;
        Context->MinTrackH = rcMin.bottom - rcMin.top;
    }

    RECT rcMin = { 0, 0, Context->MinTrackW, Context->MinTrackH };

    const int WindowW = rc.right - rc.left;
    const int WindowH = rc.bottom - rc.top;
    const int PosX = ParentRect.left +
        ((ParentRect.right - ParentRect.left - WindowW) / 2);
    const int PosY = ParentRect.top +
        ((ParentRect.bottom - ParentRect.top - WindowH) / 2);
    RECT InitialRect = { PosX, PosY, PosX + WindowW, PosY + WindowH };

    // Restore the last size and position when it still intersects a monitor.
    // A disconnected monitor or an undersized saved rectangle falls back to
    // the centered default calculated above.
    if (Context->WindowRect.right > Context->WindowRect.left &&
        Context->WindowRect.bottom > Context->WindowRect.top &&
        ::MonitorFromRect(&Context->WindowRect, MONITOR_DEFAULTTONULL))
    {
        InitialRect = Context->WindowRect;
        const int MinWindowW = rcMin.right - rcMin.left;
        const int MinWindowH = rcMin.bottom - rcMin.top;
        if (InitialRect.right - InitialRect.left < MinWindowW)
        {
            InitialRect.right = InitialRect.left + MinWindowW;
        }
        if (InitialRect.bottom - InitialRect.top < MinWindowH)
        {
            InitialRect.bottom = InitialRect.top + MinWindowH;
        }

        HMONITOR Monitor = ::MonitorFromRect(
            &InitialRect, MONITOR_DEFAULTTONEAREST);
        MONITORINFO MonitorInfo = { sizeof(MonitorInfo) };
        if (Monitor && ::GetMonitorInfoW(Monitor, &MonitorInfo))
        {
            const int WorkWidth =
                MonitorInfo.rcWork.right - MonitorInfo.rcWork.left;
            const int WorkHeight =
                MonitorInfo.rcWork.bottom - MonitorInfo.rcWork.top;
            int SavedWidth = InitialRect.right - InitialRect.left;
            int SavedHeight = InitialRect.bottom - InitialRect.top;
            if (SavedWidth > WorkWidth)
            {
                SavedWidth = WorkWidth;
            }
            if (SavedHeight > WorkHeight)
            {
                SavedHeight = WorkHeight;
            }
            InitialRect.right = InitialRect.left + SavedWidth;
            InitialRect.bottom = InitialRect.top + SavedHeight;
            if (InitialRect.left < MonitorInfo.rcWork.left)
            {
                InitialRect.left = MonitorInfo.rcWork.left;
                InitialRect.right = InitialRect.left + SavedWidth;
            }
            if (InitialRect.top < MonitorInfo.rcWork.top)
            {
                InitialRect.top = MonitorInfo.rcWork.top;
                InitialRect.bottom = InitialRect.top + SavedHeight;
            }
            if (InitialRect.right > MonitorInfo.rcWork.right)
            {
                InitialRect.right = MonitorInfo.rcWork.right;
                InitialRect.left = InitialRect.right - SavedWidth;
            }
            if (InitialRect.bottom > MonitorInfo.rcWork.bottom)
            {
                InitialRect.bottom = MonitorInfo.rcWork.bottom;
                InitialRect.top = InitialRect.bottom - SavedHeight;
            }
        }
    }

    int Result = ::K7ModernShowXamlWindow(
        WindowHandle,
        ClientW,
        ClientH,
        ParentWindowHandle,
        &InitialRect);

    return Result;
}

// Temporary diagnostics for the settings-dialog Esc/input routing. Appends
// to %TEMP%\sss_settings_diag.log; remove once the Esc issue is located.
static void SettingsDiagLog(const wchar_t* msg)
{
    wchar_t path[MAX_PATH];
    const DWORD n = ::GetTempPathW(MAX_PATH, path);
    if (n == 0 || n >= MAX_PATH)
    {
        return;
    }
    wcscat_s(path, L"sss_settings_diag.log");
    HANDLE h = ::CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        ::WriteFile(h, msg, (DWORD)(wcslen(msg) * sizeof(wchar_t)),
            &written, nullptr);
        ::WriteFile(h, L"\r\n", 4, &written, nullptr);
        ::CloseHandle(h);
    }
}

EXTERN_C INT WINAPI K7ModernShowSettingsDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_SETTINGS_DIALOG_CONTEXT Context)
{
    if (!Context)
    {
        return -1;
    }

    SettingsDiagLog(L"[S1] K7ModernShowSettingsDialog enter");

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
    // The XAML page computes the minimum from its fully wrapped content;
    // this subclass reads it in WM_GETMINMAXINFO and falls back to a fixed
    // floor until the page has run. Also snapshot the window rect when the
    // dialog starts closing, while the window is still alive (the XAML
    // Unloaded event may fire after the window is already destroyed, which
    // would leave GetWindowRect empty).
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
        auto SettingsContext =
            reinterpret_cast<PK7_SETTINGS_DIALOG_CONTEXT>(dwRefData);
        if (uMsg == WM_CLOSE)
        {
            if (SettingsContext)
            {
                ::GetWindowRect(hWnd, &SettingsContext->WindowRect);
            }
            SettingsDiagLog(L"[S3] WM_CLOSE");
        }
        else if (uMsg == WM_KEYDOWN && wParam == VK_ESCAPE)
        {
            // Esc closes the dialog like the X button (a cancel). The
            // Win32-level handler is the reliable path: the keyboard focus
            // often sits on the Win32 host window, so the XAML page's own
            // KeyDown/PreviewKeyDown never sees the key.
            SettingsDiagLog(L"[S2] WM_KEYDOWN VK_ESCAPE");
            ::PostMessageW(hWnd, WM_CLOSE, 0, 0);
            return 0;
        }
        else if (uMsg == WM_GETMINMAXINFO)
        {
            MINMAXINFO *MinMaxInfo =
                reinterpret_cast<MINMAXINFO *>(lParam);
            UINT DpiValue = ::GetDpiForWindow(hWnd);
            MinMaxInfo->ptMinTrackSize.x =
                (SettingsContext && SettingsContext->MinTrackW > 0)
                ? SettingsContext->MinTrackW
                : ::MulDiv(480, DpiValue, USER_DEFAULT_SCREEN_DPI);
            MinMaxInfo->ptMinTrackSize.y =
                (SettingsContext && SettingsContext->MinTrackH > 0)
                ? SettingsContext->MinTrackH
                : ::MulDiv(420, DpiValue, USER_DEFAULT_SCREEN_DPI);
            return 0;
        }
        return ::DefSubclassProc(
            hWnd,
            uMsg,
            wParam,
            lParam);
    },
        1,
        reinterpret_cast<DWORD_PTR>(Context)))
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

    // Measure the initialized page before showing it: the default size
    // follows the current tab's content (labels, combos and dialog font
    // already applied) instead of a hard-coded rectangle, so the dialog
    // opens at the size its content actually needs.
    winrt::Windows::Foundation::Size Desired(540, 640);
    {
        auto Page = winrt::get_self<Implementation>(Window);
        Desired = Page->PrepareForShow();
    }

    const UINT Dpi = ::GetDpiForWindow(WindowHandle);
    const float Scale = (float)Dpi / (float)USER_DEFAULT_SCREEN_DPI;
    const LONG_PTR Style = ::GetWindowLongPtrW(WindowHandle, GWL_STYLE);
    const LONG_PTR ExStyle = ::GetWindowLongPtrW(WindowHandle, GWL_EXSTYLE);

    // Width follows the longest content row (the tab bar or the widest
    // option row). The measured natural width already includes the tab
    // bar's own left/right padding (12px), so no extra margin is added:
    // the tab bar's left edge lands exactly at the dialog's left edge as
    // designed.
    int ClientW = (int)(Desired.Width * Scale + 0.5f);
    if (ClientW < (int)(480.0f * Scale))
    {
        ClientW = (int)(480.0f * Scale);
    }
    if (ClientW > 1400) ClientW = 1400;

    // Content-driven height: exact fit. ceil() so the client area always
    // covers the measured content by construction (a floor/round can leave
    // the viewport half a pixel shorter than the content, which makes the
    // ScrollViewer show a scrollbar). 95% of the screen is only an extreme
    // font-size guard.
    RECT ScreenRect = {};
    {
        HMONITOR MonitorHandle = ::MonitorFromWindow(
            WindowHandle, MONITOR_DEFAULTTONEAREST);
        if (MonitorHandle)
        {
            MONITORINFO MonitorInfo = { sizeof(MonitorInfo) };
            if (::GetMonitorInfoW(MonitorHandle, &MonitorInfo))
            {
                ScreenRect = MonitorInfo.rcMonitor;
            }
        }
    }
    int ClientH = (int)::ceil(Desired.Height * Scale);
    if (ScreenRect.bottom > ScreenRect.top)
    {
        // Settings dialog rule (user-approved): content-driven height,
        // capped at 75% of the screen.
        const int MaxClientH =
            (int)((double)(ScreenRect.bottom - ScreenRect.top) * 0.75 + 0.5);
        ClientH = (std::min)(ClientH, MaxClientH);
    }
    if (ClientH < (int)(360.0f * Scale))
    {
        ClientH = (int)(360.0f * Scale);
    }
    if (ClientH > 1400) ClientH = 1400;

    RECT WindowRect = { 0, 0, ClientW, ClientH };
    ::AdjustWindowRectEx(
        &WindowRect, (DWORD)Style, FALSE, (DWORD)ExStyle);
    const int WindowW = WindowRect.right - WindowRect.left;
    const int WindowH = WindowRect.bottom - WindowRect.top;

    // Center the default position on the owner window (fall back to the
    // nearest monitor's work area).
    RECT CenterRect = {};
    if (ParentWindowHandle)
    {
        ::GetWindowRect(ParentWindowHandle, &CenterRect);
    }
    if (CenterRect.right <= CenterRect.left ||
        CenterRect.bottom <= CenterRect.top)
    {
        HMONITOR MonitorHandle = ::MonitorFromWindow(
            WindowHandle, MONITOR_DEFAULTTONEAREST);
        if (MonitorHandle)
        {
            MONITORINFO MonitorInfo = {};
            MonitorInfo.cbSize = sizeof(MonitorInfo);
            if (::GetMonitorInfoW(MonitorHandle, &MonitorInfo))
            {
                CenterRect = MonitorInfo.rcWork;
            }
        }
    }
    const int PosX = CenterRect.left +
        ((CenterRect.right - CenterRect.left - WindowW) / 2);
    const int PosY = CenterRect.top +
        ((CenterRect.bottom - CenterRect.top - WindowH) / 2);
    RECT InitialRect = { PosX, PosY, PosX + WindowW, PosY + WindowH };

    // Restore the last size and position when it still intersects a
    // monitor. A disconnected monitor or an undersized saved rectangle
    // falls back to the centered default calculated above.
    if (Context->WindowRect.right > Context->WindowRect.left &&
        Context->WindowRect.bottom > Context->WindowRect.top &&
        ::MonitorFromRect(&Context->WindowRect, MONITOR_DEFAULTTONULL))
    {
        InitialRect = Context->WindowRect;
        const int MinWindowW = (Context->MinTrackW > 0)
            ? Context->MinTrackW
            : ::MulDiv(480, Dpi, USER_DEFAULT_SCREEN_DPI);
        const int MinWindowH = (Context->MinTrackH > 0)
            ? Context->MinTrackH
            : ::MulDiv(420, Dpi, USER_DEFAULT_SCREEN_DPI);
        if (InitialRect.right - InitialRect.left < MinWindowW)
        {
            InitialRect.right = InitialRect.left + MinWindowW;
        }
        if (InitialRect.bottom - InitialRect.top < MinWindowH)
        {
            InitialRect.bottom = InitialRect.top + MinWindowH;
        }

        HMONITOR Monitor = ::MonitorFromRect(
            &InitialRect, MONITOR_DEFAULTTONEAREST);
        MONITORINFO MonitorInfo = { sizeof(MonitorInfo) };
        if (Monitor && ::GetMonitorInfoW(Monitor, &MonitorInfo))
        {
            const int WorkWidth =
                MonitorInfo.rcWork.right - MonitorInfo.rcWork.left;
            const int WorkHeight =
                MonitorInfo.rcWork.bottom - MonitorInfo.rcWork.top;
            int SavedWidth = InitialRect.right - InitialRect.left;
            int SavedHeight = InitialRect.bottom - InitialRect.top;
            if (SavedWidth > WorkWidth)
            {
                SavedWidth = WorkWidth;
            }
            if (SavedHeight > WorkHeight)
            {
                SavedHeight = WorkHeight;
            }
            InitialRect.right = InitialRect.left + SavedWidth;
            InitialRect.bottom = InitialRect.top + SavedHeight;
            if (InitialRect.left < MonitorInfo.rcWork.left)
            {
                InitialRect.left = MonitorInfo.rcWork.left;
                InitialRect.right = InitialRect.left + SavedWidth;
            }
            if (InitialRect.top < MonitorInfo.rcWork.top)
            {
                InitialRect.top = MonitorInfo.rcWork.top;
                InitialRect.bottom = InitialRect.top + SavedHeight;
            }
            if (InitialRect.right > MonitorInfo.rcWork.right)
            {
                InitialRect.right = MonitorInfo.rcWork.right;
                InitialRect.left = InitialRect.right - SavedWidth;
            }
            if (InitialRect.bottom > MonitorInfo.rcWork.bottom)
            {
                InitialRect.bottom = MonitorInfo.rcWork.bottom;
                InitialRect.top = InitialRect.bottom - SavedHeight;
            }
        }
    }

    int Result = ::K7ModernShowXamlWindow(
        WindowHandle,
        ClientW,
        ClientH,
        ParentWindowHandle,
        &InitialRect);

    return Result;
}

EXTERN_C INT WINAPI K7ModernShowCompressOptionsDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_COMPRESS_OPTIONS_DIALOG_CONTEXT Context)
{
    if (!Context)
    {
        return -1;
    }

    SettingsDiagLog(L"[O1] K7ModernShowCompressOptionsDialog enter");

    HWND WindowHandle = ::K7ModernCreateXamlDialog(ParentWindowHandle);
    if (!WindowHandle)
    {
        SettingsDiagLog(L"[O1b] K7ModernCreateXamlDialog FAILED");
        return -1;
    }
    SettingsDiagLog(L"[O2] K7ModernCreateXamlDialog OK");

    // Esc at the Win32 level closes the dialog like the X button (a
    // cancel): the keyboard focus often sits on the Win32 host window, so
    // the XAML page's own KeyDown never sees the key.
    if (!::SetWindowSubclass(
        WindowHandle,
        [](_In_ HWND hWnd,
            _In_ UINT uMsg,
            _In_ WPARAM wParam,
            _In_ LPARAM lParam,
            _In_ UINT_PTR uIdSubclass,
            _In_ DWORD_PTR dwRefData) -> LRESULT
        {
            UNREFERENCED_PARAMETER(uIdSubclass);
            UNREFERENCED_PARAMETER(dwRefData);
            if (uMsg == WM_KEYDOWN && wParam == VK_ESCAPE)
            {
                ::PostMessageW(hWnd, WM_CLOSE, 0, 0);
                return 0;
            }
            if (uMsg == K7_COMPRESS_OPTIONS_REFIT2_MESSAGE)
            {
                // The window is visible and laid out; compensate the last
                // pixel of measured-vs-rendered height so the dialog never
                // opens with a scrollbar for fully visible content.
                winrt::DesktopWindowXamlSource XamlSource =
                    ::K7ModernGetDesktopWindowXamlSource(hWnd);
                if (XamlSource)
                {
                    if (auto Page = XamlSource.Content().try_as<
                        winrt::NanaZip::Modern::CompressOptionsPage>())
                    {
                        auto Impl = winrt::get_self<
                            winrt::NanaZip::Modern::implementation::
                                CompressOptionsPage>(Page);
                        const double Delta = Impl->GetScrollDelta();
                        if (Delta < 0.0)
                        {
                            const UINT Dpi =
                                ::GetDpiForWindow(hWnd);
                            const double Scale =
                                (double)Dpi / (double)USER_DEFAULT_SCREEN_DPI;
                            const int Add =
                                (int)::ceil(-Delta * Scale);
                            RECT rc = {};
                            ::GetWindowRect(hWnd, &rc);
                            // Keep the bottom edge, grow upward.
                            ::SetWindowPos(
                                hWnd,
                                nullptr,
                                rc.left,
                                rc.top - Add,
                                rc.right - rc.left,
                                rc.bottom - rc.top + Add,
                                SWP_NOZORDER | SWP_NOACTIVATE);
                            wchar_t buf[128];
                            swprintf_s(buf,
                                L"[O11] SelfHeal delta=%.1f add=%d",
                                Delta, Add);
                            SettingsDiagLog(buf);
                        }
                    }
                }
                return 0;
            }
            return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
        },
        1,
        reinterpret_cast<DWORD_PTR>(Context)))
    {
        ::DestroyWindow(WindowHandle);
        SettingsDiagLog(L"[O2b] SetWindowSubclass FAILED");
        return -1;
    }
    SettingsDiagLog(L"[O3] subclass OK");

    using Interface =
        winrt::NanaZip::Modern::CompressOptionsPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::CompressOptionsPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        Context);
    SettingsDiagLog(L"[O4] winrt::make OK");

    ::MileAllowNonClientDefaultDrawingForWindow(WindowHandle, FALSE);

    // NOTE: EnableWindow(Parent, FALSE) is intentionally disabled for now:
    // the parent here is the compress dialog's own XAML island window, and
    // disabling an island window while a second island (this dialog) runs
    // its message loop on the same thread deadlocks the shared XAML
    // pipeline (the settings dialog's parent is the plain FM main window,
    // which is why it never hit this). Real modal semantics can be
    // restored once the nested-island bring-up is stable.

    if (FAILED(::MileXamlSetXamlContentForContentWindow(
        WindowHandle,
        winrt::get_abi(Window))))
    {
        ::DestroyWindow(WindowHandle);
        SettingsDiagLog(L"[O4b] SetXamlContent FAILED");
        return -1;
    }
    SettingsDiagLog(L"[O5] SetXamlContent OK");

    // The content drives the default size (labels, check boxes and dialog
    // font already applied by the page); the host caps the height at 75%
    // of the monitor so the dialog never dominates the display (the page
    // scrolls if the content is taller).
    winrt::Windows::Foundation::Size Desired(420, 480);
    {
        auto Page = winrt::get_self<Implementation>(Window);
        Desired = Page->PrepareForShow();
    }
    SettingsDiagLog(L"[O6] PrepareForShow OK");
    const UINT Dpi = ::GetDpiForWindow(WindowHandle);
    const float Scale = (float)Dpi / (float)USER_DEFAULT_SCREEN_DPI;
    const LONG_PTR Style = ::GetWindowLongPtrW(WindowHandle, GWL_STYLE);
    const LONG_PTR ExStyle = ::GetWindowLongPtrW(WindowHandle, GWL_EXSTYLE);

    int ClientW = (int)(Desired.Width * Scale + 0.5f);
    if (ClientW < (int)(380.0f * Scale))
    {
        ClientW = (int)(380.0f * Scale);
    }
    if (ClientW > 1400) ClientW = 1400;

    RECT ScreenRect = {};
    {
        HMONITOR MonitorHandle = ::MonitorFromWindow(
            WindowHandle, MONITOR_DEFAULTTONEAREST);
        if (MonitorHandle)
        {
            MONITORINFO MonitorInfo = { sizeof(MonitorInfo) };
            if (::GetMonitorInfoW(MonitorHandle, &MonitorInfo))
            {
                ScreenRect = MonitorInfo.rcMonitor;
            }
        }
    }
    int ClientH = (int)::ceil(Desired.Height * Scale);
    // Content-driven height: exact fit (see the comment above). The 95%
    // screen cap only guards against an extreme font size pushing the
    // window past the screen bottom.
    const int MaxClientH =
        (ScreenRect.bottom > ScreenRect.top)
        ? (int)((double)(ScreenRect.bottom - ScreenRect.top) * 0.95 + 0.5)
        : 0;
    if (ScreenRect.bottom > ScreenRect.top)
    {
        ClientH = (std::min)(ClientH, MaxClientH);
    }
    if (ClientH < (int)(320.0f * Scale))
    {
        ClientH = (int)(320.0f * Scale);
    }
    if (ClientH > 1400) ClientH = 1400;

    {
        wchar_t buf[160];
        swprintf_s(buf,
            L"[O9] DesiredH=%.0f ScreenH=%d Max75=%d ClientH=%d",
            Desired.Height,
            (ScreenRect.bottom > ScreenRect.top)
                ? (ScreenRect.bottom - ScreenRect.top) : 0,
            MaxClientH,
            ClientH);
        SettingsDiagLog(buf);
    }

    // The page's first OnLoaded posts a refit request once the controls are
    // populated; consume it here, before ShowWindow, so the window is sized
    // from the real content height even if the first Measure ran before the
    // visual tree was complete. The window is not visible yet, so the resize
    // cannot bounce.
    MSG RefitMsg;
    while (::PeekMessageW(
        &RefitMsg,
        WindowHandle,
        K7_COMPRESS_OPTIONS_REFIT_MESSAGE,
        K7_COMPRESS_OPTIONS_REFIT_MESSAGE,
        PM_REMOVE))
    {
        winrt::Windows::Foundation::Size RefitDesired(420, 480);
        {
            auto Page = winrt::get_self<Implementation>(Window);
            RefitDesired = Page->RefreshSize();
        }
        ClientW = (int)(RefitDesired.Width * Scale + 0.5f);
        if (ClientW < (int)(380.0f * Scale))
        {
            ClientW = (int)(380.0f * Scale);
        }
        if (ClientW > 1400) ClientW = 1400;
        ClientH = (int)::ceil(RefitDesired.Height * Scale);
        if (ScreenRect.bottom > ScreenRect.top)
        {
            ClientH = (std::min)(ClientH, MaxClientH);
        }
        if (ClientH < (int)(320.0f * Scale))
        {
            ClientH = (int)(320.0f * Scale);
        }
        if (ClientH > 1400) ClientH = 1400;
        {
            wchar_t buf[160];
            swprintf_s(buf,
                L"[O10] Refit ClientW=%d ClientH=%d (DesiredH=%.0f)",
                ClientW, ClientH, RefitDesired.Height);
            SettingsDiagLog(buf);
        }
    }
    RECT WindowRect = { 0, 0, ClientW, ClientH };
    ::AdjustWindowRectEx(
        &WindowRect, (DWORD)Style, FALSE, (DWORD)ExStyle);
    const int WindowW = WindowRect.right - WindowRect.left;
    const int WindowH = WindowRect.bottom - WindowRect.top;

    // Center on the owner window (fall back to the nearest monitor's work
    // area).
    RECT CenterRect = {};
    if (ParentWindowHandle)
    {
        ::GetWindowRect(ParentWindowHandle, &CenterRect);
    }
    if (CenterRect.right <= CenterRect.left ||
        CenterRect.bottom <= CenterRect.top)
    {
        HMONITOR MonitorHandle = ::MonitorFromWindow(
            WindowHandle, MONITOR_DEFAULTTONEAREST);
        if (MonitorHandle)
        {
            MONITORINFO MonitorInfo = {};
            MonitorInfo.cbSize = sizeof(MonitorInfo);
            if (::GetMonitorInfoW(MonitorHandle, &MonitorInfo))
            {
                CenterRect = MonitorInfo.rcWork;
            }
        }
    }
    const int PosX = CenterRect.left +
        ((CenterRect.right - CenterRect.left - WindowW) / 2);
    const int PosY = CenterRect.top +
        ((CenterRect.bottom - CenterRect.top - WindowH) / 2);
    RECT InitialRect = { PosX, PosY, PosX + WindowW, PosY + WindowH };

    SettingsDiagLog(L"[O7] enter message loop");
    int Result = ::K7ModernShowXamlWindow(
        WindowHandle,
        ClientW,
        ClientH,
        ParentWindowHandle,
        &InitialRect);
    SettingsDiagLog(L"[O8] message loop exited");

    return Result;
}

// Split-dialog host function.
EXTERN_C INT WINAPI K7ModernShowSplitDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_SPLIT_DIALOG_CONTEXT Context)
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
        winrt::NanaZip::Modern::SplitPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::SplitPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        Context);

    ::MileAllowNonClientDefaultDrawingForWindow(WindowHandle, FALSE);

    // The split dialog adapts its size to the content, but the user should
    // still be able to enlarge it (e.g. with a larger dialog font size), so
    // give it a resize border and the maximize/minimize box.
    {
        LONG_PTR Style = ::GetWindowLongPtrW(WindowHandle, GWL_STYLE);
        Style |= WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
        ::SetWindowLongPtrW(WindowHandle, GWL_STYLE, Style);
    }

    // Enforce a minimum window size so that dragging the border can never
    // shrink the dialog below its measured content (which would clip the
    // controls). The XAML page writes MinTrackW/MinTrackH during
    // PrepareForShow; this subclass reads them in WM_GETMINMAXINFO.
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
        else if (uMsg == WM_KEYDOWN && wParam == VK_ESCAPE)
        {
            // Esc closes the dialog like the X button (a cancel: the page's
            // OnUnloaded clears the OK flag).
            ::PostMessageW(hWnd, WM_CLOSE, 0, 0);
            return 0;
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
    winrt::Windows::Foundation::Size Desired(360, 140);
    {
        auto Self = winrt::get_self<Implementation>(Window);
        Desired = Self->PrepareForShow();
    }

    const UINT Dpi = ::GetDpiForWindow(WindowHandle);
    const float Scale = (float)Dpi / (float)USER_DEFAULT_SCREEN_DPI;

    // Default size: content plus a tiny margin. The content rows are Auto,
    // so Desired already is the full height; keeping the window close to the
    // content avoids empty space both below the volume row and below the
    // buttons (the Grid is top-aligned when the window is taller).
    int ClientW = (int)((Desired.Width + 48.0f) * Scale + 0.5f);
    int ClientH = (int)((Desired.Height + 4.0f) * Scale + 0.5f);
    if (ClientW < 400) ClientW = 400;
    if (ClientH < 110) ClientH = 110;

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

    RECT rc = { 0, 0, ClientW, ClientH };
    {
        const LONG_PTR Style = ::GetWindowLongPtrW(WindowHandle, GWL_STYLE);
        const LONG_PTR ExStyle = ::GetWindowLongPtrW(WindowHandle, GWL_EXSTYLE);
        ::AdjustWindowRectEx(&rc, (DWORD)Style, FALSE, (DWORD)ExStyle);
    }

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

EXTERN_C INT WINAPI K7ModernShowPasswordDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_PASSWORD_DIALOG_CONTEXT Context)
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
        winrt::NanaZip::Modern::PasswordPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::PasswordPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        Context);

    ::MileAllowNonClientDefaultDrawingForWindow(WindowHandle, FALSE);

    // Enforce a minimum window size so that dragging the border can never
    // shrink the dialog below its measured content (which would clip the
    // controls). The XAML page writes MinTrackW/MinTrackH during
    // PrepareForShow; this subclass reads them in WM_GETMINMAXINFO.
    if (!::SetWindowSubclass(
        WindowHandle,
        [](_In_ HWND hWnd,
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
        else if (uMsg == WM_KEYDOWN && wParam == VK_ESCAPE)
        {
            // Esc closes the dialog like the X button (a cancel: the page's
            // OnUnloaded clears the OK flag).
            ::PostMessageW(hWnd, WM_CLOSE, 0, 0);
            return 0;
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
    winrt::Windows::Foundation::Size Desired(320, 120);
    {
        auto Self = winrt::get_self<Implementation>(Window);
        Desired = Self->PrepareForShow();
    }

    const UINT Dpi = ::GetDpiForWindow(WindowHandle);
    const float Scale = (float)Dpi / (float)USER_DEFAULT_SCREEN_DPI;

    // Default size: content plus a tiny margin. The content rows are Auto,
    // so Desired already is the full height; keeping the window close to the
    // content avoids empty space both below the password row and below the
    // buttons.
    int ClientW = (int)((Desired.Width + 48.0f) * Scale + 0.5f);
    int ClientH = (int)((Desired.Height + 4.0f) * Scale + 0.5f);
    if (ClientW < 300) ClientW = 300;
    if (ClientH < 120) ClientH = 120;

    // Never exceed the work area (minus a small margin), so the dialog
    // cannot open off-screen; 900 is a hard backstop.
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

    RECT rc = { 0, 0, ClientW, ClientH };
    {
        const LONG_PTR Style = ::GetWindowLongPtrW(WindowHandle, GWL_STYLE);
        const LONG_PTR ExStyle = ::GetWindowLongPtrW(WindowHandle, GWL_EXSTYLE);
        ::AdjustWindowRectEx(&rc, (DWORD)Style, FALSE, (DWORD)ExStyle);
    }

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

EXTERN_C INT WINAPI K7ModernShowComboDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_COMBO_DIALOG_CONTEXT Context)
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
        winrt::NanaZip::Modern::ComboPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::ComboPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        Context);

    ::MileAllowNonClientDefaultDrawingForWindow(WindowHandle, FALSE);

    // Enforce a minimum window size so that dragging the border can never
    // shrink the dialog below its measured content (which would clip the
    // controls). The XAML page writes MinTrackW/MinTrackH during
    // PrepareForShow; this subclass reads them in WM_GETMINMAXINFO.
    if (!::SetWindowSubclass(
        WindowHandle,
        [](_In_ HWND hWnd,
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
        else if (uMsg == WM_KEYDOWN && wParam == VK_ESCAPE)
        {
            // Esc closes the dialog like the X button (a cancel: the page's
            // OnUnloaded clears the OK flag).
            ::PostMessageW(hWnd, WM_CLOSE, 0, 0);
            return 0;
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
    winrt::Windows::Foundation::Size Desired(360, 120);
    {
        auto Self = winrt::get_self<Implementation>(Window);
        Desired = Self->PrepareForShow();
    }

    const UINT Dpi = ::GetDpiForWindow(WindowHandle);
    const float Scale = (float)Dpi / (float)USER_DEFAULT_SCREEN_DPI;

    // Default size: content plus a tiny margin. The content rows are Auto,
    // so Desired already is the full height; keeping the window close to the
    // content avoids empty space both below the prompt row and below the
    // buttons.
    int ClientW = (int)((Desired.Width + 48.0f) * Scale + 0.5f);
    int ClientH = (int)((Desired.Height + 4.0f) * Scale + 0.5f);
    if (ClientW < 300) ClientW = 300;
    if (ClientH < 120) ClientH = 120;

    // Never exceed the work area (minus a small margin), so the dialog
    // cannot open off-screen; 1400 is a hard backstop.
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

    RECT rc = { 0, 0, ClientW, ClientH };
    {
        const LONG_PTR Style = ::GetWindowLongPtrW(WindowHandle, GWL_STYLE);
        const LONG_PTR ExStyle = ::GetWindowLongPtrW(WindowHandle, GWL_EXSTYLE);
        ::AdjustWindowRectEx(&rc, (DWORD)Style, FALSE, (DWORD)ExStyle);
    }

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
EXTERN_C INT WINAPI K7ModernShowLinkDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_LINK_DIALOG_CONTEXT Context)
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
        winrt::NanaZip::Modern::LinkPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::LinkPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        Context);

    ::MileAllowNonClientDefaultDrawingForWindow(WindowHandle, FALSE);

    // Enforce a minimum window size so that dragging the border can never
    // shrink the dialog below its measured content (which would clip the
    // controls). The XAML page writes MinTrackW/MinTrackH during
    // PrepareForShow; this subclass reads them in WM_GETMINMAXINFO.
    if (!::SetWindowSubclass(
        WindowHandle,
        [](_In_ HWND hWnd,
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
        else if (uMsg == WM_KEYDOWN && wParam == VK_ESCAPE)
        {
            // Esc closes the dialog like the X button (a cancel: the page's
            // OnUnloaded clears the OK flag).
            ::PostMessageW(hWnd, WM_CLOSE, 0, 0);
            return 0;
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
    winrt::Windows::Foundation::Size Desired(420, 320);
    {
        auto Self = winrt::get_self<Implementation>(Window);
        Desired = Self->PrepareForShow();
    }

    const UINT Dpi = ::GetDpiForWindow(WindowHandle);
    const float Scale = (float)Dpi / (float)USER_DEFAULT_SCREEN_DPI;

    // Default size: content plus a tiny margin. The content rows are Auto,
    // so Desired already is the full height; keeping the window close to the
    // content avoids empty space both below the prompt row and below the
    // buttons.
    int ClientW = (int)((Desired.Width + 48.0f) * Scale + 0.5f);
    int ClientH = (int)((Desired.Height + 4.0f) * Scale + 0.5f);
    if (ClientW < 420) ClientW = 420;
    if (ClientH < 300) ClientH = 300;

    // Never exceed the work area (minus a small margin), so the dialog
    // cannot open off-screen; 1400 is a hard backstop.
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

    RECT rc = { 0, 0, ClientW, ClientH };
    {
        const LONG_PTR Style = ::GetWindowLongPtrW(WindowHandle, GWL_STYLE);
        const LONG_PTR ExStyle = ::GetWindowLongPtrW(WindowHandle, GWL_EXSTYLE);
        ::AdjustWindowRectEx(&rc, (DWORD)Style, FALSE, (DWORD)ExStyle);
    }

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
EXTERN_C INT WINAPI K7ModernShowBenchmarkDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_BENCHMARK_DIALOG_CONTEXT Context,
    _In_ SUBCLASSPROC WindowSubclassHandler,
    _In_ LPVOID WindowSubclassContext)
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

    ::MileAllowNonClientDefaultDrawingForWindow(WindowHandle, FALSE);

    using Interface =
        winrt::NanaZip::Modern::BenchmarkPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::BenchmarkPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        Context);

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

    // Enforce a minimum window size so that dragging the border can never
    // shrink the dialog below its measured content (which would clip the
    // controls). The XAML page writes MinTrackW/MinTrackH during
    // PrepareForShow; this subclass reads them in WM_GETMINMAXINFO.
    if (!::SetWindowSubclass(
        WindowHandle,
        [](_In_ HWND hWnd,
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
        2,
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
    winrt::Windows::Foundation::Size Desired(560, 480);
    {
        auto Self = winrt::get_self<Implementation>(Window);
        Desired = Self->PrepareForShow();
    }

    const UINT Dpi = ::GetDpiForWindow(WindowHandle);
    const float Scale = (float)Dpi / (float)USER_DEFAULT_SCREEN_DPI;

    // Default size: content plus a small margin. The content is scrollable,
    // so a generous default height is fine.
    int ClientW = (int)((Desired.Width + 48.0f) * Scale + 0.5f);
    int ClientH = (int)((Desired.Height + 4.0f) * Scale + 0.5f);
    if (ClientW < 520) ClientW = 520;
    if (ClientH < 420) ClientH = 420;

    // Never exceed the work area (minus a small margin), so the dialog
    // cannot open off-screen; 1400 is a hard backstop.
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

    RECT rc = { 0, 0, ClientW, ClientH };
    {
        const LONG_PTR Style = ::GetWindowLongPtrW(WindowHandle, GWL_STYLE);
        const LONG_PTR ExStyle = ::GetWindowLongPtrW(WindowHandle, GWL_EXSTYLE);
        ::AdjustWindowRectEx(&rc, (DWORD)Style, FALSE, (DWORD)ExStyle);
    }

    // Center the dialog on the parent window (or the nearest monitor), or
    // restore the last position when one was persisted (clamped to the
    // work area so the dialog cannot open off-screen). ParentRect was
    // already resolved above for the work-area limit.
    const int WindowW = rc.right - rc.left;
    const int WindowH = rc.bottom - rc.top;
    int PosX;
    int PosY;
    if (Context->HasInitialPos)
    {
        PosX = (int)Context->InitialX;
        PosY = (int)Context->InitialY;
        const int WorkLeft = ParentRect.left;
        const int WorkTop = ParentRect.top;
        const int WorkRight = ParentRect.right;
        const int WorkBottom = ParentRect.bottom;
        if (PosX < WorkLeft) PosX = WorkLeft;
        if (PosY < WorkTop) PosY = WorkTop;
        if (PosX + WindowW > WorkRight) PosX = WorkRight - WindowW;
        if (PosY + WindowH > WorkBottom) PosY = WorkBottom - WindowH;
    }
    else
    {
        PosX = ParentRect.left +
            ((ParentRect.right - ParentRect.left - WindowW) / 2);
        PosY = ParentRect.top +
            ((ParentRect.bottom - ParentRect.top - WindowH) / 2);
    }
    RECT InitialRect = { PosX, PosY, PosX + WindowW, PosY + WindowH };

    int Result = ::K7ModernShowXamlWindow(
        WindowHandle,
        ClientW,
        ClientH,
        ParentWindowHandle,
        &InitialRect);

    if (ParentWindowHandle)
    {
        ::EnableWindow(ParentWindowHandle, TRUE);
    }

    return Result;
}

EXTERN_C VOID WINAPI K7ModernUpdateBenchmarkStatus(
    _In_ HWND WindowHandle,
    _In_ PK7_BENCHMARK_STATUS Status)
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
        winrt::NanaZip::Modern::BenchmarkPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::BenchmarkPage;
    Interface InstanceObject = XamlSource.Content().as<Interface>();
    if (!InstanceObject)
    {
        return;
    }
    winrt::get_self<Implementation>(InstanceObject)->ApplyStatus(Status);
}
