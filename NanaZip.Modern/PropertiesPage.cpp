#include "pch.h"
#include "PropertiesPage.h"
#if __has_include("PropertiesPage.g.cpp")
#include "PropertiesPage.g.cpp"
#endif
#if __has_include("PropertyDetailItem.g.cpp")
#include "PropertyDetailItem.g.cpp"
#endif
#if __has_include("SignatureItem.g.cpp")
#include "SignatureItem.g.cpp"
#endif
#if __has_include("SecurityItem.g.cpp")
#include "SecurityItem.g.cpp"
#endif
#if __has_include("VersionItem.g.cpp")
#include "VersionItem.g.cpp"
#endif
#if __has_include("CustomPropertyItem.g.cpp")
#include "CustomPropertyItem.g.cpp"
#endif

#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.UI.Core.h>

#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <propsys.h>
#include <propkey.h>
#include <propvarutil.h>
#include <sddl.h>
#include <wincodec.h>
#include <wintrust.h>
#include <softpub.h>
#include <wincrypt.h>
#include <aclapi.h>
#include <wbemidl.h>
#include <shlwapi.h>
#include <aclui.h>

#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <limits>

namespace winrt::NanaZip::Modern::implementation
{
    // =====================================================================
    // Diagnostics. The properties dialog may block on shell / WMI / crypto
    // calls, so every phase writes a timestamped line to
    // %TEMP%\k7properties_diag.log. The last line before a hang identifies
    // the blocking call.
    // =====================================================================

    static void PropertiesDiagLog(LPCWSTR Format, ...)
    {
        wchar_t Buffer[1024] = {};
        va_list Args;
        va_start(Args, Format);
        (void)_vsnwprintf_s(
            Buffer,
            1024,
            _TRUNCATE,
            Format,
            Args);
        va_end(Args);

        SYSTEMTIME Time = {};
        ::GetLocalTime(&Time);
        const DWORD ThreadId = ::GetCurrentThreadId();

        wchar_t TempPath[MAX_PATH] = {};
        DWORD TempLen = ::GetEnvironmentVariableW(
            L"TEMP",
            TempPath,
            MAX_PATH);
        if (TempLen == 0 || TempLen >= MAX_PATH)
        {
            (void)wcscpy_s(TempPath, L"C:\\Windows\\Temp");
        }
        std::wstring LogPath = std::wstring(TempPath) +
            L"\\k7properties_diag.log";

        FILE* File = nullptr;
        if (_wfopen_s(&File, LogPath.c_str(), L"a, ccs=UTF-8") == 0 && File)
        {
            (void)fwprintf(
                File,
                L"[%02u:%02u:%02u.%03u T%u] %s\n",
                Time.wHour,
                Time.wMinute,
                Time.wSecond,
                Time.wMilliseconds,
                ThreadId,
                Buffer);
            (void)fclose(File);
        }
        ::OutputDebugStringW(Buffer);
    }

    // =====================================================================
    // List item types (declared in PropertiesPage.idl)
    // =====================================================================

    struct PropertyDetailItem : PropertyDetailItemT<PropertyDetailItem>
    {
        PropertyDetailItem() = default;
        PropertyDetailItem(
            winrt::hstring const& Name,
            winrt::hstring const& Value) :
            m_Name(Name),
            m_Value(Value)
        {
        }

        winrt::hstring Name() { return m_Name; }
        void Name(winrt::hstring const& value) { m_Name = value; }
        winrt::hstring Value() { return m_Value; }
        void Value(winrt::hstring const& value) { m_Value = value; }

    private:
        winrt::hstring m_Name;
        winrt::hstring m_Value;
    };

    struct SignatureItem : SignatureItemT<SignatureItem>
    {
        SignatureItem() = default;
        SignatureItem(
            winrt::hstring const& Signer,
            winrt::hstring const& Status,
            winrt::hstring const& Detail) :
            m_Signer(Signer),
            m_Status(Status),
            m_Detail(Detail)
        {
        }

        winrt::hstring Signer() { return m_Signer; }
        void Signer(winrt::hstring const& value) { m_Signer = value; }
        winrt::hstring Status() { return m_Status; }
        void Status(winrt::hstring const& value) { m_Status = value; }
        winrt::hstring Detail() { return m_Detail; }
        void Detail(winrt::hstring const& value) { m_Detail = value; }

    private:
        winrt::hstring m_Signer;
        winrt::hstring m_Status;
        winrt::hstring m_Detail;
    };

    struct SecurityItem : SecurityItemT<SecurityItem>
    {
        SecurityItem() = default;
        SecurityItem(
            winrt::hstring const& Name,
            winrt::hstring const& Access) :
            m_Name(Name),
            m_Access(Access)
        {
        }

        winrt::hstring Name() { return m_Name; }
        void Name(winrt::hstring const& value) { m_Name = value; }
        winrt::hstring Access() { return m_Access; }
        void Access(winrt::hstring const& value) { m_Access = value; }

    private:
        winrt::hstring m_Name;
        winrt::hstring m_Access;
    };

    struct VersionItem : VersionItemT<VersionItem>
    {
        VersionItem() = default;
        VersionItem(
            winrt::hstring const& Time,
            winrt::hstring const& Location) :
            m_Time(Time),
            m_Location(Location)
        {
        }

        winrt::hstring Time() { return m_Time; }
        void Time(winrt::hstring const& value) { m_Time = value; }
        winrt::hstring Location() { return m_Location; }
        void Location(winrt::hstring const& value) { m_Location = value; }

    private:
        winrt::hstring m_Time;
        winrt::hstring m_Location;
    };

    struct CustomPropertyItem : CustomPropertyItemT<CustomPropertyItem>
    {
        CustomPropertyItem() = default;
        CustomPropertyItem(
            winrt::hstring const& Name,
            winrt::hstring const& Type,
            winrt::hstring const& Value) :
            m_Name(Name),
            m_Type(Type),
            m_Value(Value)
        {
        }

        winrt::hstring Name() { return m_Name; }
        void Name(winrt::hstring const& value) { m_Name = value; }
        winrt::hstring Type() { return m_Type; }
        void Type(winrt::hstring const& value) { m_Type = value; }
        winrt::hstring Value() { return m_Value; }
        void Value(winrt::hstring const& value) { m_Value = value; }

    private:
        winrt::hstring m_Name;
        winrt::hstring m_Type;
        winrt::hstring m_Value;
    };

    // =====================================================================
    // Static helpers
    // =====================================================================

    static winrt::hstring ReadStringProp(
        IPropertyStore* Store,
        REFPROPERTYKEY Key)
    {
        winrt::hstring Result;
        if (!Store)
        {
            return Result;
        }
        PROPVARIANT Value;
        ::PropVariantInit(&Value);
        if (SUCCEEDED(Store->GetValue(Key, &Value)))
        {
            PWSTR Text = nullptr;
            if (SUCCEEDED(::PropVariantToStringAlloc(Value, &Text)))
            {
                Result = Text;
                ::CoTaskMemFree(Text);
            }
            ::PropVariantClear(&Value);
        }
        return Result;
    }

    static UINT64 ReadUInt64Prop(
        IPropertyStore* Store,
        REFPROPERTYKEY Key)
    {
        UINT64 Result = 0;
        if (!Store)
        {
            return Result;
        }
        PROPVARIANT Value;
        ::PropVariantInit(&Value);
        if (SUCCEEDED(Store->GetValue(Key, &Value)))
        {
            (void)::PropVariantToUInt64(Value, &Result);
            ::PropVariantClear(&Value);
        }
        return Result;
    }

    static UINT32 ReadUInt32Prop(
        IPropertyStore* Store,
        REFPROPERTYKEY Key)
    {
        UINT32 Result = 0;
        if (!Store)
        {
            return Result;
        }
        PROPVARIANT Value;
        ::PropVariantInit(&Value);
        if (SUCCEEDED(Store->GetValue(Key, &Value)))
        {
            (void)::PropVariantToUInt32(Value, &Result);
            ::PropVariantClear(&Value);
        }
        return Result;
    }

    static FILETIME ReadFileTimeProp(
        IPropertyStore* Store,
        REFPROPERTYKEY Key)
    {
        FILETIME Result = {};
        if (!Store)
        {
            return Result;
        }
        PROPVARIANT Value;
        ::PropVariantInit(&Value);
        if (SUCCEEDED(Store->GetValue(Key, &Value)))
        {
            (void)::PropVariantToFileTime(Value, PSTF_UTC, &Result);
            ::PropVariantClear(&Value);
        }
        return Result;
    }

    static std::wstring SidToName(PSID Sid)
    {
        std::wstring Result = L"";
        if (!Sid)
        {
            return Result;
        }
        WCHAR Name[256] = {};
        WCHAR Domain[256] = {};
        DWORD NameSize = 256;
        DWORD DomainSize = 256;
        SID_NAME_USE Use;
        if (::LookupAccountSidW(
            nullptr,
            Sid,
            Name,
            &NameSize,
            Domain,
            &DomainSize,
            &Use))
        {
            if (Domain[0] != L'\0')
            {
                Result = Domain;
                Result += L"\\";
            }
            Result += Name;
        }
        else
        {
            // Fall back to the SID string.
            LPWSTR SidString = nullptr;
            if (::ConvertSidToStringSidW(Sid, &SidString))
            {
                Result = SidString;
                ::LocalFree(SidString);
            }
        }
        return Result;
    }

