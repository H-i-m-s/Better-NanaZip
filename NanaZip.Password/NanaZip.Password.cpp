#include "NanaZip.Password.h"

#include <appmodel.h>
#include <bcrypt.h>
#include <shlobj.h>
#include <wincrypt.h>
#include <winhttp.h>

#include <cstdio>
#include <cstdlib>
#include <cwctype>
#include <vector>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <random>
#include <thread>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

namespace
{
    constexpr wchar_t kApiConfigFileName[] = L"api_config.txt";
    constexpr wchar_t kPasswordBookFileName[] = L"passwords.txt";
    constexpr wchar_t kNanaZipPackageFamilyName[] =
        L"SSS.NanaZip.RemotePassword_t9byekn60qs4j";
    constexpr wchar_t kRegistryPath[] = L"Software\\NanaZip\\FM";
    constexpr wchar_t kAutoQueryCloud[] = L"AutoQueryCloud";
    constexpr wchar_t kAutoMatchLocal[] = L"AutoMatchLocal";
    constexpr wchar_t kMatchPriority[] = L"MatchPriority";

    // Diagnostic log for the batch password session (server and client
    // sides). Only events and error codes are recorded; passwords never
    // appear here. A separate file keeps it free of cross-process writes
    // with the 7zG extraction trace.
    static void SssBatchDiagLog(const wchar_t* event, DWORD error)
    {
        wchar_t tempPath[MAX_PATH] = {};
        if (::GetTempPathW(MAX_PATH, tempPath) == 0)
        {
            return;
        }
        std::wstring path = tempPath;
        path += L"k7batch_session_diag.log";
        HANDLE file = ::CreateFileW(
            path.c_str(),
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            return;
        }
        wchar_t buffer[192];
        const int length = ::wsprintfW(
            buffer, L"[Q4-S] %s err=%lu\n", event, error);
        DWORD written = 0;
        ::WriteFile(file, buffer, length * sizeof(wchar_t),
            &written, nullptr);
        ::CloseHandle(file);
    }

    struct BcryptObject
    {
        BCRYPT_ALG_HANDLE Handle = nullptr;

        ~BcryptObject()
        {
            if (Handle)
            {
                BCryptCloseAlgorithmProvider(Handle, 0);
            }
        }
    };

    struct BcryptKey
    {
        BCRYPT_KEY_HANDLE Handle = nullptr;

        ~BcryptKey()
        {
            if (Handle)
            {
                BCryptDestroyKey(Handle);
            }
        }
    };

    static bool IsNtSuccess(NTSTATUS status)
    {
        return status >= 0;
    }

    static bool ReadFileBytes(
        const std::wstring& path,
        std::vector<BYTE>& bytes)
    {
        bytes.clear();
        HANDLE file = ::CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        LARGE_INTEGER length = {};
        const bool lengthOk = ::GetFileSizeEx(file, &length) != FALSE;
        if (!lengthOk || length.QuadPart < 0 || length.QuadPart > 16 * 1024 * 1024)
        {
            ::CloseHandle(file);
            return false;
        }

        bytes.resize(static_cast<size_t>(length.QuadPart));
        DWORD read = 0;
        const bool readOk = bytes.empty() || ::ReadFile(
            file,
            bytes.data(),
            static_cast<DWORD>(bytes.size()),
            &read,
            nullptr) != FALSE;
        ::CloseHandle(file);
        if (!readOk || read != bytes.size())
        {
            bytes.clear();
            return false;
        }
        return true;
    }

