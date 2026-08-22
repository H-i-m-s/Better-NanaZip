#ifndef NANAZIP_PASSWORD_CLIENT_H
#define NANAZIP_PASSWORD_CLIENT_H

#include <Windows.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
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

    // Adds a password to the local password book (LocalState/passwords.txt,
    // UTF-8, one entry per line). Idempotent: an exact duplicate is left
    // untouched and still counts as success. Returns false for an empty
    // password or a write failure.
    bool AddPasswordToBook(const std::wstring& password);

    // Reads the automatic password lookup settings from the FileManager
    // registry. MatchPriority is 0 for local-first and nonzero for
    // cloud-first.
    void ReadAutomaticPasswordSettings(
        bool& queryCloud,
        bool& matchLocal,
        DWORD& matchPriority);

    // Reads the FileManager settings and creates the automatic candidate list
    // for one archive. Local and cloud candidates follow MatchPriority.
    // A failed or incomplete cloud lookup is intentionally silent.
    void BuildAutomaticCandidates(
        const std::wstring& archivePath,
        std::vector<Candidate>& candidates);

    // Performs the configured cloud lookup for the archive. Returns false for
    // incomplete configuration, invalid HTTPS transport, timeout, or an
    // invalid response. It never exposes a password through logs or files.
    // Queries the remote password service for one archive. timeoutSeconds
    // (0 = the configured timeout) caps every HTTP stage, which keeps the
    // batch prefetch thread from stalling the session teardown.
    bool QueryCloudPassword(
        const std::wstring& archivePath,
        std::wstring& password,
        DWORD timeoutSeconds = 0);

    // Shares a password only when called by a caller that already established
    // user authorization and real extraction success. Failure is silent.
    bool SharePassword(
        const std::wstring& archivePath,
        const std::wstring& password);

    bool LoadApiConfig(ApiConfig& config);

    // ---- Batch password prefetch session (FileManager <-> 7zG) ----
    //
    // The FileManager creates one session per batch extraction run. A
    // prefetch thread publishes every archive's local password-book
    // candidates and its cloud lookup result into the session; 7zG workers
    // (one process per archive) ask the session for the current archive's
    // candidates over a local named pipe. Passwords never cross the
    // command line, temporary files, or logs.

    // A random session id identifying one batch run. Passed to 7zG on its
    // command line; it contains no password material.
    std::wstring GeneratePasswordSessionId();

    // The named pipe path for one session.
    std::wstring PasswordSessionPipeName(
        const std::wstring& sessionId);

    // The candidate payload for one archive, exchanged over the pipe.
    struct BatchCandidates
    {
        std::vector<std::wstring> LocalCandidates;
        std::wstring CloudPassword; // empty = none available
        // true = the password book has been loaded and LocalCandidates is
        // the complete snapshot, including the valid empty-book case.
        // false = the FM worker has not published the book yet.
        bool LocalReady = false;
        // true = the cloud lookup for this archive has finished (its
        // result may still be empty). false = still in flight; the 7zG
        // prefetch worker retries shortly instead of giving up.
        bool CloudReady = false;
    };

    // Server side (FileManager). Owns the candidate store that the
    // prefetch thread fills and serves 7zG requests on a named pipe.
    class BatchSession
    {
    public:
        // archivePaths fixes the order used to route requests.
        BatchSession(
            const std::wstring& sessionId,
            const std::vector<std::wstring>& archivePaths);
        ~BatchSession();

        const std::wstring& SessionId() const { return m_sessionId; }
        const std::wstring& PipeName() const { return m_pipeName; }

        // Prefetch thread: publishes the shared password-book candidates
        // once and each archive's cloud result as it arrives.
        void PublishLocalCandidates(
            const std::vector<std::wstring>& candidates);
        void PublishCloudResult(
            size_t index,
            const std::wstring& cloudPassword);

        // Pipe listener thread: accepts one 7zG connection (blocking,
        // waking on the stop event), hands it to a dedicated client
        // thread and immediately returns to accept the next one.
        bool ServeConnection();

        // One 7zG request handler: reads the request, waits for the
        // prefetch thread's cloud result, writes the response and waits
        // for the client to read it before disconnecting.
        void HandleClient(HANDLE pipe, HANDLE connectEvent);

        // Marks the session stopped and unblocks listeners/waiters.
        void Stop();

        // True once Stop() was called (lets workers bail out early).
        bool IsStopped() const { return this->m_stopped.load(); }

        // Returns the archive index matching a path (case-insensitive),
        // or -1 when not found.
        int IndexOf(const std::wstring& archivePath) const;

        size_t ArchiveCount() const { return m_archivePaths.size(); }

    private:
        std::wstring m_sessionId;
        std::wstring m_pipeName;
        std::vector<std::wstring> m_archivePaths;
        mutable std::mutex m_mutex;
        mutable std::condition_variable m_cv;
        std::vector<std::wstring> m_localCandidates;
        bool m_localReady;
        std::vector<std::wstring> m_cloudPasswords;
        std::vector<bool> m_cloudReady;
        std::vector<std::thread> m_clientThreads;
        std::atomic<bool> m_stopped;
        HANDLE m_stopEvent;
    };

    // Client side (7zG): asks the FileManager session for one archive's
    // candidates. Returns false when the pipe is unavailable or the
    // request times out.
    bool RequestBatchCandidates(
        const std::wstring& sessionId,
        const std::wstring& archivePath,
        DWORD timeoutMs,
        BatchCandidates& candidates);

    // Owns a session for the duration of one batch extraction call: starts
    // the prefetch thread (password book once, cloud per archive) and the
    // pipe listener, and stops both when destroyed. Both the FileManager
    // loop and the shared ExtractArchives entry use it.
    class BatchSessionScope
    {
    public:
        // sessionId empty -> a new random id is generated.
        explicit BatchSessionScope(
            const std::vector<std::wstring>& archivePaths,
            const std::wstring& sessionId = std::wstring());
        ~BatchSessionScope();
        BatchSessionScope(const BatchSessionScope&) = delete;
        BatchSessionScope& operator=(const BatchSessionScope&) = delete;

        const std::wstring& SessionId() const { return m_session.SessionId(); }
        const std::wstring& PipeName() const { return m_session.PipeName(); }

    private:
        BatchSession m_session;
        std::thread m_prefetchThread;
        std::thread m_pipeThread;
    };
}

#endif