    static std::wstring MapAccessMask(DWORD Mask)
    {
        // Order matters: the generic and aggregate bits are tested first.
        const DWORD kFileAllAccess = FILE_ALL_ACCESS & 0x1FF;
        if ((Mask & kFileAllAccess) == kFileAllAccess)
        {
            return L"完全控制";
        }
        const DWORD kModify = FILE_GENERIC_READ | FILE_GENERIC_WRITE |
            FILE_GENERIC_EXECUTE | DELETE;
        if ((Mask & kModify) == kModify)
        {
            return L"修改";
        }
        const DWORD kReadExecute = FILE_GENERIC_READ | FILE_GENERIC_EXECUTE;
        if ((Mask & kReadExecute) == kReadExecute)
        {
            return L"读取和执行";
        }
        if ((Mask & FILE_GENERIC_READ) == FILE_GENERIC_READ)
        {
            return L"读取";
        }
        if ((Mask & FILE_GENERIC_WRITE) == FILE_GENERIC_WRITE)
        {
            return L"写入";
        }
        std::wstring Parts;
        if (Mask & FILE_READ_DATA)
        {
            Parts += L"读取, ";
        }
        if (Mask & FILE_WRITE_DATA)
        {
            Parts += L"写入, ";
        }
        if (Mask & FILE_EXECUTE)
        {
            Parts += L"执行, ";
        }
        if (Mask & DELETE)
        {
            Parts += L"删除, ";
        }
        if (Mask & WRITE_DAC)
        {
            Parts += L"更改权限, ";
        }
        if (!Parts.empty())
        {
            Parts.resize(Parts.size() - 2);
            return Parts;
        }
        return L"特殊权限";
    }

    static std::wstring GetDisplayNameForPropertyKey(REFPROPERTYKEY Key)
    {
        std::wstring Result;
        PWSTR Name = nullptr;
        if (SUCCEEDED(::PSGetNameFromPropertyKey(Key, &Name)))
        {
            Result = Name;
            ::CoTaskMemFree(Name);
        }
        return Result;
    }

    // =====================================================================
    // PropertiesPage
    // =====================================================================

    PropertiesPage::PropertiesPage(
        _In_ HWND WindowHandle,
        _In_ PK7_FILE_PROPERTIES_DIALOG_CONTEXT Context,
        _In_ UINT32 FontSizeDialog) :
        MinTrackW(0),
        MinTrackH(0),
        m_WindowHandle(WindowHandle),
        m_FontSizeDialog(FontSizeDialog),
        m_SingleSelection(false),
        m_CurrentTab(0),
        m_FileAttributes(0),
        m_GeneralSizeBytes(0),
        m_AttributeDirty(false),
        m_CustomDirty(false),
        m_CustomEditMode(0),
        m_CustomEditIndex(0),
        m_VersionsLoaded(false),
        m_SignatureLoaded(false),
        m_Details(winrt::single_threaded_vector<winrt::Windows::Foundation::IInspectable>()),
        m_DetailsFiltered(winrt::single_threaded_vector<winrt::Windows::Foundation::IInspectable>()),
        m_Signatures(winrt::single_threaded_vector<winrt::Windows::Foundation::IInspectable>()),
        m_SecurityItems(winrt::single_threaded_vector<winrt::Windows::Foundation::IInspectable>()),
        m_VersionItems(winrt::single_threaded_vector<winrt::Windows::Foundation::IInspectable>()),
        m_CustomItems(winrt::single_threaded_vector<winrt::Windows::Foundation::IInspectable>())
    {
        if (Context)
        {
            UINT32 Count = Context->PathCount;
            if (Count > K7_MODERN_FILE_PROPERTIES_MAX_PATHS)
            {
                Count = K7_MODERN_FILE_PROPERTIES_MAX_PATHS;
            }
            for (UINT32 i = 0; i < Count; i++)
            {
                m_Paths.push_back(Context->Paths[i]);
            }
        }
        m_SingleSelection = (m_Paths.size() == 1);
        PropertiesDiagLog(L"M01 constructor: %u paths, single=%d, first=%ls",
            (UINT32)m_Paths.size(),
            m_SingleSelection ? 1 : 0,
            m_Paths.empty() ? L"" : m_Paths[0].c_str());
    }

    void PropertiesPage::InitializeComponent()
    {
        PropertiesPageT::InitializeComponent();
    }