    static bool Utf8ToWide(
        const BYTE* bytes,
        size_t length,
        std::wstring& text)
    {
        text.clear();
        if (length >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
        {
            bytes += 3;
            length -= 3;
        }
        if (length == 0)
        {
            return true;
        }
        if (length > static_cast<size_t>(INT_MAX))
        {
            return false;
        }
        const int count = ::MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            reinterpret_cast<const char*>(bytes),
            static_cast<int>(length),
            nullptr,
            0);
        if (count <= 0)
        {
            return false;
        }
        text.resize(static_cast<size_t>(count));
        return ::MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            reinterpret_cast<const char*>(bytes),
            static_cast<int>(length),
            text.data(),
            count) == count;
    }

    static bool WideToUtf8(
        const std::wstring& text,
        std::vector<BYTE>& bytes)
    {
        bytes.clear();
        if (text.empty())
        {
            return true;
        }
        if (text.size() > static_cast<size_t>(INT_MAX))
        {
            return false;
        }
        const int count = ::WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0,
            nullptr,
            nullptr);
        if (count <= 0)
        {
            return false;
        }
        bytes.resize(static_cast<size_t>(count));
        return ::WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            reinterpret_cast<char*>(bytes.data()),
            count,
            nullptr,
            nullptr) == count;
    }

    static bool GetLocalStatePath(
        const wchar_t* name,
        std::wstring& path)
    {
        path.clear();
        PWSTR localAppData = nullptr;
        if (FAILED(::SHGetKnownFolderPath(
            FOLDERID_LocalAppData,
            0,
            nullptr,
            &localAppData)))
        {
            return false;
        }

        UINT32 bufferLength = 0;
        UINT32 count = 0;
        const LONG firstResult = ::GetCurrentPackageInfo(
            PACKAGE_FILTER_HEAD | PACKAGE_FILTER_DIRECT,
            &bufferLength,
            nullptr,
            &count);
        if (firstResult != ERROR_INSUFFICIENT_BUFFER || bufferLength == 0)
        {
            path.assign(localAppData);
            ::CoTaskMemFree(localAppData);
            path += L"\\Packages\\";
            path += kNanaZipPackageFamilyName;
            path += L"\\LocalState\\";
            path += name;
            return true;
        }

        std::vector<BYTE> packageBuffer(bufferLength);
        if (::GetCurrentPackageInfo(
            PACKAGE_FILTER_HEAD | PACKAGE_FILTER_DIRECT,
            &bufferLength,
            packageBuffer.data(),
            &count) != ERROR_SUCCESS)
        {
            path.assign(localAppData);
            ::CoTaskMemFree(localAppData);
            path += L"\\Packages\\";
            path += kNanaZipPackageFamilyName;
            path += L"\\LocalState\\";
            path += name;
            return true;
        }

        const auto* package = reinterpret_cast<const PACKAGE_INFO*>(packageBuffer.data());
        const wchar_t* packageFamilyName =
            (package->packageFamilyName && package->packageFamilyName[0])
                ? package->packageFamilyName
                : kNanaZipPackageFamilyName;

        path.assign(localAppData);
        ::CoTaskMemFree(localAppData);
        path += L"\\Packages\\";
        path += packageFamilyName;
        path += L"\\LocalState\\";
        path += name;
        return true;
    }

    static void SplitLines(
        const std::wstring& text,
        std::vector<std::wstring>& lines)
    {
        lines.clear();
        size_t start = 0;
        const size_t len = text.size();
        while (start <= len)
        {
            // Find LF, CRLF, or CR separators for legacy password files.
            size_t end = start;
            while (end < len && text[end] != L'\n' && text[end] != L'\r')
            {
                end++;
            }
            lines.emplace_back(text.substr(start, end - start));
            // Skip consecutive separators, including CRLF as one break.
            while (end < len && (text[end] == L'\n' || text[end] == L'\r'))
            {
                end++;
            }
            start = end;
            if (end >= len)
            {
                break;
            }
        }
    }

    static bool ReadTextFile(
        const wchar_t* name,
        std::wstring& text)
    {
        std::wstring path;
        std::vector<BYTE> bytes;
        return GetLocalStatePath(name, path)
            && ReadFileBytes(path, bytes)
            && Utf8ToWide(bytes.data(), bytes.size(), text);
    }

    static bool ReadRegistryBool(const wchar_t* name)
    {
        DWORD value = 0;
        DWORD size = sizeof(value);
        return ::RegGetValueW(
            HKEY_CURRENT_USER,
            kRegistryPath,
            name,
            RRF_RT_REG_DWORD,
            nullptr,
            &value,
            &size) == ERROR_SUCCESS && value != 0;
    }

    static DWORD ReadRegistryDword(const wchar_t* name)
    {
        DWORD value = 0;
        DWORD size = sizeof(value);
        if (::RegGetValueW(
            HKEY_CURRENT_USER,
            kRegistryPath,
            name,
            RRF_RT_REG_DWORD,
            nullptr,
            &value,
            &size) != ERROR_SUCCESS)
        {
            return 0;
        }
        return value;
    }

    static void ReadAutomaticPasswordSettings(
        bool& queryCloud,
        bool& matchLocal,
        DWORD& matchPriority)
    {
        queryCloud = ReadRegistryBool(kAutoQueryCloud);
        matchLocal = ReadRegistryBool(kAutoMatchLocal);
        matchPriority = ReadRegistryDword(kMatchPriority);
    }

    static bool Base64Encode(
        const std::vector<BYTE>& input,
        std::wstring& output)
    {
        output.clear();
        DWORD length = 0;
        if (!::CryptBinaryToStringW(
            input.data(),
            static_cast<DWORD>(input.size()),
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
            nullptr,
            &length))
        {
            return false;
        }
        output.resize(length);
        if (!::CryptBinaryToStringW(
            input.data(),
            static_cast<DWORD>(input.size()),
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
            output.data(),
            &length))
        {
            output.clear();
            return false;
        }
        if (!output.empty() && output.back() == L'\0')
        {
            output.pop_back();
        }
        return true;
    }

    static bool Base64Decode(
        const std::wstring& input,
        std::vector<BYTE>& output)
    {
        output.clear();
        DWORD length = 0;
        if (!::CryptStringToBinaryW(
            input.c_str(),
            static_cast<DWORD>(input.size()),
            CRYPT_STRING_BASE64,
            nullptr,
            &length,
            nullptr,
            nullptr))
        {
            return false;
        }
        output.resize(length);
        return ::CryptStringToBinaryW(
            input.c_str(),
            static_cast<DWORD>(input.size()),
            CRYPT_STRING_BASE64,
            output.data(),
            &length,
            nullptr,
            nullptr) != FALSE;
    }

    static bool UnprotectConfigValue(
        const std::wstring& stored,
        std::wstring& plain)
    {
        plain.clear();
        constexpr wchar_t kProtectedValuePrefix[] = L"dpapi:";
        if (stored.compare(0, ARRAYSIZE(kProtectedValuePrefix) - 1,
            kProtectedValuePrefix) != 0)
        {
            plain = stored; // Backward-compatible read of existing settings.
            return true;
        }
        std::vector<BYTE> bytes;
        if (!Base64Decode(stored.substr(ARRAYSIZE(kProtectedValuePrefix) - 1),
            bytes))
        {
            return false;
        }
        DATA_BLOB input = {};
        input.pbData = bytes.data();
        input.cbData = static_cast<DWORD>(bytes.size());
        DATA_BLOB output = {};
        if (!::CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
            CRYPTPROTECT_UI_FORBIDDEN, &output))
        {
            ::SecureZeroMemory(bytes.data(), bytes.size());
            return false;
        }
        const bool converted = Utf8ToWide(output.pbData, output.cbData, plain);
        ::SecureZeroMemory(output.pbData, output.cbData);
        ::LocalFree(output.pbData);
        ::SecureZeroMemory(bytes.data(), bytes.size());
        return converted;
    }

    static bool AesCbc(
        const std::vector<BYTE>& key,
        const std::vector<BYTE>& iv,
        const std::vector<BYTE>& input,
        bool encrypt,
        std::vector<BYTE>& output)
    {
        output.clear();
        if (iv.size() != 16 || (key.size() != 16 && key.size() != 24 && key.size() != 32))
        {
            return false;
        }

        BcryptObject algorithm;
        if (!IsNtSuccess(::BCryptOpenAlgorithmProvider(
            &algorithm.Handle,
            BCRYPT_AES_ALGORITHM,
            nullptr,
            0)) ||
            !IsNtSuccess(::BCryptSetProperty(
                algorithm.Handle,
                BCRYPT_CHAINING_MODE,
                reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_CBC)),
                static_cast<ULONG>(sizeof(BCRYPT_CHAIN_MODE_CBC)),
                0)))
        {
            return false;
        }

        DWORD objectLength = 0;
        DWORD resultLength = 0;
        if (!IsNtSuccess(::BCryptGetProperty(
            algorithm.Handle,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectLength),
            sizeof(objectLength),
            &resultLength,
            0)))
        {
            return false;
        }

        std::vector<BYTE> keyObject(objectLength);
        BcryptKey cryptoKey;
        if (!IsNtSuccess(::BCryptGenerateSymmetricKey(
            algorithm.Handle,
            &cryptoKey.Handle,
            keyObject.data(),
            static_cast<ULONG>(keyObject.size()),
            const_cast<PUCHAR>(key.data()),
            static_cast<ULONG>(key.size()),
            0)))
        {
            return false;
        }

        std::vector<BYTE> ivCopy(iv);
        ULONG outputLength = 0;
        // PKCS7 padding must be requested for both encryption and decryption.
        const ULONG flags = BCRYPT_BLOCK_PADDING;
        const NTSTATUS sizingStatus = encrypt
            ? ::BCryptEncrypt(
                cryptoKey.Handle,
                const_cast<PUCHAR>(input.data()),
                static_cast<ULONG>(input.size()),
                nullptr,
                ivCopy.data(),
                static_cast<ULONG>(ivCopy.size()),
                nullptr,
                0,
                &outputLength,
                flags)
            : ::BCryptDecrypt(
                cryptoKey.Handle,
                const_cast<PUCHAR>(input.data()),
                static_cast<ULONG>(input.size()),
                nullptr,
                ivCopy.data(),
                static_cast<ULONG>(ivCopy.size()),
                nullptr,
                0,
                &outputLength,
                flags);
        if (!IsNtSuccess(sizingStatus))
        {
            return false;
        }

        output.resize(outputLength);
        ivCopy = iv;
        ULONG actualLength = 0;
        const NTSTATUS processStatus = encrypt
            ? ::BCryptEncrypt(
                cryptoKey.Handle,
                const_cast<PUCHAR>(input.data()),
                static_cast<ULONG>(input.size()),
                nullptr,
                ivCopy.data(),
                static_cast<ULONG>(ivCopy.size()),
                output.data(),
                static_cast<ULONG>(output.size()),
                &actualLength,
                flags)
            : ::BCryptDecrypt(
                cryptoKey.Handle,
                const_cast<PUCHAR>(input.data()),
                static_cast<ULONG>(input.size()),
                nullptr,
                ivCopy.data(),
                static_cast<ULONG>(ivCopy.size()),
                output.data(),
                static_cast<ULONG>(output.size()),
                &actualLength,
                flags);
        if (!IsNtSuccess(processStatus))
        {
            output.clear();
            return false;
        }
        output.resize(actualLength);
        return true;
    }

    static bool HmacSha1(
        const std::vector<BYTE>& key,
        const std::vector<BYTE>& message,
        std::vector<BYTE>& digest)
    {
        digest.clear();
        BcryptObject algorithm;
        if (!IsNtSuccess(::BCryptOpenAlgorithmProvider(
            &algorithm.Handle,
            BCRYPT_SHA1_ALGORITHM,
            nullptr,
            BCRYPT_ALG_HANDLE_HMAC_FLAG)))
        {
            return false;
        }

        DWORD objectLength = 0;
        DWORD hashLength = 0;
        DWORD resultLength = 0;
        if (!IsNtSuccess(::BCryptGetProperty(
                algorithm.Handle,
                BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PUCHAR>(&objectLength),
                sizeof(objectLength),
                &resultLength,
                0)) ||
            !IsNtSuccess(::BCryptGetProperty(
                algorithm.Handle,
                BCRYPT_HASH_LENGTH,
                reinterpret_cast<PUCHAR>(&hashLength),
                sizeof(hashLength),
                &resultLength,
                0)))
        {
            return false;
        }

        std::vector<BYTE> hashObject(objectLength);
        BCRYPT_HASH_HANDLE hash = nullptr;
        if (!IsNtSuccess(::BCryptCreateHash(
            algorithm.Handle,
            &hash,
            hashObject.data(),
            static_cast<ULONG>(hashObject.size()),
            const_cast<PUCHAR>(key.data()),
            static_cast<ULONG>(key.size()),
            0)))
        {
            return false;
        }
        const NTSTATUS hashStatus = ::BCryptHashData(
            hash,
            const_cast<PUCHAR>(message.data()),
            static_cast<ULONG>(message.size()),
            0);
        digest.resize(hashLength);
        const NTSTATUS finishStatus = IsNtSuccess(hashStatus)
            ? ::BCryptFinishHash(
                hash,
                digest.data(),
                static_cast<ULONG>(digest.size()),
                0)
            : hashStatus;
        ::BCryptDestroyHash(hash);
        if (!IsNtSuccess(finishStatus))
        {
            digest.clear();
            return false;
        }
        return true;
    }

    static std::wstring JsonEscape(const std::wstring& value)
    {
        std::wstring escaped;
        for (wchar_t ch : value)
        {
            switch (ch)
            {
            case L'\\': escaped += L"\\\\"; break;
            case L'\"': escaped += L"\\\""; break;
            case L'\b': escaped += L"\\b"; break;
            case L'\f': escaped += L"\\f"; break;
            case L'\n': escaped += L"\\n"; break;
            case L'\r': escaped += L"\\r"; break;
            case L'\t': escaped += L"\\t"; break;
            default:
                if (ch < 0x20)
                {
                    wchar_t buffer[7] = {};
                    swprintf_s(buffer, L"\\u%04X", static_cast<unsigned>(ch));
                    escaped += buffer;
                }
                else
                {
                    escaped += ch;
                }
                break;
            }
        }
        return escaped;
    }

    static bool FindJsonString(
        const std::wstring& json,
        const wchar_t* name,
        std::wstring& value)
    {
        value.clear();
        const std::wstring key = std::wstring(L"\"") + name + L"\"";
        size_t position = json.find(key);
        if (position == std::wstring::npos)
        {
            return false;
        }
        position = json.find(L':', position + key.size());
        if (position == std::wstring::npos)
        {
            return false;
        }
        ++position;
        while (position < json.size() && std::iswspace(json[position]))
        {
            ++position;
        }
        if (position >= json.size() || json[position] != L'\"')
        {
            return false;
        }
        ++position;
        while (position < json.size())
        {
            const wchar_t ch = json[position++];
            if (ch == L'\"')
            {
                return true;
            }
            if (ch != L'\\')
            {
                value += ch;
                continue;
            }
            if (position >= json.size())
            {
                return false;
            }
            const wchar_t escaped = json[position++];
            switch (escaped)
            {
            case L'\"': value += L'\"'; break;
            case L'\\': value += L'\\'; break;
            case L'/': value += L'/'; break;
            case L'b': value += L'\b'; break;
            case L'f': value += L'\f'; break;
            case L'n': value += L'\n'; break;
            case L'r': value += L'\r'; break;
            case L't': value += L'\t'; break;
            default: return false;
            }
        }
        return false;
    }

    static bool HasHeaderControlCharacter(const std::wstring& value)
    {
        return value.find(L'\r') != std::wstring::npos ||
            value.find(L'\n') != std::wstring::npos;
    }

    static bool IsHttpsUrl(const std::wstring& url)
    {
        wchar_t host[256] = {};
        wchar_t path[2048] = {};
        URL_COMPONENTS components = {};
        components.dwStructSize = sizeof(components);
        components.lpszHostName = host;
        components.dwHostNameLength = ARRAYSIZE(host);
        components.lpszUrlPath = path;
        components.dwUrlPathLength = ARRAYSIZE(path);
        return ::WinHttpCrackUrl(
            url.c_str(),
            static_cast<DWORD>(url.size()),
            0,
            &components) != FALSE &&
            components.nScheme == INTERNET_SCHEME_HTTPS &&
            components.dwHostNameLength != 0;
    }

    static bool HttpPost(
        const NanaZipPassword::ApiConfig& config,
        const wchar_t* relativePath,
        const std::wstring& body,
        const std::wstring* accessHeader,
        std::wstring& response,
        DWORD timeoutSeconds = 0)
    {
        response.clear();
        if (!IsHttpsUrl(config.Url))
        {
            return false;
        }

        wchar_t hostBuffer[256] = {};
        wchar_t pathBuffer[2048] = {};
        URL_COMPONENTS components = {};
        components.dwStructSize = sizeof(components);
        components.lpszHostName = hostBuffer;
        components.dwHostNameLength = ARRAYSIZE(hostBuffer);
        components.lpszUrlPath = pathBuffer;
        components.dwUrlPathLength = ARRAYSIZE(pathBuffer);
        if (!::WinHttpCrackUrl(
            config.Url.c_str(),
            static_cast<DWORD>(config.Url.size()),
            0,
            &components) ||
            components.nScheme != INTERNET_SCHEME_HTTPS ||
            components.dwHostNameLength == 0 ||
            HasHeaderControlCharacter(config.AppId) ||
            HasHeaderControlCharacter(config.ProtocolVersion))
        {
            return false;
        }

        std::wstring host(hostBuffer, components.dwHostNameLength);
        std::wstring basePath;
        if (components.dwUrlPathLength > 0)
        {
            basePath.assign(pathBuffer, components.dwUrlPathLength);
        }
        if (basePath.empty())
        {
            basePath = L"/";
        }
        if (basePath.back() == L'/')
        {
            basePath.pop_back();
        }
        std::wstring requestPath = basePath;
        requestPath += relativePath;

        HINTERNET session = ::WinHttpOpen(
            L"NanaZip/RemotePassword",
            WINHTTP_ACCESS_TYPE_NO_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);
        if (!session)
        {
            return false;
        }
        // timeoutSeconds != 0 overrides the configured timeout: the batch
        // prefetch thread must never stall the session teardown for long.
        const DWORD timeout = (timeoutSeconds != 0
            ? timeoutSeconds : config.TimeoutSeconds) * 1000;
        ::WinHttpSetTimeouts(session, timeout, timeout, timeout, timeout);
        HINTERNET connection = ::WinHttpConnect(
            session,
            host.c_str(),
            components.nPort,
            0);
        if (!connection)
        {
            ::WinHttpCloseHandle(session);
            return false;
        }
        HINTERNET request = ::WinHttpOpenRequest(
            connection,
            L"POST",
            requestPath.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);
        if (!request)
        {
            ::WinHttpCloseHandle(connection);
            ::WinHttpCloseHandle(session);
            return false;
        }

        DWORD redirects = WINHTTP_DISABLE_REDIRECTS;
        ::WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &redirects, sizeof(redirects));

        std::wstring headers = L"Content-Type: application/json\r\nappid: ";
        headers += config.AppId;
        headers += L"\r\nversion: ";
        headers += config.ProtocolVersion;
        headers += L"\r\nplatform: windows\r\n";
        if (accessHeader)
        {
            headers += L"User-Client-Access: ";
            headers += *accessHeader;
            headers += L"\r\n";
        }

        std::vector<BYTE> bodyBytes;
        const bool bodyOk = WideToUtf8(body, bodyBytes);
        const bool sendOk = bodyOk && ::WinHttpSendRequest(
            request,
            headers.c_str(),
            static_cast<DWORD>(headers.size()),
            bodyBytes.empty() ? WINHTTP_NO_REQUEST_DATA : bodyBytes.data(),
            static_cast<DWORD>(bodyBytes.size()),
            static_cast<DWORD>(bodyBytes.size()),
            0) != FALSE;
        const bool receiveOk = sendOk && ::WinHttpReceiveResponse(request, nullptr) != FALSE;
        DWORD status = 0;
        DWORD statusSize = sizeof(status);
        const bool statusOk = receiveOk && ::WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX) != FALSE && status >= 200 && status < 300;
        std::vector<BYTE> responseBytes;
        if (statusOk)
        {
            for (;;)
            {
                DWORD available = 0;
                if (!::WinHttpQueryDataAvailable(request, &available) || available == 0)
                {
                    break;
                }
                const size_t previous = responseBytes.size();
                responseBytes.resize(previous + available);
                DWORD read = 0;
                if (!::WinHttpReadData(
                    request,
                    responseBytes.data() + previous,
                    available,
                    &read) || read == 0)
                {
                    responseBytes.clear();
                    break;
                }
                responseBytes.resize(previous + read);
            }
        }
        ::WinHttpCloseHandle(request);
        ::WinHttpCloseHandle(connection);
        ::WinHttpCloseHandle(session);
        return statusOk && Utf8ToWide(
            responseBytes.data(),
            responseBytes.size(),
            response);
    }

    static bool CreateEncryptedEnvelope(
        const NanaZipPassword::ApiConfig& config,
        const std::wstring& plainJson,
        std::wstring& envelope)
    {
        envelope.clear();
        std::vector<BYTE> key;
        std::vector<BYTE> plain;
        if (!WideToUtf8(config.AesKey, key) ||
            !WideToUtf8(plainJson, plain) ||
            (key.size() != 16 && key.size() != 24 && key.size() != 32))
        {
            return false;
        }
        std::vector<BYTE> iv(16);
        if (!IsNtSuccess(::BCryptGenRandom(
            nullptr,
            iv.data(),
            static_cast<ULONG>(iv.size()),
            BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
        {
            return false;
        }
        std::vector<BYTE> cipher;
        std::wstring encodedCipher;
        std::wstring encodedIv;
        if (!AesCbc(key, iv, plain, true, cipher) ||
            !Base64Encode(cipher, encodedCipher) ||
            !Base64Encode(iv, encodedIv))
        {
            return false;
        }
        envelope = L"{\"data\":\"" + JsonEscape(encodedCipher)
            + L"\",\"iv\":\"" + JsonEscape(encodedIv) + L"\"}";
        return true;
    }

    static bool CreateAccessHeader(
        const NanaZipPassword::ApiConfig& config,
        const std::wstring& fileName,
        ULONGLONG fileSize,
        std::wstring& header)
    {
        std::wstring size = std::to_wstring(fileSize);
        std::wstring message = config.Fingerprint + config.PackageName + fileName
            + size + config.Fingerprint + L"127";
        std::vector<BYTE> key;
        std::vector<BYTE> messageBytes;
        std::vector<BYTE> digest;
        return WideToUtf8(config.SigningKey, key)
            && WideToUtf8(message, messageBytes)
            && HmacSha1(key, messageBytes, digest)
            && Base64Encode(digest, header);
    }

    static bool GetArchiveInfo(
        const std::wstring& path,
        std::wstring& name,
        ULONGLONG& size)
    {
        WIN32_FILE_ATTRIBUTE_DATA data = {};
        if (!::GetFileAttributesExW(
            path.c_str(),
            GetFileExInfoStandard,
            &data))
        {
            return false;
        }
        size = (static_cast<ULONGLONG>(data.nFileSizeHigh) << 32)
            | data.nFileSizeLow;
        const size_t separator = path.find_last_of(L"\\/");
        name = separator == std::wstring::npos ? path : path.substr(separator + 1);
        return !name.empty();
    }
}

namespace NanaZipPassword
{
    ApiConfig::ApiConfig() :
        ProtocolVersion(L"2.2.3"),
        TimeoutSeconds(5)
    {
    }

    bool ApiConfig::IsComplete() const
    {
        return !Url.empty() && !AppId.empty() && !AesKey.empty()
            && !SigningKey.empty() && !PackageName.empty() && !Fingerprint.empty();
    }

    bool LoadApiConfig(ApiConfig& config)
    {
        config = ApiConfig();
        std::wstring text;
        if (!ReadTextFile(kApiConfigFileName, text))
        {
            return false;
        }

        std::vector<std::wstring> lines;
        SplitLines(text, lines);
        for (const auto& line : lines)
        {
            if (line.empty() || line[0] == L'#')
            {
                continue;
            }
            const size_t separator = line.find(L'=');
            if (separator == std::wstring::npos)
            {
                continue;
            }
            const std::wstring key = line.substr(0, separator);
            const std::wstring value = line.substr(separator + 1);
            if (key == L"CloudApiUrl") config.Url = value;
            else if (key == L"CloudAppId" && !UnprotectConfigValue(value, config.AppId)) return false;
            else if (key == L"CloudAesKey" && !UnprotectConfigValue(value, config.AesKey)) return false;
            else if (key == L"CloudSigningKey" && !UnprotectConfigValue(value, config.SigningKey)) return false;
            else if (key == L"CloudPackageName" && !UnprotectConfigValue(value, config.PackageName)) return false;
            else if (key == L"CloudFingerprint" && !UnprotectConfigValue(value, config.Fingerprint)) return false;
            else if (key == L"CloudProtocolVersion" && !value.empty()) config.ProtocolVersion = value;
            else if (key == L"CloudTimeoutSeconds")
            {
                const unsigned long parsed = wcstoul(value.c_str(), nullptr, 10);
                if (parsed >= 1 && parsed <= 30)
                {
                    config.TimeoutSeconds = parsed;
                }
            }
        }
        return true;
    }

    void ReadAutomaticPasswordSettings(
        bool& queryCloud,
        bool& matchLocal,
        DWORD& matchPriority)
    {
        ::ReadAutomaticPasswordSettings(queryCloud, matchLocal, matchPriority);
    }

    bool LoadLocalCandidates(std::vector<Candidate>& candidates)
    {
        candidates.clear();
        std::wstring text;
        if (!ReadTextFile(kPasswordBookFileName, text))
        {
            return false;
        }
        std::vector<std::wstring> lines;
        SplitLines(text, lines);
        for (auto& line : lines)
        {
            if (!line.empty() && line.back() == L'\r')
            {
                line.pop_back();
            }
            if (!line.empty() && line[0] != L'#')
            {
                candidates.push_back({ std::move(line), PasswordSource::Local });
            }
        }
        return true;
    }

    bool AddPasswordToBook(const std::wstring& password)
    {
        if (password.empty())
        {
            return false;
        }
        std::wstring path;
        if (!GetLocalStatePath(kPasswordBookFileName, path))
        {
            return false;
        }
        // Read the current book (it may not exist yet). The readers use
        // FILE_SHARE_READ | FILE_SHARE_WRITE, so appending never breaks a
        // concurrent load in the other process.
        std::vector<BYTE> bytes;
        const bool fileExists = ReadFileBytes(path, bytes);
        if (fileExists)
        {
            // Idempotent: an exact duplicate is left untouched.
            std::wstring text;
            if (Utf8ToWide(bytes.data(), bytes.size(), text))
            {
                size_t start = 0;
                const size_t len = text.size();
                while (start <= len)
                {
                    size_t end = start;
                    while (end < len &&
                        text[end] != L'\n' && text[end] != L'\r')
                    {
                        end++;
                    }
                    if (text.substr(start, end - start) == password)
                    {
                        return true;
                    }
                    while (end < len &&
                        (text[end] == L'\n' || text[end] == L'\r'))
                    {
                        end++;
                    }
                    start = end;
                    if (end >= len)
                    {
                        break;
                    }
                }
            }
        }
        std::vector<BYTE> utf8;
        if (!WideToUtf8(password, utf8))
        {
            return false;
        }
        // Append: keep every entry on its own line. OPEN_ALWAYS creates the
        // file when it does not exist and never truncates an existing one.
        HANDLE h = ::CreateFileW(
            path.c_str(),
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (h == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        bool ok = false;
        // The previous line may not end with a newline; keep the book
        // line-separated so LoadLocalCandidates sees clean entries.
        if (fileExists && !bytes.empty() && bytes.back() != '\n')
        {
            const char lf = '\n';
            DWORD written = 0;
            ::WriteFile(h, &lf, 1, &written, nullptr);
        }
        if (!utf8.empty())
        {
            DWORD written = 0;
            ok = ::WriteFile(h, utf8.data(),
                static_cast<DWORD>(utf8.size()), &written, nullptr) != FALSE;
        }
        const char lf = '\n';
        DWORD written2 = 0;
        ok = ::WriteFile(h, &lf, 1, &written2, nullptr) != FALSE && ok;
        ::CloseHandle(h);
        return ok;
    }

    bool QueryCloudPassword(
        const std::wstring& archivePath,
        std::wstring& password,
        DWORD timeoutSeconds)
    {
        password.clear();
        ApiConfig config;
        if (!LoadApiConfig(config) || !config.IsComplete())
        {
            return false;
        }
        std::wstring name;
        ULONGLONG size = 0;
        if (!GetArchiveInfo(archivePath, name, size))
        {
            return false;
        }
        std::wstring accessHeader;
        std::wstring envelope;
        const std::wstring plain = L"{\"fileName\":\"" + JsonEscape(name)
            + L"\",\"fileSize\":" + std::to_wstring(size) + L"}";
        if (!CreateAccessHeader(config, name, size, accessHeader) ||
            !CreateEncryptedEnvelope(config, plain, envelope))
        {
            return false;
        }
        std::wstring response;
        if (!HttpPost(config, L"/api/v3/search/info", envelope,
            &accessHeader, response, timeoutSeconds))
        {
            return false;
        }
        std::wstring data;
        std::wstring iv;
        std::vector<BYTE> cipher;
        std::vector<BYTE> ivBytes;
        std::vector<BYTE> key;
        std::vector<BYTE> plainBytes;
        std::wstring plainResponse;
        if (!FindJsonString(response, L"data", data) ||
            !FindJsonString(response, L"iv", iv) ||
            !Base64Decode(data, cipher) ||
            !Base64Decode(iv, ivBytes) ||
            !WideToUtf8(config.AesKey, key) ||
            !AesCbc(key, ivBytes, cipher, false, plainBytes) ||
            !Utf8ToWide(plainBytes.data(), plainBytes.size(), plainResponse) ||
            !FindJsonString(plainResponse, L"pd", password) || password.empty())
        {
            password.clear();
            return false;
        }
        return true;
    }

    void BuildAutomaticCandidates(
        const std::wstring& archivePath,
        std::vector<Candidate>& candidates)
    {
        candidates.clear();
        std::vector<Candidate> local;
        std::wstring cloud;
        const bool useLocal = ReadRegistryBool(kAutoMatchLocal);
        const bool useCloud = ReadRegistryBool(kAutoQueryCloud);
        if (useLocal)
        {
            LoadLocalCandidates(local);
        }
        const bool cloudFound = useCloud && QueryCloudPassword(archivePath, cloud);
        const bool cloudFirst = ReadRegistryDword(kMatchPriority) == 1;
        if (cloudFirst && cloudFound)
        {
            candidates.push_back({ std::move(cloud), PasswordSource::Cloud });
        }
        candidates.insert(candidates.end(), local.begin(), local.end());
        if (!cloudFirst && cloudFound)
        {
            candidates.push_back({ std::move(cloud), PasswordSource::Cloud });
        }
    }

    bool SharePassword(
        const std::wstring& archivePath,
        const std::wstring& password)
    {
        if (password.empty())
        {
            return false;
        }
        ApiConfig config;
        if (!LoadApiConfig(config) || !config.IsComplete())
        {
            return false;
        }
        std::wstring name;
        ULONGLONG size = 0;
        if (!GetArchiveInfo(archivePath, name, size))
        {
            return false;
        }
        const std::wstring plain = L"{\"fileName\":\"" + JsonEscape(name)
            + L"\",\"fileSize\":" + std::to_wstring(size)
            + L",\"pd\":\"" + JsonEscape(password)
            + L"\",\"region\":\"CN\"}";
        std::wstring envelope;
        std::wstring response;
        std::wstring code;
        return CreateEncryptedEnvelope(config, plain, envelope)
            && HttpPost(config, L"/api/v3/sync/ads", envelope, nullptr, response)
            && FindJsonString(response, L"code", code)
            && code == L"200";
    }

    std::wstring GeneratePasswordSessionId()
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        wchar_t buf[40];
        swprintf_s(buf, L"%016llx%016llx",
            static_cast<unsigned long long>(gen()),
            static_cast<unsigned long long>(gen()));
        return buf;
    }

    std::wstring PasswordSessionPipeName(
        const std::wstring& sessionId)
    {
        // Named pipes use the \\.\pipe\ prefix; \\?\pipe\ is not a
        // valid pipe name and would make CreateNamedPipe/CreateFile fail.
        return L"\\\\.\\pipe\\NanaZip.Pwd." + sessionId;
    }

    BatchSession::BatchSession(
        const std::wstring& sessionId,
        const std::vector<std::wstring>& archivePaths) :
        m_sessionId(sessionId),
        m_pipeName(PasswordSessionPipeName(sessionId)),
        m_archivePaths(archivePaths),
        m_localReady(false),
        m_cloudPasswords(archivePaths.size()),
        m_cloudReady(archivePaths.size(), false),
        m_stopped(false),
        m_stopEvent(::CreateEventW(nullptr, TRUE, FALSE, nullptr))
    {
    }

    BatchSession::~BatchSession()
    {
        this->Stop();
        // Handler threads are woken by the stop event (or finish their
        // request) and then exit on their own; join them all before the
        // session members go away.
        SssBatchDiagLog(L"[Q4-S] client join begin", 0);
        for (auto& thread : this->m_clientThreads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
        SssBatchDiagLog(L"[Q4-S] client join done", 0);
        if (this->m_stopEvent)
        {
            ::CloseHandle(this->m_stopEvent);
        }
    }

    void BatchSession::Stop()
    {
        this->m_stopped.store(true);
        this->m_cv.notify_all();
        if (this->m_stopEvent)
        {
            ::SetEvent(this->m_stopEvent);
        }
    }

    void BatchSession::PublishLocalCandidates(
        const std::vector<std::wstring>& candidates)
    {
        std::lock_guard<std::mutex> lock(this->m_mutex);
        this->m_localCandidates = candidates;
        this->m_localReady = true;
        this->m_cv.notify_all();
    }

    void BatchSession::PublishCloudResult(
        size_t index,
        const std::wstring& cloudPassword)
    {
        std::lock_guard<std::mutex> lock(this->m_mutex);
        if (index < this->m_cloudReady.size())
        {
            this->m_cloudPasswords[index] = cloudPassword;
            this->m_cloudReady[index] = true;
        }
        this->m_cv.notify_all();
    }

    int BatchSession::IndexOf(
        const std::wstring& archivePath) const
    {
        for (size_t i = 0; i < this->m_archivePaths.size(); ++i)
        {
            if (::_wcsicmp(archivePath.c_str(),
                this->m_archivePaths[i].c_str()) == 0)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    bool BatchSession::ServeConnection()
    {
        // Once the session is stopping, never accept a new connection:
        // the File Manager tears the scope down as soon as 7zG exits, and
        // a late client would make the pipe thread join hang.
        if (this->m_stopped.load())
        {
            return false;
        }
        SssBatchDiagLog(L"[Q4-SV] pipe create", 0);
        HANDLE pipe = ::CreateNamedPipeW(
            this->m_pipeName.c_str(),
            // FILE_FLAG_OVERLAPPED is mandatory: without it the pipe
            // handle is synchronous, ConnectNamedPipe blocks until a
            // client connects and ignores the OVERLAPPED/stop event, so
            // the listener could never be woken during teardown (the
            // File Manager would hang joining this thread).
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            8192,
            8192,
            5000,
            nullptr);
        if (pipe == INVALID_HANDLE_VALUE)
        {
            SssBatchDiagLog(L"[Q4-SV] pipe create failed",
                ::GetLastError());
            return false;
        }
        HANDLE connectEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!connectEvent)
        {
            ::CloseHandle(pipe);
            return false;
        }
        OVERLAPPED overlapped = {};
        overlapped.hEvent = connectEvent;
        BOOL connected = ::ConnectNamedPipe(pipe, &overlapped);
        DWORD connectError = ::GetLastError();
        if (!connected && connectError == ERROR_PIPE_CONNECTED)
        {
            connected = TRUE;
        }
        else if (!connected && connectError == ERROR_IO_PENDING)
        {
            SssBatchDiagLog(L"[Q4-SV] pipe connect wait", 0);
            HANDLE waitHandles[2] = { connectEvent, this->m_stopEvent };
            const DWORD wait = ::WaitForMultipleObjects(
                2, waitHandles, FALSE, INFINITE);
            SssBatchDiagLog(
                wait == WAIT_OBJECT_0 ? L"[Q4-SV] pipe connect event"
                : L"[Q4-SV] pipe connect stopped",
                wait);
            if (wait == WAIT_OBJECT_0)
            {
                DWORD unused = 0;
                ::GetOverlappedResult(pipe, &overlapped, &unused, FALSE);
                connected = TRUE;
            }
            else
            {
                ::CancelIo(pipe);
            }
        }
        if (!connected)
        {
            SssBatchDiagLog(L"[Q4-SV] connect failed",
                ::GetLastError());
            ::CloseHandle(connectEvent);
            ::CloseHandle(pipe);
            return false;
        }

        SssBatchDiagLog(L"[Q4-SV] client connected", 0);

        // Hand the connection to a dedicated handler thread and return
        // immediately: 7zG may open several archives / files concurrently
        // (multi-threaded extraction), so the listener must never block on
        // one request while another client is waiting. The handler owns
        // pipe and connectEvent and closes them when done.
        {
            std::lock_guard<std::mutex> lock(this->m_mutex);
            this->m_clientThreads.push_back(std::thread(
                &BatchSession::HandleClient, this, pipe, connectEvent));
            SssBatchDiagLog(L"[Q4-S] client thread started",
                static_cast<DWORD>(this->m_clientThreads.size()));
        }
        return true;
    }

    void BatchSession::HandleClient(HANDLE pipe, HANDLE connectEvent)
    {
        SssBatchDiagLog(L"[Q4-S] client handler begin", 0);
        // Read the request: UINT32 pathByteLen + UTF-16 path bytes.
        // Every wait also watches the stop event so an aborted client can
        // never block the session teardown (Stop() + thread join).
        auto readExact = [&](void* buffer, DWORD bytes) -> bool
        {
            OVERLAPPED overlapped = {};
            overlapped.hEvent =
                ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (!overlapped.hEvent)
            {
                return false;
            }
            DWORD read = 0;
            const BOOL result =
                ::ReadFile(pipe, buffer, bytes, nullptr, &overlapped);
            DWORD error = ::GetLastError();
            bool done = false;
            if (!result && error == ERROR_IO_PENDING)
            {
                HANDLE waitHandles[2] = { overlapped.hEvent,
                    this->m_stopEvent };
                const DWORD wait = ::WaitForMultipleObjects(
                    2, waitHandles, FALSE, INFINITE);
                if (wait == WAIT_OBJECT_0)
                {
                    ::GetOverlappedResult(pipe, &overlapped, &read, FALSE);
                    done = (read == bytes);
                }
                else
                {
                    ::CancelIo(pipe);
                }
            }
            else if (result)
            {
                done = true;
            }
            ::CloseHandle(overlapped.hEvent);
            return done;
        };
        // Stop-aware write: a client that stops reading (or a teardown)
        // must never block this handler forever.
        auto writeExact = [&](const void* buffer, DWORD bytes) -> bool
        {
            OVERLAPPED overlapped = {};
            overlapped.hEvent =
                ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (!overlapped.hEvent)
            {
                return false;
            }
            DWORD written = 0;
            const BOOL result =
                ::WriteFile(pipe, buffer, bytes, nullptr, &overlapped);
            DWORD error = ::GetLastError();
            bool done = false;
            if (!result && error == ERROR_IO_PENDING)
            {
                HANDLE waitHandles[2] = { overlapped.hEvent,
                    this->m_stopEvent };
                const DWORD wait = ::WaitForMultipleObjects(
                    2, waitHandles, FALSE, INFINITE);
                if (wait == WAIT_OBJECT_0)
                {
                    ::GetOverlappedResult(pipe, &overlapped, &written, FALSE);
                    done = (written == bytes);
                }
                else
                {
                    ::CancelIo(pipe);
                }
            }
            else if (result)
            {
                done = true;
            }
            ::CloseHandle(overlapped.hEvent);
            return done;
        };

        UINT32 pathBytes = 0;
        if (readExact(&pathBytes, sizeof(pathBytes)) &&
            pathBytes <= 65536 &&
            (pathBytes & 1) == 0)
        {
            SssBatchDiagLog(L"[Q4-S] client header ok", 0);
            std::vector<wchar_t> path(pathBytes / sizeof(wchar_t) + 1, 0);
            if (pathBytes == 0 ||
                readExact(path.data(), pathBytes))
            {
                SssBatchDiagLog(L"[Q4-S] client path ok", 0);
                const int index = this->IndexOf(path.data());
                if (index < 0)
                {
                    SssBatchDiagLog(L"[Q4-SV] path not indexed", 0);
                }
                std::vector<std::wstring> local;
                std::wstring cloud;
                bool localReady = false;
                bool cloudReady = false;
                {
                    std::lock_guard<std::mutex> lock(this->m_mutex);
                    local = this->m_localCandidates;
                    localReady = this->m_localReady;
                    if (index >= 0)
                    {
                        // Cloud lookups run in parallel in the prefetch
                        // worker; never block a client on one. When the
                        // result is not ready yet the response carries an
                        // empty cloud field with cloudReady=false and the
                        // 7zG prefetch worker retries shortly.
                        if (this->m_cloudReady[index])
                        {
                            cloud = this->m_cloudPasswords[index];
                            cloudReady = true;
                        }
                    }
                }

                // Write the response: UINT32 localCount, then per
                // candidate UINT32 len + UTF-16 bytes, then a local-ready
                // flag, then UINT32 cloudByteLen + UTF-16 bytes. Write
                // failures are ignored:
                // the client may already be gone (stop-aware, so a
                // teardown cannot block here either).
                UINT32 count = static_cast<UINT32>(local.size());
                writeExact(&count, sizeof(count));
                for (const std::wstring& candidate : local)
                {
                    const UINT32 len =
                        static_cast<UINT32>(candidate.size());
                    writeExact(&len, sizeof(len));
                    if (!candidate.empty())
                    {
                        writeExact(candidate.c_str(),
                            len * sizeof(wchar_t));
                    }
                }
                {
                    wchar_t message[180] = {};
                    swprintf_s(message,
                        L"[Q4-S] local response count=%u ready=%u",
                        count, localReady ? 1u : 0u);
                    SssBatchDiagLog(message, 0);
                }
                const UINT32 localReadyFlag = localReady ? 1u : 0u;
                writeExact(&localReadyFlag, sizeof(localReadyFlag));
                const UINT32 cloudByteLen = static_cast<UINT32>(
                    cloud.size() * sizeof(wchar_t));
                writeExact(&cloudByteLen, sizeof(cloudByteLen));
                if (!cloud.empty())
                {
                    writeExact(cloud.c_str(), cloudByteLen);
                }
                // Trailing flag: whether the cloud lookup has finished
                // (its result may still be empty). The 7zG prefetch worker
                // uses it to stop retrying once the lookup completed.
                const UINT32 cloudReadyFlag = cloudReady ? 1u : 0u;
                writeExact(&cloudReadyFlag, sizeof(cloudReadyFlag));

                // The response is fully written. Do NOT disconnect here:
                // DisconnectNamedPipe drops buffered data the client has
                // not read yet, and the client needs a moment to start its
                // read (the server can win the race by microseconds). Wait
                // for the client to read everything and close its handle
                // instead; this read returns when the client is gone, and
                // the stop event still wakes it during teardown.
                SssBatchDiagLog(L"[Q4-S] client response written", 0);
                BYTE tail = 0;
                (void)readExact(&tail, 1);
                SssBatchDiagLog(L"[Q4-S] client tail done", 0);
            }
        }
        else
        {
            SssBatchDiagLog(L"[Q4-SV] read request failed",
                ::GetLastError());
        }

        ::DisconnectNamedPipe(pipe);
        ::CloseHandle(connectEvent);
        ::CloseHandle(pipe);
        SssBatchDiagLog(L"[Q4-S] client handler done", 0);
    }

    bool RequestBatchCandidates(
        const std::wstring& sessionId,
        const std::wstring& archivePath,
        DWORD timeoutMs,
        BatchCandidates& candidates)
    {
        candidates.LocalCandidates.clear();
        candidates.CloudPassword.clear();
        candidates.LocalReady = false;
        candidates.CloudReady = false;
        const std::wstring pipeName =
            PasswordSessionPipeName(sessionId);

        HANDLE pipe = INVALID_HANDLE_VALUE;
        for (int attempt = 0; attempt < 50; ++attempt)
        {
            pipe = ::CreateFileW(
                pipeName.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED,
                nullptr);
            if (pipe != INVALID_HANDLE_VALUE)
            {
                break;
            }
            const DWORD error = ::GetLastError();
            if (error == ERROR_PIPE_BUSY)
            {
                if (!::WaitNamedPipeW(pipeName.c_str(), 200))
                {
                    break;
                }
            }
            else if (error != ERROR_FILE_NOT_FOUND)
            {
                // Access denied and similar failures will not heal.
                break;
            }
            else
            {
                // The server may not have created the pipe yet.
                ::Sleep(100);
            }
        }
        if (pipe == INVALID_HANDLE_VALUE)
        {
            SssBatchDiagLog(L"[Q4-CL] pipe open failed",
                ::GetLastError());
            return false;
        }

        // Message-mode reads match the server's PIPE_TYPE_MESSAGE
        // instance; byte mode would also work for reading a message
        // stream, but being explicit keeps the pairing well-defined.
        DWORD pipeMode = PIPE_READMODE_MESSAGE;
        ::SetNamedPipeHandleState(pipe, &pipeMode, nullptr, nullptr);

        // Send the request.
        const UINT32 pathBytes =
            static_cast<UINT32>(archivePath.size() * sizeof(wchar_t));
        DWORD written = 0;
        bool ok = ::WriteFile(pipe, &pathBytes, sizeof(pathBytes),
                &written, nullptr) &&
            ::WriteFile(pipe, archivePath.c_str(), pathBytes,
                &written, nullptr);
        if (!ok)
        {
            SssBatchDiagLog(L"[Q4-CL] write failed",
                ::GetLastError());
            ::CloseHandle(pipe);
            return false;
        }

        // Read the response with a bounded timeout.
        auto readBytes = [&](void* buffer, DWORD bytes) -> bool
        {
            DWORD read = 0;
            OVERLAPPED overlapped = {};
            overlapped.hEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (!overlapped.hEvent)
            {
                return false;
            }
            const BOOL result =
                ::ReadFile(pipe, buffer, bytes, nullptr, &overlapped);
            DWORD error = ::GetLastError();
            if (!result && error == ERROR_IO_PENDING)
            {
                if (::WaitForSingleObject(overlapped.hEvent, timeoutMs)
                    != WAIT_OBJECT_0)
                {
                    SssBatchDiagLog(L"[Q4-CL] read timeout",
                        WAIT_TIMEOUT);
                    ::CancelIo(pipe);
                    ::CloseHandle(overlapped.hEvent);
                    return false;
                }
                ::GetOverlappedResult(pipe, &overlapped, &read, FALSE);
            }
            else if (result)
            {
                read = bytes;
            }
            else
            {
                SssBatchDiagLog(L"[Q4-CL] read failed", error);
                ::CloseHandle(overlapped.hEvent);
                return false;
            }
            ::CloseHandle(overlapped.hEvent);
            return read == bytes;
        };

        UINT32 count = 0;
        ok = readBytes(&count, sizeof(count));
        if (ok && count <= 4096)
        {
            candidates.LocalCandidates.reserve(count);
            for (UINT32 i = 0; i < count && ok; ++i)
            {
                UINT32 len = 0;
                ok = readBytes(&len, sizeof(len));
                if (!ok || len > 1024)
                {
                    ok = false;
                    break;
                }
                std::wstring value(len, L'\0');
                if (len > 0)
                {
                    ok = readBytes(&value[0], len * sizeof(wchar_t));
                }
                if (ok)
                {
                    candidates.LocalCandidates.push_back(
                        std::move(value));
                }
            }
        }
        if (ok)
        {
            wchar_t message[180] = {};
            swprintf_s(message,
                L"[Q4-CL] local response count=%zu",
                candidates.LocalCandidates.size());
            SssBatchDiagLog(message, 0);
        }
        if (ok)
        {
            UINT32 localReadyFlag = 0;
            ok = readBytes(&localReadyFlag, sizeof(localReadyFlag));
            candidates.LocalReady = (ok && localReadyFlag != 0);
        }
        if (ok)
        {
            UINT32 cloudByteLen = 0;
            ok = readBytes(&cloudByteLen, sizeof(cloudByteLen));
            if (ok && cloudByteLen <= 2048 &&
                (cloudByteLen % sizeof(wchar_t)) == 0)
            {
                if (cloudByteLen > 0)
                {
                    // The protocol length is UTF-16 bytes on both sides.
                    candidates.CloudPassword.assign(
                        cloudByteLen / sizeof(wchar_t), L'\0');
                    ok = readBytes(&candidates.CloudPassword[0],
                        cloudByteLen);
                }
            }
            else
            {
                ok = false;
            }
        }
        if (ok)
        {
            // Trailing ready flag: the server writes it after the cloud
            // password (see HandleClient). Without it the client cannot
            // tell "lookup still in flight" apart from "lookup done, no
            // password", so this field is mandatory.
            UINT32 cloudReadyFlag = 0;
            ok = readBytes(&cloudReadyFlag, sizeof(cloudReadyFlag));
            candidates.CloudReady = (ok && cloudReadyFlag != 0);
        }
        ::CloseHandle(pipe);
        return ok;
    }

    namespace
    {
        // Prefetch worker: publishes the password book once (shared by
        // every archive) and queries the cloud for every archive in
        // parallel (bounded concurrency). A lookup can take seconds
        // (user-configured timeout), so they all run while the current
        // archive's 7zG process is still extracting; serial lookups would
        // make every later archive wait behind earlier ones. When the
        // session stops, no new lookup is started and the in-flight ones
        // finish within the configured timeout, so teardown stays bounded.
        void BatchPrefetchWorker(
            BatchSession* session,
            std::vector<std::wstring> paths,
            bool queryCloud,
            bool matchLocal)
        {
            std::vector<std::wstring> local;
            if (matchLocal)
            {
                std::vector<Candidate> candidates;
                LoadLocalCandidates(candidates);
                for (const auto& c : candidates)
                {
                    local.push_back(c.Value);
                }
            }
            // Always publish the local side, including the disabled and
            // empty-book cases. LocalReady then means "the local decision
            // is settled", not "the candidate list is non-empty".
            {
                wchar_t message[160] = {};
                swprintf_s(message,
                    L"[Q4-S] local candidates published count=%zu",
                    local.size());
                SssBatchDiagLog(message, 0);
            }
            for (size_t i = 0; i < local.size(); ++i)
            {
                wchar_t message[160] = {};
                swprintf_s(message,
                    L"[Q4-S] local candidate published index=%zu units=%zu",
                    i, local[i].size());
                SssBatchDiagLog(message, 0);
            }
            session->PublishLocalCandidates(local);
            if (!queryCloud)
            {
                // No cloud lookups at all: mark every archive ready with
                // an empty password so clients never wait on the flag.
                for (size_t i = 0; i < paths.size(); ++i)
                {
                    session->PublishCloudResult(i, std::wstring());
                }
                return;
            }

            // Bounded parallel lookups: at most kMaxCloudConcurrency
            // queries in flight at once. Each archive gets its own thread
            // so one slow lookup never delays the rest.
            constexpr size_t kMaxCloudConcurrency = 4;
            std::mutex slotMutex;
            std::condition_variable slotCv;
            size_t active = 0;
            size_t next = 0;
            std::vector<std::thread> workers;
            for (;;)
            {
                std::unique_lock<std::mutex> lock(slotMutex);
                while (active >= kMaxCloudConcurrency)
                {
                    if (session->IsStopped())
                    {
                        break;
                    }
                    slotCv.wait(lock);
                }
                if (session->IsStopped() || next >= paths.size())
                {
                    break;
                }
                const size_t index = next++;
                const std::wstring path = paths[index];
                ++active;
                workers.emplace_back(
                    [&slotMutex, &slotCv, &active, session, index, path]()
                {
                    std::wstring cloud;
                    // The configured timeout (api_config.txt
                    // CloudTimeoutSeconds) already caps every HTTP stage
                    // including DNS; honor the user's value.
                    QueryCloudPassword(path, cloud);
                    session->PublishCloudResult(index, cloud);
                    {
                        std::lock_guard<std::mutex> lock(slotMutex);
                        --active;
                    }
                    slotCv.notify_one();
                });
            }
            for (auto& worker : workers)
            {
                if (worker.joinable())
                {
                    worker.join();
                }
            }
        }

        // Pipe listener worker: serves one connection per archive until
        // the session is stopped (ServeConnection polls the stop flag).
        void BatchPipeWorker(BatchSession* session)
        {
            while (session->ServeConnection())
            {
            }
        }
    }

    BatchSessionScope::BatchSessionScope(
        const std::vector<std::wstring>& archivePaths,
        const std::wstring& sessionId) :
        m_session(sessionId.empty()
            ? GeneratePasswordSessionId() : sessionId, archivePaths)
    {
        bool queryCloud = false;
        bool matchLocal = false;
        DWORD priority = 0;
        ReadAutomaticPasswordSettings(queryCloud, matchLocal, priority);
        m_prefetchThread = std::thread(
            BatchPrefetchWorker, &m_session, archivePaths,
            queryCloud, matchLocal);
        m_pipeThread = std::thread(BatchPipeWorker, &m_session);
    }

    BatchSessionScope::~BatchSessionScope()
    {
        SssBatchDiagLog(L"[Q4-S] scope stop begin", 0);
        m_session.Stop();
        if (m_prefetchThread.joinable())
        {
            m_prefetchThread.join();
            SssBatchDiagLog(L"[Q4-S] scope prefetch joined", 0);
        }
        if (m_pipeThread.joinable())
        {
            m_pipeThread.join();
            SssBatchDiagLog(L"[Q4-S] scope pipe joined", 0);
        }
        SssBatchDiagLog(L"[Q4-S] scope stop done", 0);
    }
}
