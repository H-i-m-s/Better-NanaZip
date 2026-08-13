#include "pch.h"
#include "OverwritePage.h"
#if __has_include("OverwritePage.g.cpp")
#include "OverwritePage.g.cpp"
#endif

#include "NanaZip.Modern.h"

#include <wincodec.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <vector>

#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.Storage.Streams.h>

namespace winrt::NanaZip::Modern::implementation
{
    OverwritePage::OverwritePage(
        _In_opt_ HWND WindowHandle,
        _In_ PK7_OVERWRITE_DIALOG_CONTEXT Context) :
        m_WindowHandle(WindowHandle),
        m_Context(Context)
    {
        this->Unloaded({ this, &OverwritePage::OnUnloaded });
        this->Loaded({ this, &OverwritePage::OnLoaded });
        this->KeyDown({ this, &OverwritePage::OnPageKeyDown });
    }

    void OverwritePage::InitializeComponent()
    {
        OverwritePageT::InitializeComponent();
    }

    winrt::hstring OverwritePage::Res(
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

    void OverwritePage::ApplyDialogFont(UINT32 Pt)
    {
        double FontSizePx = (Pt == 0) ? 0.0 : (double)Pt * 96.0 / 72.0;
        ApplyFontToTree(*this, FontSizePx);
    }

    void OverwritePage::ApplyFontToTree(
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

    static void ReduceString(std::wstring& s, bool IsBig)
    {
        const size_t Limit = IsBig ? 82 : 30;
        if (s.size() > Limit)
        {
            const size_t Half = Limit / 2;
            s.replace(Half, s.size() - Limit, L" ... ");
        }
        if (!s.empty() && s.back() == L' ')
        {
            s.insert(s.begin(), L'"');
            s += L'"';
        }
    }

    static winrt::hstring RemoveMnemonic(winrt::hstring const& Text)
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

    static std::wstring UInt64ToString(UINT64 Value)
    {
        wchar_t Buf[32];
        _ui64tow_s(Value, Buf, _countof(Buf), 10);
        return std::wstring(Buf);
    }

    static std::wstring FileTimeToString(FILETIME const& Time)
    {
        FILETIME Local;
        if (!::FileTimeToLocalFileTime(&Time, &Local))
        {
            return L"";
        }
        SYSTEMTIME St;
        if (!::FileTimeToSystemTime(&Local, &St))
        {
            return L"";
        }
        wchar_t Buf[64];
        _snwprintf_s(Buf, _countof(Buf), _TRUNCATE,
            L"%04u-%02u-%02u %02u:%02u:%02u",
            (unsigned)St.wYear, (unsigned)St.wMonth, (unsigned)St.wDay,
            (unsigned)St.wHour, (unsigned)St.wMinute, (unsigned)St.wSecond);
        return std::wstring(Buf);
    }

    winrt::hstring OverwritePage::BuildFileInfoText(
        BOOLEAN SizeDefined,
        BOOLEAN TimeDefined,
        UINT64 Size,
        FILETIME const& Time,
        LPCWSTR Name)
    {
        std::wstring SizeString;
        if (SizeDefined)
        {
            std::wstring Format = Res(3504, L"{0} bytes").c_str();
            std::wstring SizeText = UInt64ToString(Size);
            size_t Pos = Format.find(L"{0}");
            if (Pos != std::wstring::npos)
            {
                Format.replace(Pos, 3, SizeText);
            }
            SizeString = Format;
        }

        std::wstring FileName(Name ? Name : L"");
        size_t SlashPos = FileName.find_last_of(L"\\/");

        std::wstring S1 = (SlashPos == std::wstring::npos)
            ? std::wstring()
            : FileName.substr(0, SlashPos + 1);
        std::wstring S2 = (SlashPos == std::wstring::npos)
            ? FileName
            : FileName.substr(SlashPos + 1);

        ReduceString(S1, true);
        ReduceString(S2, true);

        std::wstring S = S1;
        S += L"\n";
        S += S2;
        S += L"\n";
        S += SizeString;
        S += L"\n";

        if (TimeDefined)
        {
            std::wstring MtimeLabel = Res(1012, L"Modified").c_str();
            S += MtimeLabel;
            S += L": ";
            S += FileTimeToString(Time);
        }

        return winrt::hstring(S);
    }

    static winrt::Windows::UI::Xaml::Media::ImageSource IconFromHIcon(HICON Icon)
    {
        if (!Icon)
        {
            return nullptr;
        }

        winrt::Windows::UI::Xaml::Media::Imaging::BitmapImage Bitmap;

        IStream* Stream = nullptr;
        if (FAILED(::CreateStreamOnHGlobal(nullptr, TRUE, &Stream)))
        {
            return nullptr;
        }

        HRESULT hr = E_FAIL;
        IWICImagingFactory* Factory = nullptr;
        hr = ::CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&Factory));
        if (SUCCEEDED(hr))
        {
            IWICBitmap* BitmapWic = nullptr;
            hr = Factory->CreateBitmapFromHICON(Icon, &BitmapWic);
            if (SUCCEEDED(hr))
            {
                IWICStream* WicStream = nullptr;
                hr = Factory->CreateStream(&WicStream);
                if (SUCCEEDED(hr))
                {
                    hr = WicStream->InitializeFromIStream(Stream);
                    if (SUCCEEDED(hr))
                    {
                        IWICBitmapEncoder* Encoder = nullptr;
                        hr = Factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &Encoder);
                        if (SUCCEEDED(hr))
                        {
                            hr = Encoder->Initialize(WicStream, WICBitmapEncoderNoCache);
                            if (SUCCEEDED(hr))
                            {
                                IWICBitmapFrameEncode* Frame = nullptr;
                                hr = Encoder->CreateNewFrame(&Frame, nullptr);
                                if (SUCCEEDED(hr))
                                {
                                    hr = Frame->Initialize(nullptr);
                                    if (SUCCEEDED(hr))
                                    {
                                        hr = Frame->WriteSource(BitmapWic, nullptr);
                                        if (SUCCEEDED(hr))
                                        {
                                            hr = Frame->Commit();
                                        }
                                    }
                                    Frame->Release();
                                }
                                if (SUCCEEDED(hr))
                                {
                                    hr = Encoder->Commit();
                                }
                            }
                            Encoder->Release();
                        }
                    }
                    WicStream->Release();
                }
                BitmapWic->Release();
            }
            Factory->Release();
        }