    winrt::hstring PropertiesPage::Res(
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

    winrt::hstring PropertiesPage::FormatTime(FILETIME const& Time)
    {
        if (Time.dwLowDateTime == 0 && Time.dwHighDateTime == 0)
        {
            return winrt::hstring(L"-");
        }
        FILETIME Local = {};
        if (!::FileTimeToLocalFileTime(&Time, &Local))
        {
            return winrt::hstring(L"-");
        }
        SYSTEMTIME SystemTime = {};
        if (!::FileTimeToSystemTime(&Local, &SystemTime))
        {
            return winrt::hstring(L"-");
        }
        wchar_t Buffer[64] = {};
        (void)swprintf_s(
            Buffer,
            L"%d/%d/%d %02d:%02d",
            SystemTime.wYear,
            SystemTime.wMonth,
            SystemTime.wDay,
            SystemTime.wHour,
            SystemTime.wMinute);
        return winrt::hstring(Buffer);
    }

    winrt::hstring PropertiesPage::FormatSize(UINT64 Size)
    {
        wchar_t Buffer[64] = {};
        if (::StrFormatByteSizeW(static_cast<LONGLONG>(Size), Buffer, 64))
        {
            return winrt::hstring(Buffer);
        }
        return winrt::hstring(L"-");
    }

    void PropertiesPage::ApplyDialogFont(UINT32 Pt)
    {
        double FontSizePx = (Pt == 0) ? 0.0 : (double)Pt * 96.0 / 72.0;
        ApplyFontToTree(*this, FontSizePx);
    }

    void PropertiesPage::ApplyFontToTree(
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

    winrt::Windows::Foundation::Size PropertiesPage::PrepareForShow()
    {
        PropertiesDiagLog(L"M02 PrepareForShow enter");
        ::SetWindowTextW(this->m_WindowHandle, this->Res(6600, L"属性").c_str());

        // Tab labels. The legacy resource map has no property sheet tab
        // names, so the fallback texts carry the localized strings.
        this->TabGeneralButton().Content(winrt::box_value(winrt::hstring(L"常规")));
        this->TabDetailsButton().Content(winrt::box_value(winrt::hstring(L"详细信息")));
        this->TabSignatureButton().Content(winrt::box_value(winrt::hstring(L"数字签名")));
        this->TabSecurityButton().Content(winrt::box_value(winrt::hstring(L"安全")));
        this->TabVersionsButton().Content(winrt::box_value(winrt::hstring(L"以前的版本")));
        this->TabCustomButton().Content(winrt::box_value(winrt::hstring(L"自定义")));
        this->ApplyButton().Content(winrt::box_value(winrt::hstring(L"应用")));
        PropertiesDiagLog(L"M03 tab labels set");

        this->CollectData();
        PropertiesDiagLog(L"M11 CollectData done");

        // General tab
        if (this->m_SingleSelection)
        {
            this->HeaderNameText().Text(this->GetHeaderName());
            this->HeaderTypeText().Text(this->GetHeaderType());
            this->GeneralTypeValueText().Text(this->m_GeneralType);
            this->GeneralLocationValueText().Text(this->m_GeneralLocation);
            this->GeneralSizeValueText().Text(this->m_GeneralSize);
            this->GeneralAllocSizeValueText().Text(this->m_GeneralAllocSize);
            this->GeneralCreatedValueText().Text(this->m_GeneralCreated);
            this->GeneralModifiedValueText().Text(this->m_GeneralModified);
            this->GeneralAccessedValueText().Text(this->m_GeneralAccessed);
            this->GeneralReadOnlyCheck().IsChecked(
                (this->m_FileAttributes & FILE_ATTRIBUTE_READONLY) != 0);
            this->GeneralHiddenCheck().IsChecked(
                (this->m_FileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0);
        }
        else
        {
            this->HeaderNameText().Text(this->GetHeaderName());
            this->HeaderTypeText().Text(this->GetHeaderType());
            this->GeneralTypeValueText().Text(this->GetHeaderType());
            this->GeneralLocationValueText().Text(L"-");
            this->GeneralSizeValueText().Text(
                this->m_SingleSelection
                    ? this->m_GeneralSize
                    : this->FormatSize(this->m_GeneralSizeBytes));
            this->GeneralCreatedValueText().Text(L"-");
            this->GeneralModifiedValueText().Text(L"-");
            this->GeneralAccessedValueText().Text(L"-");
            this->GeneralReadOnlyCheck().IsEnabled(false);
            this->GeneralHiddenCheck().IsEnabled(false);
        }
        PropertiesDiagLog(L"M12 general tab filled");

        if (this->m_Paths.size() == 1)
        {
            this->LoadHeaderIcon(this->m_Paths[0]);
        }
        PropertiesDiagLog(L"M13 icon loaded");

        // Details tab
        this->RefreshDetailsFilter();
        PropertiesDiagLog(L"M14 details tab filled (%u rows)",
            this->m_DetailsFiltered.Size());

        // Signature tab
        this->SignatureStatusText().Text(this->m_SingleSelection
            ? winrt::hstring(L"正在验证数字签名...")
            : winrt::hstring(L"多选时不可用。"));
        this->SignatureListView().ItemsSource(this->m_Signatures);
        if (!this->m_SingleSelection)
        {
            this->SignatureListView().IsEnabled(false);
        }
        PropertiesDiagLog(L"M15 signature tab init");

        // Security tab
        this->SecurityOwnerText().Text(this->m_SecurityOwnerText);
        this->SecurityListView().ItemsSource(this->m_SecurityItems);
        if (!this->m_SingleSelection)
        {
            this->SecurityListView().IsEnabled(false);
            this->SecurityAdvancedButton().IsEnabled(false);
        }
        PropertiesDiagLog(L"M16 security tab filled (%u rows)",
            this->m_SecurityItems.Size());

        // Previous versions tab
        this->VersionsStatusText().Text(this->m_SingleSelection
            ? winrt::hstring(L"正在查找以前的版本...")
            : winrt::hstring(L"多选时不可用。"));
        this->VersionsListView().ItemsSource(this->m_VersionItems);
        if (!this->m_SingleSelection)
        {
            this->VersionsListView().IsEnabled(false);
        }
        PropertiesDiagLog(L"M17 versions tab init");

        // Custom tab
        this->CustomStatusText().Text(this->m_CustomItems.Size() == 0
            ? winrt::hstring(this->m_SingleSelection
                ? L"没有自定义属性。"
                : L"多选时不可用。")
            : winrt::hstring(L"自定义属性："));
        this->CustomListView().ItemsSource(this->m_CustomItems);
        if (!this->m_SingleSelection)
        {
            this->CustomListView().IsEnabled(false);
            this->AddCustomButton().IsEnabled(false);
        }
        this->RefreshCustomButtons();
        PropertiesDiagLog(L"M18 custom tab filled (%u rows)",
            this->m_CustomItems.Size());

        // Header icon is loaded in LoadHeaderIcon for single selection.
        this->ApplyDialogFont(this->m_FontSizeDialog);
        PropertiesDiagLog(L"M19 dialog font applied");

        // Keep the General tab as a borderless two-column table. Measure all
        // labels after applying the dialog font, then assign the same width
        // to every label so every value starts on one vertical grid line.
        winrt::Windows::Foundation::Size Inf(100000.0f, 100000.0f);
        double MaxGeneralLabelW = 0.0;
        for (auto const& Label : std::vector<winrt::Windows::UI::Xaml::Controls::TextBlock>{
                this->GeneralTypeLabel(),
                this->GeneralLocationLabel(),
                this->GeneralSizeLabel(),
                this->GeneralAllocSizeLabel(),
                this->GeneralCreatedLabel(),
                this->GeneralModifiedLabel(),
                this->GeneralAccessedLabel() })
        {
            Label.Measure(Inf);
            MaxGeneralLabelW = (std::max)(
                MaxGeneralLabelW,
                (double)Label.DesiredSize().Width);
        }
        if (MaxGeneralLabelW > 0.0)
        {
            for (auto const& Label : std::vector<winrt::Windows::UI::Xaml::Controls::TextBlock>{
                    this->GeneralTypeLabel(),
                    this->GeneralLocationLabel(),
                    this->GeneralSizeLabel(),
                    this->GeneralAllocSizeLabel(),
                    this->GeneralCreatedLabel(),
                    this->GeneralModifiedLabel(),
                    this->GeneralAccessedLabel() })
            {
                Label.Width(MaxGeneralLabelW + 10.0);
            }
        }
        PropertiesDiagLog(L"M19a generalLabelW=%.1f", MaxGeneralLabelW + 10.0);

        // Initial tab
        this->SwitchTab(0);
        PropertiesDiagLog(L"M20 initial tab switched");

        // Same width formula as SettingsPage: the tab bar is the widest
        // established control, so the default and the minimum both follow
        // it. Measuring the whole page at infinite width (or adding a
        // separate pixel floor) makes the window wider than the tabs and
        // leaves unused room on the right of every other tab.
        this->TabBar().Measure(Inf);
        const float TabBarW = this->TabBar().DesiredSize().Width;
        const float DefaultW = TabBarW;

        this->Measure(winrt::Windows::Foundation::Size(
            DefaultW,
            std::numeric_limits<float>::infinity()));
        const float MeasuredHeight = this->DesiredSize().Height;
        float DefaultH = MeasuredHeight;
        if (DefaultH < 420.0f)
        {
            DefaultH = 420.0f;
        }
        PropertiesDiagLog(L"M21 tabBarW=%.0f default=%.0fx%.0f measuredH=%.0f",
            TabBarW,
            DefaultW,
            DefaultH,
            MeasuredHeight);
        int MinClientW = (int)(DefaultW + 0.5f);
        const int MinClientH = 420;

        const UINT Dpi = ::GetDpiForWindow(this->m_WindowHandle);
        const float Scale = (float)Dpi / (float)USER_DEFAULT_SCREEN_DPI;
        int MinW = (int)((float)MinClientW * Scale + 0.5f);
        int MinH = (int)((float)MinClientH * Scale + 0.5f);

        RECT rc = { 0, 0, MinW, MinH };
        {
            const LONG_PTR Style = ::GetWindowLongPtrW(
                this->m_WindowHandle, GWL_STYLE);
            const LONG_PTR ExStyle = ::GetWindowLongPtrW(
                this->m_WindowHandle, GWL_EXSTYLE);
            ::AdjustWindowRectEx(&rc, (DWORD)Style, FALSE, (DWORD)ExStyle);
        }
        this->MinTrackW = rc.right - rc.left;
        this->MinTrackH = rc.bottom - rc.top;
        PropertiesDiagLog(L"M22 PrepareForShow done (min %d x %d)",
            this->MinTrackW, this->MinTrackH);

        return winrt::Windows::Foundation::Size(DefaultW, DefaultH);
    }

    // =====================================================================
    // Data collection
    // =====================================================================

    void PropertiesPage::CollectData()
    {
        PropertiesDiagLog(L"M04 CollectData enter (single=%d)",
            this->m_SingleSelection ? 1 : 0);
        if (this->m_Paths.empty())
        {
            PropertiesDiagLog(L"M04b CollectData: no paths");
            return;
        }

        this->CollectGeneralAndDetails();
        if (this->m_SingleSelection)
        {
            this->CollectSecurity();
            this->CollectCustom();
            // Signature verification and the previous-versions WMI query
            // run lazily on worker threads when their tabs are first
            // opened; neither may block the UI thread during display.
        }
        PropertiesDiagLog(L"M04z CollectData leave");
    }

    winrt::hstring PropertiesPage::GetHeaderName()
    {
        if (this->m_SingleSelection)
        {
            // Prefer the display name collected from the property store.
            if (!this->m_DisplayName.empty())
            {
                return this->m_DisplayName;
            }
            if (!this->m_Paths.empty())
            {
                return winrt::hstring(this->m_Paths[0]);
            }
            return winrt::hstring(L"");
        }
        wchar_t Buffer[64] = {};
        (void)swprintf_s(Buffer, L"%zu 个项目", this->m_Paths.size());
        return winrt::hstring(Buffer);
    }

    winrt::hstring PropertiesPage::GetHeaderType()
    {
        if (this->m_SingleSelection)
        {
            return this->m_GeneralType;
        }
        return winrt::hstring(L"");
    }

    void PropertiesPage::CollectGeneralAndDetails()
    {
        PropertiesDiagLog(L"M05 general+details collect enter");
        for (std::wstring const& Path : this->m_Paths)
        {
            winrt::com_ptr<IShellItem2> Item;
            if (FAILED(::SHCreateItemFromParsingName(
                Path.c_str(),
                nullptr,
                IID_PPV_ARGS(&Item))))
            {
                continue;
            }
            winrt::com_ptr<IPropertyStore> Store;
            if (FAILED(Item->GetPropertyStore(
                GPS_DEFAULT,
                IID_PPV_ARGS(&Store))))
            {
                continue;
            }

            winrt::hstring Name = ReadStringProp(Store.get(), PKEY_ItemNameDisplay);
            if (Name.empty())
            {
                Name = winrt::hstring(Path);
            }
            if (this->m_SingleSelection)
            {
                this->m_DisplayName = Name;
                this->m_GeneralType = ReadStringProp(Store.get(), PKEY_ItemTypeText);
                this->m_GeneralLocation = ReadStringProp(Store.get(), PKEY_ItemFolderPathDisplay);
                this->m_GeneralSize = this->FormatSize(
                    ReadUInt64Prop(Store.get(), PKEY_Size));
                this->m_GeneralAllocSize = this->FormatSize(
                    ReadUInt64Prop(Store.get(), PKEY_FileAllocationSize));
                this->m_GeneralCreated = this->FormatTime(
                    ReadFileTimeProp(Store.get(), PKEY_DateCreated));
                this->m_GeneralModified = this->FormatTime(
                    ReadFileTimeProp(Store.get(), PKEY_DateModified));
                this->m_GeneralAccessed = this->FormatTime(
                    ReadFileTimeProp(Store.get(), PKEY_DateAccessed));
                this->m_FileAttributes = ReadUInt32Prop(
                    Store.get(), PKEY_FileAttributes);
            }
            else
            {
                this->m_GeneralSize = this->FormatSize(
                    ReadUInt64Prop(Store.get(), PKEY_Size));
                this->m_DisplayName = Name;
            }

            // Details: enumerate the whole property store once, collecting
            // display name and formatted value for every non-empty property.
            DWORD Count = 0;
            if (FAILED(Store->GetCount(&Count)))
            {
                continue;
            }
            for (DWORD i = 0; i < Count; i++)
            {
                PROPERTYKEY Key = {};
                if (FAILED(Store->GetAt(i, &Key)))
                {
                    continue;
                }
                PROPVARIANT Value;
                ::PropVariantInit(&Value);
                if (FAILED(Store->GetValue(Key, &Value)) ||
                    Value.vt == VT_EMPTY)
                {
                    ::PropVariantClear(&Value);
                    continue;
                }

                winrt::hstring DetailName;
                winrt::hstring DetailValue;
                winrt::com_ptr<IPropertyDescription> Description;
                if (SUCCEEDED(::PSGetPropertyDescription(
                    Key,
                    IID_PPV_ARGS(&Description))))
                {
                    PWSTR NameText = nullptr;
                    if (SUCCEEDED(Description->GetDisplayName(&NameText)))
                    {
                        DetailName = NameText;
                        ::CoTaskMemFree(NameText);
                    }
                    PWSTR ValueText = nullptr;
                    if (SUCCEEDED(Description->FormatForDisplay(
                        Value,
                        PDFF_DEFAULT,
                        &ValueText)))
                    {
                        DetailValue = ValueText;
                        ::CoTaskMemFree(ValueText);
                    }
                }
                if (DetailName.empty())
                {
                    DetailName = winrt::hstring(
                        GetDisplayNameForPropertyKey(Key));
                }
                if (DetailValue.empty())
                {
                    // Fallback: convert the variant to a simple string.
                    PWSTR ValueText = nullptr;
                    if (SUCCEEDED(::PropVariantToStringAlloc(
                        Value,
                        &ValueText)))
                    {
                        DetailValue = ValueText;
                        ::CoTaskMemFree(ValueText);
                    }
                }
                ::PropVariantClear(&Value);

                if (!DetailName.empty())
                {
                    this->m_Details.Append(
                        winrt::make<PropertyDetailItem>(
                            DetailName,
                            DetailValue));
                }
            }
        }
        PropertiesDiagLog(L"M06 general+details collect done (%u rows)",
            this->m_Details.Size());
    }

    // Runs on a worker thread: verifies the file signature and extracts
    // the signer from the embedded PKCS#7. WinVerifyTrust can be slow on
    // first use, so it must never block the UI thread during display.
    static void QuerySignature(
        std::wstring const& Path,
        winrt::hstring& StatusText,
        std::vector<std::tuple<
            winrt::hstring,
            winrt::hstring,
            winrt::hstring>>& Rows)
    {
        PropertiesDiagLog(L"Q1 QuerySignature start");
        // Verification result first.
        WINTRUST_FILE_INFO FileInfo = {};
        FileInfo.cbStruct = sizeof(FileInfo);
        FileInfo.pcwszFilePath = Path.c_str();

        WINTRUST_DATA Data = {};
        Data.cbStruct = sizeof(Data);
        Data.dwUIChoice = WTD_UI_NONE;
        Data.fdwRevocationChecks = WTD_REVOKE_NONE;
        Data.dwUnionChoice = WTD_CHOICE_FILE;
        Data.pFile = &FileInfo;
        Data.dwStateAction = WTD_STATEACTION_VERIFY;
        Data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;

        GUID VerifyAction = WINTRUST_ACTION_GENERIC_VERIFY_V2;
        LONG VerifyStatus = ::WinVerifyTrust(
            nullptr,
            &VerifyAction,
            &Data);

        Data.dwStateAction = WTD_STATEACTION_CLOSE;
        (void)::WinVerifyTrust(
            nullptr,
            &VerifyAction,
            &Data);

        if (VerifyStatus == ERROR_SUCCESS)
        {
            StatusText = L"数字签名已验证。";
        }
        else if (VerifyStatus == TRUST_E_NOSIGNATURE ||
            VerifyStatus == TRUST_E_SUBJECT_FORM_UNKNOWN ||
            VerifyStatus == TRUST_E_PROVIDER_UNKNOWN ||
            VerifyStatus == CRYPT_E_FILE_ERROR)
        {
            StatusText = L"此文件没有有效的数字签名。";
            return;
        }
        else if (VerifyStatus == TRUST_E_EXPLICIT_DISTRUST ||
            VerifyStatus == TRUST_E_BAD_DIGEST)
        {
            StatusText = L"此文件的数字签名无效。";
            return;
        }
        else
        {
            StatusText = L"此文件的数字签名存在异常。";
            return;
        }

        PropertiesDiagLog(L"Q2 WinVerifyTrust status=0x%08X",
            (UINT32)VerifyStatus);

        // Extract the signer from the embedded PKCS#7.
        HCRYPTMSG Message = nullptr;
        DWORD EncodingType = 0;
        DWORD ContentType = 0;
        DWORD FormatType = 0;
        if (!::CryptQueryObject(
            CERT_QUERY_OBJECT_FILE,
            Path.c_str(),
            CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED |
                CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED,
            CERT_QUERY_FORMAT_FLAG_BINARY,
            0,
            &EncodingType,
            &ContentType,
            &FormatType,
            nullptr,
            &Message,
            nullptr))
        {
            PropertiesDiagLog(L"Q3 CryptQueryObject failed");
            return;
        }
        PropertiesDiagLog(L"Q3 CryptQueryObject ok");

        DWORD SignerInfoSize = 0;
        if (::CryptMsgGetParam(
            Message,
            CMSG_SIGNER_INFO_PARAM,
            0,
            nullptr,
            &SignerInfoSize) &&
            SignerInfoSize > 0)
        {
            std::vector<BYTE> SignerInfoBuffer(SignerInfoSize);
            if (::CryptMsgGetParam(
                Message,
                CMSG_SIGNER_INFO_PARAM,
                0,
                SignerInfoBuffer.data(),
                &SignerInfoSize))
            {
                // The signer certificate lives in the same message.
                DWORD CertSize = 0;
                if (::CryptMsgGetParam(
                    Message,
                    CMSG_CERT_PARAM,
                    0,
                    nullptr,
                    &CertSize) &&
                    CertSize > 0)
                {
                    std::vector<BYTE> CertBuffer(CertSize);
                    if (::CryptMsgGetParam(
                        Message,
                        CMSG_CERT_PARAM,
                        0,
                        CertBuffer.data(),
                        &CertSize))
                    {
                        PCCERT_CONTEXT Cert = ::CertCreateCertificateContext(
                            X509_ASN_ENCODING,
                            CertBuffer.data(),
                            CertSize);
                        if (Cert)
                        {
                            WCHAR Subject[512] = {};
                            WCHAR Issuer[512] = {};
                            ::CertGetNameStringW(
                                Cert,
                                CERT_NAME_SIMPLE_DISPLAY_TYPE,
                                0,
                                nullptr,
                                Subject,
                                512);
                            ::CertGetNameStringW(
                                Cert,
                                CERT_NAME_SIMPLE_DISPLAY_TYPE,
                                CERT_NAME_ISSUER_FLAG,
                                nullptr,
                                Issuer,
                                512);

                            winrt::hstring Detail = L"颁发者：";
                            Detail = Detail + Issuer;

                            BYTE Hash[20] = {};
                            DWORD HashSize = sizeof(Hash);
                            if (::CertGetCertificateContextProperty(
                                Cert,
                                CERT_SHA1_HASH_PROP_ID,
                                Hash,
                                &HashSize))
                            {
                                wchar_t HashText[64] = {};
                                for (DWORD h = 0; h < HashSize; h++)
                                {
                                    (void)swprintf_s(
                                        HashText + h * 2,
                                        64 - h * 2,
                                        L"%02X",
                                        Hash[h]);
                                }
                                Detail = Detail + L"\n指纹：";
                                Detail = Detail + HashText;
                            }

                            Rows.emplace_back(
                                winrt::hstring(Subject),
                                StatusText,
                                Detail);
                            ::CertFreeCertificateContext(Cert);
                        }
                    }
                }
            }
        }

        ::CryptMsgClose(Message);
    }

    winrt::Windows::Foundation::IAsyncAction PropertiesPage::OnSignatureTabEntered()
    {
        PropertiesDiagLog(L"S01 signature tab entered (loaded=%d)",
            this->m_SignatureLoaded ? 1 : 0);
        if (this->m_SignatureLoaded || this->m_Paths.size() != 1)
        {
            co_return;
        }
        this->m_SignatureLoaded = true;
        this->SignatureStatusText().Text(L"正在验证数字签名...");

        std::wstring Path = this->m_Paths[0];
        auto Dispatcher = this->Dispatcher();
        auto Weak = this->get_weak();

        winrt::hstring StatusText;
        std::vector<std::tuple<
            winrt::hstring,
            winrt::hstring,
            winrt::hstring>> Rows;

        co_await winrt::resume_background();
        PropertiesDiagLog(L"S02 signature worker start");
        QuerySignature(Path, StatusText, Rows);
        PropertiesDiagLog(L"S02b signature worker done (rows=%u)",
            (UINT32)Rows.size());

        Dispatcher.RunAsync(
            winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
            [Weak, StatusText, Rows]()
        {
            auto Self = Weak.get();
            if (Self)
            {
                Self->OnSignatureLoaded(StatusText, Rows);
            }
        });
    }

    void PropertiesPage::OnSignatureLoaded(
        winrt::hstring const& StatusText,
        std::vector<std::tuple<
            winrt::hstring,
            winrt::hstring,
            winrt::hstring>> const& Rows)
    {
        PropertiesDiagLog(L"S03 signature loaded on UI thread (rows=%u)",
            (UINT32)Rows.size());
        for (auto const& Row : Rows)
        {
            this->m_Signatures.Append(
                winrt::make<SignatureItem>(
                    std::get<0>(Row),
                    std::get<1>(Row),
                    std::get<2>(Row)));
        }
        this->SignatureStatusText().Text(StatusText);
        this->SignatureListView().ItemsSource(this->m_Signatures);
    }

    void PropertiesPage::CollectSecurity()
    {
        PropertiesDiagLog(L"M07 security collect enter");
        if (this->m_Paths.size() != 1)
        {
            return;
        }
        std::wstring const& Path = this->m_Paths[0];

        PSID Owner = nullptr;
        PSID Group = nullptr;
        PACL Dacl = nullptr;
        PSECURITY_DESCRIPTOR SecurityDescriptor = nullptr;
        DWORD Error = ::GetNamedSecurityInfoW(
            Path.c_str(),
            SE_FILE_OBJECT,
            OWNER_SECURITY_INFORMATION |
                GROUP_SECURITY_INFORMATION |
                DACL_SECURITY_INFORMATION,
            &Owner,
            &Group,
            &Dacl,
            nullptr,
            &SecurityDescriptor);
        if (Error != ERROR_SUCCESS || !SecurityDescriptor)
        {
            this->m_SecurityOwnerText = L"无法读取安全信息。";
            return;
        }

        std::wstring OwnerName = SidToName(Owner);
        this->m_SecurityOwnerText = L"所有者：";
        this->m_SecurityOwnerText = this->m_SecurityOwnerText + OwnerName;

        if (Dacl)
        {
            ACL_SIZE_INFORMATION AclInfo = {};
            if (::GetAclInformation(
                Dacl,
                &AclInfo,
                sizeof(AclInfo),
                AclSizeInformation))
            {
                for (DWORD i = 0; i < AclInfo.AceCount; i++)
                {
                    void* Ace = nullptr;
                    if (!::GetAce(Dacl, i, &Ace))
                    {
                        continue;
                    }
                    ACE_HEADER* Header =
                        reinterpret_cast<ACE_HEADER*>(Ace);
                    if (Header->AceType != ACCESS_ALLOWED_ACE_TYPE &&
                        Header->AceType != ACCESS_DENIED_ACE_TYPE)
                    {
                        continue;
                    }
                    ACCESS_ALLOWED_ACE* AllowAce =
                        reinterpret_cast<ACCESS_ALLOWED_ACE*>(Ace);
                    PSID AceSid = &AllowAce->SidStart;
                    std::wstring AceName = SidToName(AceSid);
                    std::wstring AccessText = MapAccessMask(AllowAce->Mask);
                    if (Header->AceType == ACCESS_DENIED_ACE_TYPE)
                    {
                        AccessText = L"拒绝：" + AccessText;
                    }
                    this->m_SecurityItems.Append(
                        winrt::make<SecurityItem>(
                            winrt::hstring(AceName),
                            winrt::hstring(AccessText)));
                }
            }
        }

        ::LocalFree(SecurityDescriptor);
        PropertiesDiagLog(L"M08 security collect done (%u rows)",
            this->m_SecurityItems.Size());
    }

    // Runs on a worker thread: enumerates volume shadow copies through WMI
    // and collects the ones that still contain the target path. Returns
    // (display time, full shadow-copy path) pairs. WMI can block, so it
    // must never run on the UI thread.
    static void QueryShadowCopies(
        std::wstring const& Path,
        std::vector<std::pair<winrt::hstring, std::wstring>>& Results)
    {
        PropertiesDiagLog(L"W1 QueryShadowCopies start");
        HRESULT ComResult = ::CoInitializeEx(
            nullptr,
            COINIT_MULTITHREADED);
        if (FAILED(ComResult) && ComResult != RPC_E_CHANGED_MODE)
        {
            return;
        }

        // Compute the path relative to the volume root so it can be
        // appended to every shadow copy device path.
        wchar_t VolumeRoot[4] = {};
        if (!::GetVolumePathNameW(
            Path.c_str(),
            VolumeRoot,
            4))
        {
            if (SUCCEEDED(ComResult))
            {
                ::CoUninitialize();
            }
            return;
        }
        std::wstring RelativePath = Path;
        size_t RootLen = wcslen(VolumeRoot);
        if (RelativePath.size() > RootLen)
        {
            RelativePath = RelativePath.substr(RootLen);
        }
        else
        {
            if (SUCCEEDED(ComResult))
            {
                ::CoUninitialize();
            }
            return;
        }

        winrt::com_ptr<IWbemLocator> Locator;
        if (FAILED(::CoCreateInstance(
            CLSID_WbemLocator,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&Locator))))
        {
            if (SUCCEEDED(ComResult))
            {
                ::CoUninitialize();
            }
            return;
        }
        winrt::com_ptr<IWbemServices> Service;
        BSTR Namespace = ::SysAllocString(L"ROOT\\CIMV2");
        HRESULT ConnectResult = Locator->ConnectServer(
            Namespace,
            nullptr,
            nullptr,
            nullptr,
            0,
            nullptr,
            nullptr,
            Service.put());
        ::SysFreeString(Namespace);
        if (FAILED(ConnectResult))
        {
            PropertiesDiagLog(L"W2 ConnectServer failed 0x%08X",
                (UINT32)ConnectResult);
            if (SUCCEEDED(ComResult))
            {
                ::CoUninitialize();
            }
            return;
        }
        PropertiesDiagLog(L"W2 ConnectServer ok");
        (void)::CoSetProxyBlanket(
            Service.get(),
            RPC_C_AUTHN_WINNT,
            RPC_C_AUTHZ_NONE,
            nullptr,
            RPC_C_AUTHN_LEVEL_CALL,
            RPC_C_IMP_LEVEL_IMPERSONATE,
            nullptr,
            EOAC_NONE);

        winrt::com_ptr<IEnumWbemClassObject> Enumerator;
        BSTR QueryLanguage = ::SysAllocString(L"WQL");
        BSTR QueryText = ::SysAllocString(L"SELECT * FROM Win32_ShadowCopy");
        HRESULT QueryResult = Service->ExecQuery(
            QueryLanguage,
            QueryText,
            WBEM_FLAG_FORWARD_ONLY,
            nullptr,
            Enumerator.put());
        ::SysFreeString(QueryLanguage);
        ::SysFreeString(QueryText);
        if (FAILED(QueryResult))
        {
            PropertiesDiagLog(L"W3 ExecQuery failed 0x%08X",
                (UINT32)QueryResult);
            if (SUCCEEDED(ComResult))
            {
                ::CoUninitialize();
            }
            return;
        }
        PropertiesDiagLog(L"W3 ExecQuery ok");

        IWbemClassObject* Object = nullptr;
        ULONG Returned = 0;
        while (SUCCEEDED(Enumerator->Next(
            WBEM_INFINITE,
            1,
            &Object,
            &Returned)) &&
            Returned == 1)
        {
            VARIANT DeviceVariant;
            ::VariantInit(&DeviceVariant);
            if (SUCCEEDED(Object->Get(
                L"DeviceObject",
                0,
                &DeviceVariant,
                nullptr,
                nullptr)) &&
                DeviceVariant.vt == VT_BSTR)
            {
                std::wstring Device = DeviceVariant.bstrVal;
                std::wstring Target = Device + L"\\" + RelativePath;
                if (::GetFileAttributesW(Target.c_str()) !=
                    INVALID_FILE_ATTRIBUTES)
                {
                    // Time comes from InstallDate in CIM_DATETIME form:
                    // yyyymmddhhmmss.mmmmmm+ooo (UTC). Convert to local.
                    VARIANT TimeVariant;
                    ::VariantInit(&TimeVariant);
                    winrt::hstring TimeText = L"-";
                    if (SUCCEEDED(Object->Get(
                        L"InstallDate",
                        0,
                        &TimeVariant,
                        nullptr,
                        nullptr)) &&
                        TimeVariant.vt == VT_BSTR &&
                        wcslen(TimeVariant.bstrVal) >= 14)
                    {
                        wchar_t const* P = TimeVariant.bstrVal;
                        SYSTEMTIME UtcTime = {};
                        auto Two = [P](size_t Offset)
                        {
                            return (WORD)((P[Offset] - L'0') * 10 +
                                (P[Offset + 1] - L'0'));
                        };
                        UtcTime.wYear = (WORD)(Two(0) * 100 + Two(2));
                        UtcTime.wMonth = Two(4);
                        UtcTime.wDay = Two(6);
                        UtcTime.wHour = Two(8);
                        UtcTime.wMinute = Two(10);
                        UtcTime.wSecond = Two(12);
                        FILETIME UtcFileTime = {};
                        FILETIME LocalFileTime = {};
                        if (::SystemTimeToFileTime(&UtcTime, &UtcFileTime) &&
                            ::FileTimeToLocalFileTime(
                                &UtcFileTime,
                                &LocalFileTime))
                        {
                            SYSTEMTIME LocalTime = {};
                            if (::FileTimeToSystemTime(
                                &LocalFileTime,
                                &LocalTime))
                            {
                                wchar_t Buffer[64] = {};
                                (void)swprintf_s(
                                    Buffer,
                                    L"%d/%d/%d %02d:%02d",
                                    LocalTime.wYear,
                                    LocalTime.wMonth,
                                    LocalTime.wDay,
                                    LocalTime.wHour,
                                    LocalTime.wMinute);
                                TimeText = Buffer;
                            }
                        }
                    }
                    ::VariantClear(&TimeVariant);

                    Results.emplace_back(
                        TimeText,
                        Target);
                }
            }
            ::VariantClear(&DeviceVariant);
            Object->Release();
            Object = nullptr;
        }

        if (SUCCEEDED(ComResult))
        {
            ::CoUninitialize();
        }
        PropertiesDiagLog(L"W5 QueryShadowCopies done (%u rows)",
            (UINT32)Results.size());
    }

    winrt::Windows::Foundation::IAsyncAction PropertiesPage::OnVersionsTabEntered()
    {
        PropertiesDiagLog(L"V01 versions tab entered (loaded=%d)",
            this->m_VersionsLoaded ? 1 : 0);
        if (this->m_VersionsLoaded || this->m_Paths.size() != 1)
        {
            co_return;
        }
        this->m_VersionsLoaded = true;
        this->VersionsStatusText().Text(L"正在查找以前的版本...");

        std::wstring Path = this->m_Paths[0];
        auto Dispatcher = this->Dispatcher();
        auto Weak = this->get_weak();

        std::vector<std::pair<winrt::hstring, std::wstring>> Results;

        co_await winrt::resume_background();
        PropertiesDiagLog(L"V02 versions worker start");
        QueryShadowCopies(Path, Results);
        PropertiesDiagLog(L"V02b versions worker done (rows=%u)",
            (UINT32)Results.size());

        Dispatcher.RunAsync(
            winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
            [Weak, Results]()
        {
            auto Self = Weak.get();
            if (Self)
            {
                Self->OnVersionsLoaded(Results);
            }
        });
    }

    void PropertiesPage::OnVersionsLoaded(
        std::vector<std::pair<
            winrt::hstring,
            std::wstring>> const& Results)
    {
        PropertiesDiagLog(L"V03 versions loaded on UI thread (rows=%u)",
            (UINT32)Results.size());
        for (auto const& Item : Results)
        {
            this->m_VersionItems.Append(
                winrt::make<VersionItem>(
                    Item.first,
                    winrt::hstring(Item.second)));
            this->m_VersionPaths.push_back(Item.second);
        }
        this->VersionsListView().ItemsSource(this->m_VersionItems);
        this->VersionsStatusText().Text(
            this->m_VersionItems.Size() == 0
                ? winrt::hstring(L"没有可用的以前的版本。")
                : winrt::hstring(L"可用以前的版本："));
    }

    void PropertiesPage::CollectCustom()
    {
        PropertiesDiagLog(L"M09 custom collect enter");
        if (this->m_Paths.size() != 1)
        {
            return;
        }
        std::wstring const& Path = this->m_Paths[0];

        // The writable property list mirrors the property system's
        // document properties; each entry maps to a real PKEY so the
        // value round-trips through IPropertyStore.
        struct WritableProperty
        {
            PROPERTYKEY Key;
            LPCWSTR Type;
        };
        static const WritableProperty kWritableProperties[] =
        {
            { PKEY_Comment, L"文本" },
            { PKEY_Title, L"文本" },
            { PKEY_Subject, L"文本" },
            { PKEY_Author, L"文本" },
            { PKEY_Category, L"文本" },
            { PKEY_Keywords, L"文本" },
            { PKEY_Rating, L"数字" },
        };
        const size_t kCount = _countof(kWritableProperties);

        winrt::com_ptr<IShellItem2> Item;
        if (FAILED(::SHCreateItemFromParsingName(
            Path.c_str(),
            nullptr,
            IID_PPV_ARGS(&Item))))
        {
            return;
        }
        winrt::com_ptr<IPropertyStore> Store;
        if (FAILED(Item->GetPropertyStore(
            GPS_READWRITE,
            IID_PPV_ARGS(&Store))))
        {
            return;
        }

        this->m_CustomKeys.clear();
        this->m_CustomNames.clear();
        this->m_AllCustomKeys.clear();

        for (size_t i = 0; i < kCount; i++)
        {
            std::wstring DisplayName =
                GetDisplayNameForPropertyKey(kWritableProperties[i].Key);
            if (DisplayName.empty())
            {
                continue;
            }
            // The add dialog picks from the full writable list, while the
            // list rows (and delete/edit targets) map to the existing keys
            // below, so both lists are kept in sync with their own indexes.
            this->m_AllCustomKeys.push_back(kWritableProperties[i].Key);

            PROPVARIANT Value;
            ::PropVariantInit(&Value);
            if (SUCCEEDED(Store->GetValue(
                kWritableProperties[i].Key,
                &Value)) &&
                Value.vt != VT_EMPTY)
            {
                winrt::hstring ValueText;
                if (Value.vt == VT_UI4 || Value.vt == VT_I4)
                {
                    UINT32 Number = 0;
                    (void)::PropVariantToUInt32(Value, &Number);
                    wchar_t Buffer[32] = {};
                    (void)swprintf_s(Buffer, L"%u", Number);
                    ValueText = Buffer;
                }
                else
                {
                    PWSTR Text = nullptr;
                    if (SUCCEEDED(::PropVariantToStringAlloc(
                        Value,
                        &Text)))
                    {
                        ValueText = Text;
                        ::CoTaskMemFree(Text);
                    }
                }
                ::PropVariantClear(&Value);

                this->m_CustomItems.Append(
                    winrt::make<CustomPropertyItem>(
                        winrt::hstring(DisplayName),
                        winrt::hstring(kWritableProperties[i].Type),
                        ValueText));
                this->m_CustomKeys.push_back(kWritableProperties[i].Key);
                this->m_CustomNames.push_back(DisplayName);
            }
            else
            {
                ::PropVariantClear(&Value);
            }
        }

        // Fill the add/edit name combo once.
        this->CustomNameCombo().Items().Clear();
        for (size_t i = 0; i < kCount; i++)
        {
            std::wstring DisplayName =
                GetDisplayNameForPropertyKey(kWritableProperties[i].Key);
            if (!DisplayName.empty())
            {
                this->CustomNameCombo().Items().Append(
                    winrt::box_value(winrt::hstring(DisplayName)));
            }
        }
        this->CustomTypeCombo().Items().Clear();
        this->CustomTypeCombo().Items().Append(
            winrt::box_value(winrt::hstring(L"文本")));
        this->CustomTypeCombo().Items().Append(
            winrt::box_value(winrt::hstring(L"数字")));
        PropertiesDiagLog(L"M10 custom collect done (%u rows)",
            this->m_CustomItems.Size());
    }

    // =====================================================================
    // UI helpers
    // =====================================================================

    // Converts an HICON into a WriteableBitmap synchronously. This avoids
    // the async SetSourceAsync / StoreAsync paths entirely; awaiting them
    // with .get() on the UI thread can dead lock while the modal message
    // loop has not started yet, which froze the dialog before it appeared.
    static winrt::Windows::UI::Xaml::Media::ImageSource IconToImageSource(
        HICON Icon)
    {
        ICONINFO Info = {};
        if (!::GetIconInfo(Icon, &Info))
        {
            return nullptr;
        }

        BITMAP BitmapInfo = {};
        winrt::Windows::UI::Xaml::Media::Imaging::WriteableBitmap Writeable{ nullptr };
        if (Info.hbmColor &&
            ::GetObjectW(Info.hbmColor, sizeof(BITMAP), &BitmapInfo) &&
            BitmapInfo.bmWidth > 0 &&
            BitmapInfo.bmHeight > 0)
        {
            HDC ScreenDC = ::GetDC(nullptr);
            HDC MemoryDC = ::CreateCompatibleDC(ScreenDC);
            BITMAPINFO Bmi = {};
            Bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            Bmi.bmiHeader.biWidth = BitmapInfo.bmWidth;
            Bmi.bmiHeader.biHeight = -BitmapInfo.bmHeight; // top-down
            Bmi.bmiHeader.biPlanes = 1;
            Bmi.bmiHeader.biBitCount = 32;
            Bmi.bmiHeader.biCompression = BI_RGB;
            void* Bits = nullptr;
            HBITMAP Dib = ::CreateDIBSection(
                ScreenDC,
                &Bmi,
                DIB_RGB_COLORS,
                &Bits,
                nullptr,
                0);
            if (Dib && Bits)
            {
                HGDIOBJ Old = ::SelectObject(MemoryDC, Dib);
                (void)::DrawIconEx(
                    MemoryDC,
                    0,
                    0,
                    Icon,
                    BitmapInfo.bmWidth,
                    BitmapInfo.bmHeight,
                    0,
                    nullptr,
                    DI_NORMAL);
                ::SelectObject(MemoryDC, Old);

                Writeable = winrt::Windows::UI::Xaml::Media::Imaging::
                    WriteableBitmap(
                        BitmapInfo.bmWidth,
                        BitmapInfo.bmHeight);
                auto Buffer = Writeable.PixelBuffer();
                BYTE* Pixels = Buffer.data();
                if (Pixels)
                {
                    const UINT32 Size =
                        BitmapInfo.bmWidth * BitmapInfo.bmHeight * 4;
                    (void)memcpy_s(Pixels, Size, Bits, Size);
                }
                ::DeleteObject(Dib);
            }
            ::DeleteDC(MemoryDC);
            ::ReleaseDC(nullptr, ScreenDC);
        }
        if (Info.hbmColor)
        {
            ::DeleteObject(Info.hbmColor);
        }
        if (Info.hbmMask)
        {
            ::DeleteObject(Info.hbmMask);
        }
        return Writeable;
    }

    void PropertiesPage::LoadHeaderIcon(std::wstring const& Path)
    {
        SHFILEINFOW FileInfo = {};
        // SHGFI_USEFILEATTRIBUTES resolves the icon from the system icon
        // cache by extension only; it never touches the file itself.
        if (!::SHGetFileInfoW(
            Path.c_str(),
            FILE_ATTRIBUTE_NORMAL,
            &FileInfo,
            sizeof(FileInfo),
            SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES))
        {
            return;
        }
        HICON Icon = FileInfo.hIcon;
        if (!Icon)
        {
            return;
        }
        auto Source = IconToImageSource(Icon);
        ::DestroyIcon(Icon);
        if (Source)
        {
            this->HeaderIconImage().Source(Source);
        }
    }

    void PropertiesPage::RefreshDetailsFilter()
    {
        this->m_DetailsFiltered.Clear();
        winrt::hstring Filter = this->DetailsSearchBox().Text();
        for (auto const& Item : this->m_Details)
        {
            if (Filter.empty())
            {
                this->m_DetailsFiltered.Append(Item);
                continue;
            }
            auto Detail = Item.as<
                winrt::NanaZip::Modern::PropertyDetailItem>();
            std::wstring_view FilterView(Filter);
            bool NameMatch = std::wstring_view(Detail.Name()).find(
                FilterView) != std::wstring_view::npos;
            bool ValueMatch = std::wstring_view(Detail.Value()).find(
                FilterView) != std::wstring_view::npos;
            if (NameMatch || ValueMatch)
            {
                this->m_DetailsFiltered.Append(Item);
            }
        }
        this->DetailsListView().ItemsSource(this->m_DetailsFiltered);
    }

    void PropertiesPage::SwitchTab(int Index)
    {
        if (Index < 0 || Index > 5)
        {
            return;
        }
        this->m_CurrentTab = Index;
        this->GeneralTabPanel().Visibility(Index == 0
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        this->DetailsTabPanel().Visibility(Index == 1
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        this->SignatureTabPanel().Visibility(Index == 2
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        this->SecurityTabPanel().Visibility(Index == 3
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        this->VersionsTabPanel().Visibility(Index == 4
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        this->CustomTabPanel().Visibility(Index == 5
            ? winrt::Windows::UI::Xaml::Visibility::Visible
            : winrt::Windows::UI::Xaml::Visibility::Collapsed);
        this->TabGeneralButton().IsChecked(Index == 0);
        this->TabDetailsButton().IsChecked(Index == 1);
        this->TabSignatureButton().IsChecked(Index == 2);
        this->TabSecurityButton().IsChecked(Index == 3);
        this->TabVersionsButton().IsChecked(Index == 4);
        this->TabCustomButton().IsChecked(Index == 5);

        // Previous versions are queried lazily on a worker thread the
        // first time the tab is opened, so the dialog itself never blocks.
        if (Index == 2)
        {
            this->m_SignatureTask = this->OnSignatureTabEntered();
        }
        if (Index == 4)
        {
            this->m_VersionsTask = this->OnVersionsTabEntered();
        }
    }

    void PropertiesPage::MarkAttributeDirty()
    {
        if (this->m_SingleSelection)
        {
            this->m_AttributeDirty = true;
            this->ApplyButton().IsEnabled(true);
        }
    }

    void PropertiesPage::RefreshCustomButtons()
    {
        int Selected = -1;
        if (auto Item = this->CustomListView().SelectedItem())
        {
            Selected = 0;
        }
        this->EditCustomButton().IsEnabled(
            this->m_SingleSelection && Selected >= 0);
        this->DeleteCustomButton().IsEnabled(
            this->m_SingleSelection && Selected >= 0);
    }

    // =====================================================================
    // Event handlers
    // =====================================================================

    void PropertiesPage::TabButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);

        int Index = -1;
        if (sender == this->TabGeneralButton())
        {
            Index = 0;
        }
        else if (sender == this->TabDetailsButton())
        {
            Index = 1;
        }
        else if (sender == this->TabSignatureButton())
        {
            Index = 2;
        }
        else if (sender == this->TabSecurityButton())
        {
            Index = 3;
        }
        else if (sender == this->TabVersionsButton())
        {
            Index = 4;
        }
        else if (sender == this->TabCustomButton())
        {
            Index = 5;
        }
        if (Index >= 0)
        {
            this->SwitchTab(Index);
        }
    }

    void PropertiesPage::OnDetailsSearchChanged(
        winrt::IInspectable const& sender,
        winrt::Windows::UI::Xaml::Controls::TextChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        this->RefreshDetailsFilter();
    }

    void PropertiesPage::OnReadOnlyCheckClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        this->MarkAttributeDirty();
    }

    void PropertiesPage::OnHiddenCheckClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        this->MarkAttributeDirty();
    }

    // =====================================================================
    // ISecurityInformation adapter for the standard security editor
    // (EditSecurity in aclui.h). The dialog reads and writes the
    // descriptor directly on the file, so the adapter only needs the path.
    // =====================================================================

    struct FileSecurityInfo : winrt::implements<
        FileSecurityInfo,
        ISecurityInformation>
    {
        FileSecurityInfo(std::wstring const& Path) :
            m_Path(Path)
        {
        }

        HRESULT STDMETHODCALLTYPE GetObjectInformation(
            PSI_OBJECT_INFO ObjectInfo) override
        {
            if (!ObjectInfo)
            {
                return E_POINTER;
            }
            ObjectInfo->dwFlags = SI_ADVANCED;
            ObjectInfo->hInstance = nullptr;
            ObjectInfo->pszServerName = nullptr;
            ObjectInfo->pszObjectName = const_cast<LPWSTR>(m_Path.c_str());
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetSecurity(
            SECURITY_INFORMATION RequestedInformation,
            PSECURITY_DESCRIPTOR* SecurityDescriptor,
            BOOL Default) override
        {
            UNREFERENCED_PARAMETER(Default);
            if (!SecurityDescriptor)
            {
                return E_POINTER;
            }
            DWORD Error = ::GetNamedSecurityInfoW(
                m_Path.c_str(),
                SE_FILE_OBJECT,
                RequestedInformation,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                SecurityDescriptor);
            if (Error != ERROR_SUCCESS)
            {
                return HRESULT_FROM_WIN32(Error);
            }
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE SetSecurity(
            SECURITY_INFORMATION SecurityInformation,
            PSECURITY_DESCRIPTOR SecurityDescriptor) override
        {
            if (!SecurityDescriptor)
            {
                return E_POINTER;
            }
            PSID Owner = nullptr;
            PSID Group = nullptr;
            PACL Dacl = nullptr;
            PACL Sacl = nullptr;
            BOOL Present = FALSE;
            BOOL Defaulted = FALSE;
            (void)::GetSecurityDescriptorOwner(
                SecurityDescriptor,
                &Owner,
                &Defaulted);
            (void)::GetSecurityDescriptorGroup(
                SecurityDescriptor,
                &Group,
                &Defaulted);
            (void)::GetSecurityDescriptorDacl(
                SecurityDescriptor,
                &Present,
                &Dacl,
                &Defaulted);
            (void)::GetSecurityDescriptorSacl(
                SecurityDescriptor,
                &Present,
                &Sacl,
                &Defaulted);
            DWORD Error = ::SetNamedSecurityInfoW(
                const_cast<LPWSTR>(m_Path.c_str()),
                SE_FILE_OBJECT,
                SecurityInformation,
                (SecurityInformation & OWNER_SECURITY_INFORMATION)
                    ? Owner : nullptr,
                (SecurityInformation & GROUP_SECURITY_INFORMATION)
                    ? Group : nullptr,
                (SecurityInformation & DACL_SECURITY_INFORMATION)
                    ? Dacl : nullptr,
                (SecurityInformation & SACL_SECURITY_INFORMATION)
                    ? Sacl : nullptr);
            if (Error != ERROR_SUCCESS)
            {
                return HRESULT_FROM_WIN32(Error);
            }
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetAccessRights(
            const GUID* ObjectType,
            DWORD Flags,
            PSI_ACCESS* Access,
            ULONG* AccessCount,
            ULONG* DefaultAccess) override
        {
            UNREFERENCED_PARAMETER(ObjectType);
            UNREFERENCED_PARAMETER(Flags);
            static const SI_ACCESS c_Access[] =
            {
                { &GUID_NULL, GENERIC_ALL, L"完全控制", SI_ACCESS_GENERAL },
                { &GUID_NULL, GENERIC_READ | GENERIC_EXECUTE, L"读取和执行", SI_ACCESS_GENERAL },
                { &GUID_NULL, GENERIC_READ, L"读取", SI_ACCESS_GENERAL },
                { &GUID_NULL, GENERIC_WRITE, L"写入", SI_ACCESS_GENERAL },
            };
            *Access = const_cast<PSI_ACCESS>(c_Access);
            *AccessCount = ARRAYSIZE(c_Access);
            *DefaultAccess = 0;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE MapGeneric(
            const GUID* ObjectType,
            UCHAR* AceFlags,
            ACCESS_MASK* Mask) override
        {
            UNREFERENCED_PARAMETER(ObjectType);
            UNREFERENCED_PARAMETER(AceFlags);
            if (!Mask)
            {
                return E_POINTER;
            }
            ACCESS_MASK Mapped = 0;
            if (*Mask & GENERIC_ALL)
            {
                Mapped |= FILE_ALL_ACCESS;
            }
            if (*Mask & GENERIC_READ)
            {
                Mapped |= FILE_GENERIC_READ;
            }
            if (*Mask & GENERIC_WRITE)
            {
                Mapped |= FILE_GENERIC_WRITE;
            }
            if (*Mask & GENERIC_EXECUTE)
            {
                Mapped |= FILE_GENERIC_EXECUTE;
            }
            *Mask = Mapped;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetInheritTypes(
            PSI_INHERIT_TYPE* InheritTypes,
            ULONG* InheritTypeCount) override
        {
            if (!InheritTypes || !InheritTypeCount)
            {
                return E_POINTER;
            }
            *InheritTypes = nullptr;
            *InheritTypeCount = 0;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE PropertySheetPageCallback(
            HWND Window,
            UINT Message,
            SI_PAGE_TYPE Page) override
        {
            UNREFERENCED_PARAMETER(Window);
            UNREFERENCED_PARAMETER(Message);
            UNREFERENCED_PARAMETER(Page);
            return S_OK;
        }

    private:
        std::wstring m_Path;
    };

    void PropertiesPage::OnSecurityAdvancedClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        if (this->m_Paths.size() != 1)
        {
            return;
        }
        // The standard security editor edits the descriptor directly on
        // the file; after the dialog the security data is re-read so the
        // list stays in sync.
        auto SecurityInfo = winrt::make_self<FileSecurityInfo>(
            this->m_Paths[0]);
        BOOL Changed = ::EditSecurity(
            this->m_WindowHandle,
            SecurityInfo.get());

        if (Changed)
        {
            this->m_SecurityItems.Clear();
            this->m_SecurityOwnerText = L"";
            this->CollectSecurity();
            this->SecurityOwnerText().Text(this->m_SecurityOwnerText);
            this->SecurityListView().ItemsSource(this->m_SecurityItems);
        }
    }

    void PropertiesPage::OnVersionSelectionChanged(
        winrt::IInspectable const& sender,
        winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        int Selected = this->VersionsListView().SelectedIndex();
        bool HasSelection = Selected >= 0 &&
            Selected < static_cast<int>(this->m_VersionPaths.size());
        this->OpenVersionButton().IsEnabled(HasSelection);
        this->CopyVersionButton().IsEnabled(HasSelection);
    }

    void PropertiesPage::OnOpenVersionClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        int Selected = this->VersionsListView().SelectedIndex();
        if (Selected < 0 ||
            Selected >= static_cast<int>(this->m_VersionPaths.size()))
        {
            return;
        }
        HINSTANCE Result = ::ShellExecuteW(
            this->m_WindowHandle,
            L"open",
            this->m_VersionPaths[Selected].c_str(),
            nullptr,
            nullptr,
            SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(Result) <= 32)
        {
            // Fall back to the parent folder.
            std::wstring::size_type Pos =
                this->m_VersionPaths[Selected].find_last_of(L'\\');
            std::wstring Folder =
                this->m_VersionPaths[Selected].substr(0, Pos);
            (void)::ShellExecuteW(
                this->m_WindowHandle,
                L"open",
                Folder.c_str(),
                nullptr,
                nullptr,
                SW_SHOWNORMAL);
        }
    }

    void PropertiesPage::OnCopyVersionClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        int Selected = this->VersionsListView().SelectedIndex();
        if (Selected < 0 ||
            Selected >= static_cast<int>(this->m_VersionPaths.size()))
        {
            return;
        }
        std::wstring const& Source = this->m_VersionPaths[Selected];

        winrt::com_ptr<IFileSaveDialog> SaveDialog;
        if (FAILED(::CoCreateInstance(
            CLSID_FileSaveDialog,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&SaveDialog))))
        {
            return;
        }
        std::wstring FileName = this->m_Paths[0];
        std::wstring::size_type NamePos = FileName.find_last_of(L'\\');
        if (NamePos != std::wstring::npos)
        {
            FileName = FileName.substr(NamePos + 1);
        }
        SaveDialog->SetTitle(L"复制到");
        SaveDialog->SetFileName(FileName.c_str());

        HRESULT ShowResult = SaveDialog->Show(this->m_WindowHandle);
        if (FAILED(ShowResult))
        {
            return;
        }
        winrt::com_ptr<IShellItem> ResultItem;
        if (FAILED(SaveDialog->GetResult(ResultItem.put())))
        {
            return;
        }
        PWSTR TargetPath = nullptr;
        if (FAILED(ResultItem->GetDisplayName(
            SIGDN_FILESYSPATH,
            &TargetPath)))
        {
            return;
        }
        BOOL CopyResult = ::CopyFileW(
            Source.c_str(),
            TargetPath,
            FALSE);
        ::CoTaskMemFree(TargetPath);
        if (!CopyResult)
        {
            // No direct error surface in the first pass; the copy silently
            // fails when the target is locked or the volume is offline.
            (void)0;
        }
    }

    void PropertiesPage::OnCustomSelectionChanged(
        winrt::IInspectable const& sender,
        winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        this->RefreshCustomButtons();
    }

    void PropertiesPage::OnAddCustomClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        this->m_CustomEditMode = 1;
        this->CustomEditPanel().Visibility(
            winrt::Windows::UI::Xaml::Visibility::Visible);
        this->CustomNameCombo().IsEnabled(true);
        this->CustomTypeCombo().IsEnabled(true);
        this->CustomValueBox().Text(L"");
        this->CustomNameCombo().SelectedIndex(0);
        this->CustomTypeCombo().SelectedIndex(0);
        this->CustomValueBox().Focus(
            winrt::Windows::UI::Xaml::FocusState::Programmatic);
    }

    void PropertiesPage::OnEditCustomClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        int Selected = this->CustomListView().SelectedIndex();
        if (Selected < 0)
        {
            return;
        }
        this->m_CustomEditMode = 2;
        this->m_CustomEditIndex = Selected;
        this->CustomEditPanel().Visibility(
            winrt::Windows::UI::Xaml::Visibility::Visible);
        this->CustomNameCombo().IsEnabled(false);
        this->CustomTypeCombo().IsEnabled(false);
        winrt::NanaZip::Modern::CustomPropertyItem Item =
            this->m_CustomItems.GetAt(Selected).as<
                winrt::NanaZip::Modern::CustomPropertyItem>();
        this->CustomValueBox().Text(Item.Value());
        this->CustomValueBox().Focus(
            winrt::Windows::UI::Xaml::FocusState::Programmatic);
    }

    void PropertiesPage::OnDeleteCustomClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        if (this->m_Paths.size() != 1)
        {
            return;
        }
        int Selected = this->CustomListView().SelectedIndex();
        if (Selected < 0 ||
            Selected >= static_cast<int>(this->m_CustomKeys.size()))
        {
            return;
        }
        winrt::com_ptr<IShellItem2> Item;
        if (FAILED(::SHCreateItemFromParsingName(
            this->m_Paths[0].c_str(),
            nullptr,
            IID_PPV_ARGS(&Item))))
        {
            return;
        }
        winrt::com_ptr<IPropertyStore> Store;
        if (FAILED(Item->GetPropertyStore(
            GPS_READWRITE,
            IID_PPV_ARGS(&Store))))
        {
            return;
        }
        PROPVARIANT Empty;
        ::PropVariantInit(&Empty);
        Empty.vt = VT_EMPTY;
        if (SUCCEEDED(Store->SetValue(
            this->m_CustomKeys[Selected],
            Empty)))
        {
            (void)Store->Commit();
            this->m_CustomDirty = true;
            this->m_CustomItems.RemoveAt(Selected);
            this->CustomListView().ItemsSource(nullptr);
            this->CustomListView().ItemsSource(this->m_CustomItems);
            this->CustomStatusText().Text(this->m_CustomItems.Size() == 0
                ? winrt::hstring(L"没有自定义属性。")
                : winrt::hstring(L"自定义属性："));
            this->ApplyButton().IsEnabled(true);
            this->RefreshCustomButtons();
        }
        ::PropVariantClear(&Empty);
    }

    void PropertiesPage::OnCustomEditOkClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        if (this->m_Paths.size() != 1)
        {
            return;
        }
        int NameIndex = this->CustomNameCombo().SelectedIndex();
        PROPERTYKEY Key = {};
        if (this->m_CustomEditMode == 1)
        {
            // Add mode: the combo lists the full writable property set.
            if (NameIndex < 0 ||
                NameIndex >= static_cast<int>(this->m_AllCustomKeys.size()))
            {
                return;
            }
            Key = this->m_AllCustomKeys[NameIndex];
        }
        else if (this->m_CustomEditMode == 2)
        {
            // Edit mode: the selected row maps to an existing key.
            if (this->m_CustomEditIndex < 0 ||
                this->m_CustomEditIndex >=
                    static_cast<int>(this->m_CustomKeys.size()))
            {
                return;
            }
            Key = this->m_CustomKeys[this->m_CustomEditIndex];
        }
        else
        {
            return;
        }
        winrt::hstring ValueText = this->CustomValueBox().Text();
        if (ValueText.empty())
        {
            // An empty value means deletion in the shell property system.
            return;
        }

        winrt::com_ptr<IShellItem2> Item;
        if (FAILED(::SHCreateItemFromParsingName(
            this->m_Paths[0].c_str(),
            nullptr,
            IID_PPV_ARGS(&Item))))
        {
            return;
        }
        winrt::com_ptr<IPropertyStore> Store;
        if (FAILED(Item->GetPropertyStore(
            GPS_READWRITE,
            IID_PPV_ARGS(&Store))))
        {
            return;
        }

        PROPVARIANT Value;
        ::PropVariantInit(&Value);
        bool IsNumber = false;
        {
            winrt::com_ptr<IPropertyDescription> Description;
            if (SUCCEEDED(::PSGetPropertyDescription(
                Key,
                IID_PPV_ARGS(&Description))))
            {
                PROPVARIANT typeVariant;
                ::PropVariantInit(&typeVariant);
                VARTYPE PropertyType;
                if (SUCCEEDED(Description->GetPropertyType(&PropertyType)))
                {
                    if (PropertyType == VT_I4 ||
                        PropertyType == VT_UI4)
                    {
                        IsNumber = true;
                    }
                    ::PropVariantClear(&typeVariant);
                }
            }
        }
        if (IsNumber)
        {
            UINT32 Number = (UINT32)_wtoi(ValueText.c_str());
            Value.vt = VT_UI4;
            Value.ulVal = Number;
        }
        else
        {
            Value.vt = VT_LPWSTR;
            Value.pwszVal = ::SysAllocString(ValueText.c_str());
        }

        HRESULT SetResult = Store->SetValue(Key, Value);
        ::PropVariantClear(&Value);
        if (FAILED(SetResult))
        {
            return;
        }
        (void)Store->Commit();

        this->m_CustomDirty = true;
        this->ApplyButton().IsEnabled(true);
        this->CustomEditPanel().Visibility(
            winrt::Windows::UI::Xaml::Visibility::Collapsed);

        // Refresh the list from the store.
        this->m_CustomItems.Clear();
        this->CollectCustom();
        this->CustomListView().ItemsSource(nullptr);
        this->CustomListView().ItemsSource(this->m_CustomItems);
        this->CustomStatusText().Text(this->m_CustomItems.Size() == 0
            ? winrt::hstring(L"没有自定义属性。")
            : winrt::hstring(L"自定义属性："));
        this->RefreshCustomButtons();
    }

    void PropertiesPage::OnCustomEditCancelClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        this->m_CustomEditMode = 0;
        this->CustomEditPanel().Visibility(
            winrt::Windows::UI::Xaml::Visibility::Collapsed);
        this->RefreshCustomButtons();
    }

