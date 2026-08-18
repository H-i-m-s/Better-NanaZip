#ifndef NANAZIP_PASSWORD_CLIENT_H
#define NANAZIP_PASSWORD_CLIENT_H

#include <Windows.h>

#include <string>
#include <vector>

namespace NanaZipPassword
{
    enum class PasswordSource : UINT32
    {
        None = 0,
        Manual = 1,
        Local = 2,
        Cloud = 3,
        CommandLine = 4
    };

    struct ApiConfig
    {
        std::wstring Url;
        std::wstring AppId;
        std::wstring AesKey;
        std::wstring SigningKey;
        std::wstring PackageName;
        std::wstring Fingerprint;
        std::wstring ProtocolVersion;
        DWORD TimeoutSeconds;

        ApiConfig();
        bool IsComplete() const;
    };

    struct Candidate
    {
        std::wstring Value;
        PasswordSource Source;
    };

    // Reads LocalState/passwords.txt without modifying it. Empty lines and
    // lines starting with '#' are excluded, while all remaining entries stay
    // in their original order.
    bool LoadLocalCandidates(std::vector<Candidate>& candidates);

    // Reads the FileManager settings and creates the automatic candidate list
    // for one archive. Local and cloud candidates follow MatchPriority.
    // A failed or incomplete cloud lookup is intentionally silent.
    void BuildAutomaticCandidates(
        const std::wstring& archivePath,
        std::vector<Candidate>& candidates);

    // Performs the configured cloud lookup for the archive. Returns false for
    // incomplete configuration, invalid HTTPS transport, timeout, or an
    // invalid response. It never exposes a password through logs or files.
    bool QueryCloudPassword(
        const std::wstring& archivePath,
        std::wstring& password);

    // Shares a password only when called by a caller that already established
    // user authorization and real extraction success. Failure is silent.
    bool SharePassword(
        const std::wstring& archivePath,
        const std::wstring& password);

    bool LoadApiConfig(ApiConfig& config);
}

#endif
