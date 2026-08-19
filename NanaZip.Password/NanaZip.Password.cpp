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
        std::wstring& response)
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
        const DWORD timeout = config.TimeoutSeconds * 1000;
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

    bool QueryCloudPassword(
        const std::wstring& archivePath,
        std::wstring& password)
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
        if (!HttpPost(config, L"/api/v3/search/info", envelope, &accessHeader, response))
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
        const bool cloudFirst = ReadRegistryDword(kMatchPriority) != 0;
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
}