    void PropertiesPage::OnOkClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        if (this->m_AttributeDirty)
        {
            for (std::wstring const& Path : this->m_Paths)
            {
                DWORD Attributes = ::GetFileAttributesW(Path.c_str());
                if (Attributes == INVALID_FILE_ATTRIBUTES)
                {
                    continue;
                }
                if (this->GeneralReadOnlyCheck().IsChecked().Value())
                {
                    Attributes |= FILE_ATTRIBUTE_READONLY;
                }
                else
                {
                    Attributes &= ~FILE_ATTRIBUTE_READONLY;
                }
                if (this->GeneralHiddenCheck().IsChecked().Value())
                {
                    Attributes |= FILE_ATTRIBUTE_HIDDEN;
                }
                else
                {
                    Attributes &= ~FILE_ATTRIBUTE_HIDDEN;
                }
                (void)::SetFileAttributesW(Path.c_str(), Attributes);
            }
            this->m_AttributeDirty = false;
        }
        ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }

    void PropertiesPage::OnCancelClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }

    void PropertiesPage::OnApplyClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        if (this->m_AttributeDirty)
        {
            for (std::wstring const& Path : this->m_Paths)
            {
                DWORD Attributes = ::GetFileAttributesW(Path.c_str());
                if (Attributes == INVALID_FILE_ATTRIBUTES)
                {
                    continue;
                }
                if (this->GeneralReadOnlyCheck().IsChecked().Value())
                {
                    Attributes |= FILE_ATTRIBUTE_READONLY;
                }
                else
                {
                    Attributes &= ~FILE_ATTRIBUTE_READONLY;
                }
                if (this->GeneralHiddenCheck().IsChecked().Value())
                {
                    Attributes |= FILE_ATTRIBUTE_HIDDEN;
                }
                else
                {
                    Attributes &= ~FILE_ATTRIBUTE_HIDDEN;
                }
                (void)::SetFileAttributesW(Path.c_str(), Attributes);
            }
            this->m_AttributeDirty = false;
        }
        this->ApplyButton().IsEnabled(false);
    }

    void PropertiesPage::OnPageKeyDown(
        winrt::IInspectable const& sender,
        winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        if (e.Key() == winrt::Windows::System::VirtualKey::Escape)
        {
            e.Handled(true);
            ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
        }
    }
}