        if (SUCCEEDED(hr))
        {
            // Read the encoded PNG bytes out of the memory stream and feed
            // them to a BitmapImage through an in-memory random access stream
            // (C++/WinRT has no IStream -> IRandomAccessStream adapter).
            LARGE_INTEGER Zero = {};
            Stream->Seek(Zero, STREAM_SEEK_SET, nullptr);
            STATSTG Stat = {};
            if (SUCCEEDED(Stream->Stat(&Stat, STATFLAG_NONAME)) &&
                Stat.cbSize.QuadPart > 0)
            {
                std::vector<BYTE> Bytes((size_t)Stat.cbSize.QuadPart);
                ULONG Read = 0;
                if (SUCCEEDED(Stream->Read(Bytes.data(),
                    (ULONG)Bytes.size(), &Read)))
                {
                    Bytes.resize(Read);
                    winrt::Windows::Storage::Streams::InMemoryRandomAccessStream
                        MemStream;
                    winrt::Windows::Storage::Streams::DataWriter Writer(MemStream);
                    Writer.WriteBytes(Bytes);
                    Writer.StoreAsync().get();
                    MemStream.Seek(0);
                    Bitmap.SetSourceAsync(MemStream).get();
                }
            }
        }

        Stream->Release();
        return Bitmap;
    }

    void OverwritePage::LoadFileIcon(
        winrt::Windows::UI::Xaml::Controls::Image const& Target,
        LPCWSTR Name)
    {
        if (!Name || !Name[0])
        {
            return;
        }

        SHFILEINFOW FileInfo = {};
        if (::SHGetFileInfoW(
            Name,
            FILE_ATTRIBUTE_NORMAL,
            &FileInfo,
            sizeof(FileInfo),
            SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_LARGEICON))
        {
            Target.Source(IconFromHIcon(FileInfo.hIcon));
            ::DestroyIcon(FileInfo.hIcon);
        }
    }

    void OverwritePage::OnLoaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        if (!this->m_Context)
        {
            return;
        }

        PK7_OVERWRITE_DIALOG_CONTEXT Context = this->m_Context;

        HeaderText().Text(Res(3501, L"Destination folder already contains processed file."));
        QuestionBeginText().Text(Res(3502, L"Would you like to replace the existing file"));
        QuestionEndText().Text(Res(3503, L"with this one?"));

        OldFileInfoText().Text(BuildFileInfoText(
            Context->OldSizeDefined,
            Context->OldTimeDefined,
            Context->OldSize,
            Context->OldTime,
            Context->OldName));
        NewFileInfoText().Text(BuildFileInfoText(
            Context->NewSizeDefined,
            Context->NewTimeDefined,
            Context->NewSize,
            Context->NewTime,
            Context->NewName));

        LoadFileIcon(OldFileIcon(), Context->OldName);
        LoadFileIcon(NewFileIcon(), Context->NewName);

        ::SetWindowTextW(
            this->m_WindowHandle,
            Res(3500, L"Confirm File Replace").c_str());

        YesButton().Content(winrt::box_value(
            RemoveMnemonic(Res(406, L"&Yes"))));
        NoButton().Content(winrt::box_value(
            RemoveMnemonic(Res(407, L"&No"))));
        CancelButton().Content(winrt::box_value(
            RemoveMnemonic(Res(402, L"&Cancel"))));
        YesToAllButton().Content(winrt::box_value(
            RemoveMnemonic(Res(440, L"Yes to &All"))));
        NoToAllButton().Content(winrt::box_value(
            RemoveMnemonic(Res(441, L"No to A&ll"))));
        AutoRenameButton().Content(winrt::box_value(
            RemoveMnemonic(Res(3505, L"A&uto Rename"))));

        if (!Context->ShowExtraButtons)
        {
            YesToAllButton().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Collapsed);
            NoToAllButton().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Collapsed);
            AutoRenameButton().Visibility(
                winrt::Windows::UI::Xaml::Visibility::Collapsed);
        }

        ApplyDialogFont(Context->FontSizeDialog);

        if (Context->DefaultIsNo)
        {
            NoButton().Focus(winrt::Windows::UI::Xaml::FocusState::Programmatic);
        }
        else
        {
            YesButton().Focus(winrt::Windows::UI::Xaml::FocusState::Programmatic);
        }
    }

    void OverwritePage::OnUnloaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
    }

    void OverwritePage::SetResult(UINT32 Result)
    {
        if (this->m_Context)
        {
            this->m_Context->Result = Result;
        }
        ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }

    void OverwritePage::OnYesClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        SetResult(K7_OVERWRITE_DIALOG_RESULT_YES);
    }

    void OverwritePage::OnYesToAllClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        SetResult(K7_OVERWRITE_DIALOG_RESULT_YES_TO_ALL);
    }

    void OverwritePage::OnAutoRenameClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        SetResult(K7_OVERWRITE_DIALOG_RESULT_AUTO_RENAME);
    }

    void OverwritePage::OnNoClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        SetResult(K7_OVERWRITE_DIALOG_RESULT_NO);
    }

    void OverwritePage::OnNoToAllClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        SetResult(K7_OVERWRITE_DIALOG_RESULT_NO_TO_ALL);
    }

    void OverwritePage::OnCancelClicked(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        SetResult(K7_OVERWRITE_DIALOG_RESULT_CANCEL);
    }

    void OverwritePage::OnPageKeyDown(
        winrt::IInspectable const& sender,
        winrt::KeyRoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);

        if (e.Key() == winrt::Windows::System::VirtualKey::Escape)
        {
            e.Handled(true);
            SetResult(K7_OVERWRITE_DIALOG_RESULT_CANCEL);
        }
    }
}
