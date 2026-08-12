/*
 * PROJECT:    NanaZip.Modern
 * FILE:       NanaZip.Modern.h
 * PURPOSE:    Definition for NanaZip Modern Experience
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef NANAZIP_MODERN_EXPERIENCE
#define NANAZIP_MODERN_EXPERIENCE

#include <Windows.h>
#include <CommCtrl.h>

/**
 * @brief Get legacy string resource from NanaZip Modern Experience resources.
 * @param ResourceId The legacy string resource ID.
 * @return If the legacy string resource is found, it returns the pointer to the
 *         string. Otherwise, it returns nullptr.
 * @remark This function can be used without calling K7ModernInitialize.
 */
EXTERN_C LPCWSTR WINAPI K7ModernGetLegacyStringResource(
    _In_ UINT32 ResourceId);

/**
 * @brief Check whether NanaZip Modern Experience is available.
 * @return If NanaZip Modern Experience is available, it returns TRUE.
 *         Otherwise, it returns FALSE.
 */
EXTERN_C BOOL WINAPI K7ModernAvailable();

/**
 * @brief Initialize NanaZip Modern Experience.
 * @return If the function succeeds, it returns S_OK. Otherwise, it returns an
 *         HRESULT error code.
 */
EXTERN_C HRESULT WINAPI K7ModernInitialize();

/**
 * @brief Uninitialize NanaZip Modern Experience.
 * @return If the function succeeds, it returns S_OK. Otherwise, it returns an
 *         HRESULT error code.
 */
EXTERN_C HRESULT WINAPI K7ModernUninitialize();

/**
 * @brief Show the "Sponsor NanaZip" dialog.
 * @param ParentWindowHandle A handle to the owner window of the dialog to be
 *                           created. If this parameter is nullptr, the dialog
 *                           has no owner window.
 * @return The message loop exit code of the dialog.
 */
EXTERN_C INT WINAPI K7ModernShowSponsorDialog(
    _In_opt_ HWND ParentWindowHandle);

/**
 * @brief Show the "About NanaZip" dialog.
 * @param ParentWindowHandle A handle to the owner window of the dialog to be
 *                           created. If this parameter is nullptr, the dialog
 *                           has no owner window.
 * @param ExtendedMessage The extended message to be displayed. If this
 *                        parameter is nullptr, the extended message is empty.
 * @return The message loop exit code of the dialog.
 */
EXTERN_C INT WINAPI K7ModernShowAboutDialog(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR ExtendedMessage);

/**
 * @brief Show an information dialog with the specified title and content.
 * @param ParentWindowHandle A handle to the owner window of the dialog to be
 *                           created. If this parameter is nullptr, the dialog
 *                           has no owner window.
 * @param Title The title of the information dialog. If this parameter is
 *              nullptr, the title is empty.
 * @param Content The content of the information dialog. If this parameter is
 *                nullptr, the content is empty.
 * @return The message loop exit code of the dialog.
 */
EXTERN_C INT WINAPI K7ModernShowInformationDialog(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR Title,
    _In_opt_ LPCWSTR Content);

/**
 * @brief The progress window status structure.
 */
typedef struct _K7_PROGRESS_WINDOW_STATUS
{
    /**
     * @brief If true, the progress is shown in bytes. If false, the progress is
     *        shown in files.
     */
    BOOLEAN BytesProgressMode;

    /**
     * @brief If true, the operation is compression. If false, the operation is
     *        extraction.
     */
    BOOLEAN CompressionMode;

    /**
     * @brief If true, there is an error. If false, there is no error.
     */
    BOOLEAN HaveError;

    /**
     * @brief The title of the progress window.
     */
    LPCWSTR Title;

    /**
     * @brief The current file path being processed.
     */
    LPCWSTR FilePath;

    /**
     * @brief The total size of the operation in bytes. If unknown, it is set to
     *        static_cast<UINT64>(-1).
     */
    UINT64 TotalSize;

    /**
     * @brief The processed size of the operation in bytes.
     */
    UINT64 ProcessedSize;

    /**
     * @brief The number of total files. If unknown, it is set to
     *        static_cast<UINT64>(-1).
     */
    UINT64 TotalFiles;

    /**
     * @brief The number of processed files.
     */
    UINT64 ProcessedFiles;

    /**
     * @brief The input size in bytes. For compression, it is the original size.
     *        For extraction, it is the compressed size. If unknown, it is set
     *        to static_cast<UINT64>(-1).
     */
    UINT64 InputSize;

    /**
     * @brief The output size in bytes. For compression, it is the compressed
     *        size. For extraction, it is the original size. If unknown, it is
     *        set to static_cast<UINT64>(-1).
     */
    UINT64 OutputSize;

    /**
     * @brief The status message.
     */
    LPCWSTR Status;
} K7_PROGRESS_WINDOW_STATUS, *PK7_PROGRESS_WINDOW_STATUS;

/**
 * @brief Update the progress window status.
 * @param WindowHandle A handle to the progress window which acquired from the
 *                     window subclass procedure.
 * @param Status The progress window status to be updated.
 * @remark You must call this function only in the window subclass procedure of
 *         the progress window.
 */
EXTERN_C VOID WINAPI K7ModernUpdateProgressWindowStatus(
    _In_ HWND WindowHandle,
    _In_ PK7_PROGRESS_WINDOW_STATUS Status);

/**
 * @brief The WM_COMMAND BN_CLICKED ID for the "Pause" button will be sent to
 *        the progress window when the "Pause" button is clicked.
 */
#define K7_PROGRESS_WINDOW_COMMAND_PAUSE 446

/**
 * @brief Set the paused mode of the progress window.
 * @param WindowHandle A handle to the progress window which acquired from the
 *                     window subclass procedure.
 * @param Paused If true, the UI of the progress window will be updated to the
 *               paused mode. If false, the UI of the progress window will be
 *               updated to the normal mode.
 * @remark You must call this function only in the window subclass procedure of
 *         the progress window.
 */
EXTERN_C VOID WINAPI K7ModernSetProgressWindowPausedMode(
    _In_ HWND WindowHandle,
    _In_ BOOL Paused);

/**
 * @brief Show the progress window.
 * @param ParentWindowHandle A handle to the owner window of the dialog to be
 *                           created. If this parameter is nullptr, the dialog
 *                           has no owner window.
 * @param Title The title of the progress window. If this parameter is
 *              nullptr, the title is empty.
 * @param ShowCompressionInformation If true, the progress window will show
 *                                   the packed size and compression ratio.
 *                                   Otherwise, the packed size and compression
 *                                   ratio will be hidden.
 * @param WindowSubclassHandler The window subclass procedure for the progress
 *                              window.
 * @param WindowSubclassContext The context pointer for the window subclass
 *                              procedure.
 * @return The message loop exit code of the dialog.
 */
EXTERN_C INT WINAPI K7ModernShowProgressWindow(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR Title,
    _In_ BOOL ShowCompressionInformation,
    _In_ SUBCLASSPROC WindowSubclassHandler,
    _In_ LPVOID WindowSubclassContext);

/**
 * @brief The WM_COMMAND BN_CLICKED ID for the "OK" button will be sent to
 *        the copy location dialog when the "OK" button is clicked.
 */
#define K7_COPY_LOCATION_DIALOG_RESULT_OK 1

/**
 * @brief Show the copy location dialog window.
 * @param ParentWindowHandle A handle to the owner window of the dialog to be
 *                           created. If this parameter is nullptr, the dialog
 *                           has no owner window.
 * @param Title The title of the copy location dialog window.
                If this parameter is nullptr, the title is empty.
 * @param Subtitle The subtitle of the copy location dialog window.
                   If this parameter is nullptr, the subtitle is empty.
 * @param AdditionalInformation The additional information text of
 *                              the copy location dialog window.
 *                              If this parameter is nullptr, the additional
                                information text is empty.
 * @param InitialPath The initial path set on the copy location dialog.
 * @param WindowSubclassHandler The window subclass procedure for the progress
 *                              window.
 * @param WindowSubclassContext The context pointer for the window subclass
 *                              procedure.
 * @return The message loop exit code of the dialog.
 */
EXTERN_C INT WINAPI K7ModernShowCopyLocationDialog(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR Title,
    _In_opt_ LPCWSTR Subtitle,
    _In_opt_ LPCWSTR AdditionalInformation,
    _In_opt_ LPCWSTR InitialPath,
    _In_ SUBCLASSPROC WindowSubclassHandler,
    _In_ LPVOID WindowSubclassContext);

/**
 * @brief Get the path of the copy location dialog.
 * @param WindowHandle A handle to the copy location dialog.
 * @return The path currently set.
 */
EXTERN_C LPCWSTR WINAPI K7ModernGetCopyLocationDialogPath(
    _In_ HWND WindowHandle);

/**
 * @brief Create the toolbar control for the main window.
 * @param ParentWindowHandle A handle to the owner window of the control to be
 *                           created. This parameter must be valid.
 * @param MoreMenuHandle A menu handle for the "More" menu. This parameter must
 *                       be valid.
 * @return The toolbar control instance pointer.
 */
EXTERN_C LPVOID WINAPI K7ModernCreateMainWindowToolBarPage(
    _In_ HWND ParentWindowHandle,
    _In_ HMENU MoreMenuHandle);

/**
 * @brief The callback invoked when the "Apply" button of the settings
 *        dialog is clicked. It runs on the UI thread while the dialog is
 *        open, so the caller may save the context and apply the settings
 *        immediately without closing the dialog.
 */
typedef VOID (WINAPI *K7_SETTINGS_APPLY_CALLBACK)(
    _In_ LPVOID ApplyContext);

/**
 * @brief The settings dialog context structure. The caller fills it with the
 *        current values before calling K7ModernShowSettingsDialog, and the
 *        dialog writes the user-modified values back into it.
 */
typedef struct _K7_SETTINGS_DIALOG_CONTEXT
{
    // --- Settings page ---
    BOOLEAN ShowDots;
    BOOLEAN ShowRealFileIcons;
    BOOLEAN FullRow;
    BOOLEAN ShowGrid;
    BOOLEAN SingleClick;
    BOOLEAN AlternativeSelection;
    BOOLEAN ShowSystemMenu;
    BOOLEAN LargePages;
    BOOLEAN LargePagesSupported;
    BOOLEAN ArcHistory;
    BOOLEAN PathHistory;
    BOOLEAN CopyHistory;
    BOOLEAN FolderHistory;
    BOOLEAN LowercaseHashes;
    BOOLEAN SizeFormat;
    UINT32 FontSizeAddressBar;
    UINT32 FontSizeList;
    UINT32 FontSizeStatusBar;
    UINT32 FontSizeDialog;
    UINT32 DefaultUiFontPt;

    // --- Integration (menu) page ---
    BOOLEAN ElimDup;
    BOOLEAN ExtractOnOpen;
    // 0..3, 0xFFFFFFFF means "Default" (write -1 to the registry).
    UINT32 WriteZone;
    BOOLEAN ContextFlags[13];
    WCHAR ContextNames[13][192];
    UINT32 ZoneSel;
    WCHAR ZoneItems[4][64];

    // --- Folders page ---
    // 0 = system temp, 1 = current, 2 = specified
    UINT32 WorkMode;
    BOOLEAN ForRemovableOnly;
    WCHAR WorkPath[MAX_PATH];

    // --- Editor page ---
    // 0 = viewer, 1 = editor, 2 = diff
    WCHAR EditorPaths[3][MAX_PATH];

    // --- Extract settings page ---
    BOOLEAN DeleteAfterExtract;
    BOOLEAN DeletePermanently;
    BOOLEAN AutoQueryCloud;
    BOOLEAN AutoMatchLocal;
    BOOLEAN AutoShowPassword;
    UINT32 MatchPriority;
    // True when the user modified any API configuration field; the caller
    // only saves the API config when this is set (lazy creation).
    BOOLEAN DirtyApi;
    WCHAR ApiUrl[256];
    WCHAR ApiAppId[256];
    WCHAR ApiAesKey[256];
    WCHAR ApiSigningKey[256];
    WCHAR ApiPackageName[256];
    WCHAR ApiFingerprint[256];
    WCHAR PasswordBook[4096];

    // --- Window state ---
    // Initial dialog rect in physical pixels. The caller fills it from the
    // saved registry state; pass a zero rect ({0}) to center the dialog on
    // the owner window. The dialog writes the final rect back here when it
    // is closed (position + size remembered for the next open).
    RECT WindowRect;
    // Initial tab index (0..4). The caller fills it from the saved registry
    // state; the dialog writes the last active tab back here when it closes
    // so the next open restores the tab the user was on.
    UINT32 LastTab;

    // --- Apply button ---
    // Optional callback invoked when the "Apply" button is clicked. If it is
    // null, the "Apply" button is hidden.
    K7_SETTINGS_APPLY_CALLBACK ApplyCallback;
    LPVOID ApplyContext;

    // --- Result ---
    BOOLEAN OK;
} K7_SETTINGS_DIALOG_CONTEXT, *PK7_SETTINGS_DIALOG_CONTEXT;

/**
 * @brief The overwrite dialog context structure. The caller fills it with
 *        the old/new file information before calling K7ModernShowOverwriteDialog,
 *        and the dialog writes the clicked button into the Result field.
 */
#define K7_OVERWRITE_DIALOG_RESULT_CANCEL      0
#define K7_OVERWRITE_DIALOG_RESULT_YES         1
#define K7_OVERWRITE_DIALOG_RESULT_YES_TO_ALL  2
#define K7_OVERWRITE_DIALOG_RESULT_NO          3
#define K7_OVERWRITE_DIALOG_RESULT_NO_TO_ALL   4
#define K7_OVERWRITE_DIALOG_RESULT_AUTO_RENAME 5

typedef struct _K7_OVERWRITE_DIALOG_CONTEXT
{
    // If true, the "Yes to All / No to All / Auto Rename" buttons are shown.
    BOOLEAN ShowExtraButtons;
    // If true, the "No" button is the default (focused) button.
    BOOLEAN DefaultIsNo;

    // Existing (old) file information.
    BOOLEAN OldSizeDefined;
    BOOLEAN OldTimeDefined;
    UINT64  OldSize;
    FILETIME OldTime;
    WCHAR   OldName[MAX_PATH];

    // Replacing (new) file information.
    BOOLEAN NewSizeDefined;
    BOOLEAN NewTimeDefined;
    UINT64  NewSize;
    FILETIME NewTime;
    WCHAR   NewName[MAX_PATH];

    // Dialog font size in points (0 = follow system), read from the registry
    // by the caller so the XAML page does not touch the registry.
    UINT32  FontSizeDialog;

    // The clicked button, one of the K7_OVERWRITE_DIALOG_RESULT_* values.
    // The caller should ignore it when the dialog is dismissed without a
    // button click (the X close button), which behaves like Cancel.
    UINT32  Result;
} K7_OVERWRITE_DIALOG_CONTEXT, *PK7_OVERWRITE_DIALOG_CONTEXT;

/**
 * @brief Show the overwrite (confirm file replace) dialog.
 * @param ParentWindowHandle A handle to the owner window of the dialog to be
 *                           created. If this parameter is nullptr, the dialog
 *                           has no owner window.
 * @param Context The overwrite dialog context. The caller fills it with the
 *                old/new file information before calling this function. The
 *                dialog writes the clicked button into it. The caller must
 *                keep it valid until this function returns.
 * @return The message loop exit code of the dialog.
 */
EXTERN_C INT WINAPI K7ModernShowOverwriteDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_OVERWRITE_DIALOG_CONTEXT Context);

// -----------------------------------------------------------------------------
// Compress dialog exchange types.
//
// These types intentionally contain only fixed-size Win32 ABI data. The
// Universal side owns all 7-Zip state and dynamic rules; the Modern page only
// renders the snapshot and sends semantic commands back through the callbacks.
// -----------------------------------------------------------------------------

#define K7_COMPRESS_MAX_OPTION_ITEMS 128
#define K7_COMPRESS_MAX_DISPLAY_TEXT 192
#define K7_COMPRESS_MAX_SEMANTIC_TEXT 128
#define K7_COMPRESS_MAX_ARCHIVE_PATH 32768
#define K7_COMPRESS_MAX_PARAMETERS 2048
#define K7_COMPRESS_MAX_VOLUME_TEXT 512
#define K7_COMPRESS_MAX_PASSWORD 256

#define K7_COMPRESS_OPTION_AUTO      0x00000001u
#define K7_COMPRESS_OPTION_DEFAULT   0x00000002u
#define K7_COMPRESS_OPTION_DISABLED  0x00000004u

#define K7_COMPRESS_COMMAND_FORMAT              1u
#define K7_COMPRESS_COMMAND_LEVEL               2u
#define K7_COMPRESS_COMMAND_METHOD              3u
#define K7_COMPRESS_COMMAND_DICTIONARY          4u
#define K7_COMPRESS_COMMAND_ORDER               5u
#define K7_COMPRESS_COMMAND_SOLID               6u
#define K7_COMPRESS_COMMAND_THREADS             7u
#define K7_COMPRESS_COMMAND_MEMORY              8u
#define K7_COMPRESS_COMMAND_UPDATE_MODE        9u
#define K7_COMPRESS_COMMAND_PATH_MODE          10u
#define K7_COMPRESS_COMMAND_ENCRYPTION_METHOD  11u
#define K7_COMPRESS_COMMAND_ARCHIVE_PATH       12u
#define K7_COMPRESS_COMMAND_PARAMETERS         13u
#define K7_COMPRESS_COMMAND_VOLUME             14u
#define K7_COMPRESS_COMMAND_SFX                15u
#define K7_COMPRESS_COMMAND_SHARED             16u
#define K7_COMPRESS_COMMAND_DELETE             17u
#define K7_COMPRESS_COMMAND_ENCRYPT_HEADERS    18u
#define K7_COMPRESS_COMMAND_SUBMIT             19u
#define K7_COMPRESS_COMMAND_BROWSE_ARCHIVE     20u

#define K7_COMPRESS_SUBMIT_OK       0
#define K7_COMPRESS_SUBMIT_REJECTED 1

/**
 * @brief One semantic item in a compression dialog option list.
 *
 * Value carries numeric semantics such as a format index, level, dictionary
 * size or thread count. SemanticText carries string semantics such as a
 * method name or a normalized memory-use specification. The display text is
 * never parsed back into a compression setting.
 */
typedef struct _K7_COMPRESS_OPTION_ITEM
{
    WCHAR DisplayText[K7_COMPRESS_MAX_DISPLAY_TEXT];
    WCHAR SemanticText[K7_COMPRESS_MAX_SEMANTIC_TEXT];
    INT64 Value;
    UINT32 Flags;
} K7_COMPRESS_OPTION_ITEM, *PK7_COMPRESS_OPTION_ITEM;

typedef struct _K7_COMPRESS_OPTION_LIST
{
    K7_COMPRESS_OPTION_ITEM Items[K7_COMPRESS_MAX_OPTION_ITEMS];
    UINT32 Count;
    INT32 SelectedIndex;
    BOOLEAN Visible;
    BOOLEAN Enabled;
} K7_COMPRESS_OPTION_LIST, *PK7_COMPRESS_OPTION_LIST;

/**
 * @brief Optional command callback owned by the Universal compression model.
 *
 * The callback may update the context snapshot in place. Returning FALSE
 * rejects the command; ErrorText can then be shown by the page. The callback
 * is invoked on the XAML UI thread while the dialog is open.
 */
typedef BOOLEAN (WINAPI *K7_COMPRESS_DIALOG_COMMAND_CALLBACK)(
    _In_ LPVOID CallbackContext,
    _In_ UINT32 Command,
    _In_ INT64 Value,
    _In_opt_ LPCWSTR SemanticText);

typedef BOOLEAN (WINAPI *K7_COMPRESS_DIALOG_OPTIONS_CALLBACK)(
    _In_ LPVOID CallbackContext);

/**
 * @brief The compression dialog context and current Universal-side snapshot.
 *
 * The caller fills the initial snapshot and callback pointers before calling
 * K7ModernShowCompressDialog. The page writes final primitive fields back and
 * sets OK only after the submit callback accepts the request.
 */
typedef struct _K7_COMPRESS_DIALOG_CONTEXT
{
    // --- Dynamic option snapshots ---
    K7_COMPRESS_OPTION_LIST Formats;
    K7_COMPRESS_OPTION_LIST Levels;
    K7_COMPRESS_OPTION_LIST Methods;
    K7_COMPRESS_OPTION_LIST Dictionaries;
    K7_COMPRESS_OPTION_LIST Orders;
    K7_COMPRESS_OPTION_LIST SolidBlocks;
    K7_COMPRESS_OPTION_LIST Threads;
    K7_COMPRESS_OPTION_LIST MemoryLimits;
    K7_COMPRESS_OPTION_LIST UpdateModes;
    K7_COMPRESS_OPTION_LIST PathModes;
    K7_COMPRESS_OPTION_LIST EncryptionMethods;
    K7_COMPRESS_OPTION_LIST Volumes;

    // --- Current semantic values ---
    INT32 FormatIndex;
    INT64 Level;
    INT64 Dictionary;
    INT64 Order;
    INT64 SolidBlock;
    INT64 ThreadsValue;
    INT64 MemoryLimit;
    UINT32 UpdateMode;
    UINT32 PathMode;
    BOOLEAN SfxMode;
    BOOLEAN OpenShareForWrite;
    BOOLEAN DeleteAfterCompressing;
    BOOLEAN EncryptHeaders;
    BOOLEAN ShowPassword;
    BOOLEAN EncryptHeadersAllowed;

    // --- Capability and display state ---
    BOOLEAN SfxVisible;
    BOOLEAN SfxEnabled;
    BOOLEAN EncryptionVisible;
    BOOLEAN EncryptionEnabled;
    BOOLEAN VolumeVisible;
    BOOLEAN ParametersVisible;
    BOOLEAN OptionsEnabled;
    BOOLEAN MemoryVisible;
    BOOLEAN DecompressMemoryVisible;
    WCHAR ArchiveFolderText[256];
    WCHAR HardwareThreadsText[256];
    WCHAR MemoryValueText[256];
    WCHAR DecompressMemoryText[256];
    WCHAR OptionsSummaryText[1024];
    WCHAR ErrorText[1024];

    // --- Editable text ---
    WCHAR ArchivePath[K7_COMPRESS_MAX_ARCHIVE_PATH];
    WCHAR Parameters[K7_COMPRESS_MAX_PARAMETERS];
    WCHAR VolumeText[K7_COMPRESS_MAX_VOLUME_TEXT];
    WCHAR Password[K7_COMPRESS_MAX_PASSWORD];
    WCHAR PasswordConfirmation[K7_COMPRESS_MAX_PASSWORD];

    // --- Dialog behavior ---
    UINT32 FontSizeDialog;
    LONG MinTrackW;
    LONG MinTrackH;
    BOOLEAN OK;

    K7_COMPRESS_DIALOG_COMMAND_CALLBACK CommandCallback;
    K7_COMPRESS_DIALOG_OPTIONS_CALLBACK OptionsCallback;
    LPVOID CallbackContext;
} K7_COMPRESS_DIALOG_CONTEXT, *PK7_COMPRESS_DIALOG_CONTEXT;

/**
 * @brief Show the main compression dialog.
 * @param ParentWindowHandle The owner window, or nullptr.
 * @param Context The caller-owned compression snapshot and result context.
 * @return The XAML message-loop result, or -1 when Modern is unavailable.
 */
EXTERN_C INT WINAPI K7ModernShowCompressDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_COMPRESS_DIALOG_CONTEXT Context);

/**
 * @brief The extract dialog context structure. The caller fills it with the
 *        current values before calling K7ModernShowExtractDialog, and the
 *        dialog writes the user-modified values back into it.
 */
typedef struct _K7_EXTRACT_DIALOG_CONTEXT
{
    // --- Input ---
    // The archive path, appended to the dialog title after " : ".
    WCHAR ArcPath[MAX_PATH];
    // The initial extraction directory (may end with a sub path segment that
    // is moved into the "file name" field when SplitDest is enabled).
    WCHAR DirPath[MAX_PATH];
    // The initial password (may be empty).
    WCHAR Password[256];
    // 0 = Full paths, 1 = No paths, 2 = Absolute paths.
    UINT32 PathMode;
    // 0 = Ask, 1 = Overwrite, 2 = Skip existing, 3 = Rename, 4 = Rename existing.
    UINT32 OverwriteMode;
    // If false, the dialog applies the remembered (registry) path/overwrite
    // mode instead of the value above (mirrors PathMode_Force semantics).
    BOOLEAN PathMode_Force;
    BOOLEAN OverwriteMode_Force;
    // Remembered (registry) defaults; the dialog uses them when the
    // corresponding _Force flag is false. 0xFFFFFFFF means "no default".
    UINT32 PathModeDefault;
    UINT32 OverwriteModeDefault;
    // CBoolPair semantics: Def = explicitly defined, Val = value.
    // The first pair is the caller-provided value, the second pair is the
    // remembered (registry) value; the effective value follows 7-Zip's
    // GetBoolsVal rule: pair1.Def ? pair1.Val : (pair2.Def ? pair2.Val : pair1.Val).
    BOOLEAN NtSecurityDef;
    BOOLEAN NtSecurityVal;
    BOOLEAN NtSecurityDef2;
    BOOLEAN NtSecurityVal2;
    BOOLEAN ElimDupDef;
    BOOLEAN ElimDupVal;
    BOOLEAN ElimDupDef2;
    BOOLEAN ElimDupVal2;
    BOOLEAN OpenFolderDef;
    BOOLEAN OpenFolderVal;
    BOOLEAN OpenFolderDef2;
    BOOLEAN OpenFolderVal2;
    BOOLEAN ShowPasswordDef;
    BOOLEAN ShowPasswordVal;
    BOOLEAN ShowPasswordDef2;
    BOOLEAN ShowPasswordVal2;
    BOOLEAN SplitDestDef;
    BOOLEAN SplitDestVal;
    BOOLEAN SplitDestDef2;
    BOOLEAN SplitDestVal2;
    // Per-invocation "delete archive after extraction" override.
    BOOLEAN DeleteAfterExtract;
    // Path history, most recent first (up to 16 entries).
    WCHAR Paths[16][MAX_PATH];
    UINT32 NumPaths;
    // History entries the user removed with the "x" in the drop-down
    // (written back by the caller regardless of OK/cancel).
    WCHAR RemovedPaths[16][MAX_PATH];
    UINT32 NumRemovedPaths;
    // Dialog font size in points (0 = follow system), read from the registry
    // by the caller so the XAML page does not touch the registry.
    UINT32 FontSizeDialog;
    // Minimum window track size in physical pixels, computed by the XAML
    // page from its measured content (so dragging can never hide options)
    // and read by the window subclass in WM_GETMINMAXINFO. 0 = not yet set.
    LONG MinTrackW;
    LONG MinTrackH;

    // --- Output ---
    // True when the user pressed OK; false otherwise (cancel / X close).
    BOOLEAN OK;
    // The final extraction directory (path prefix + sub path when enabled).
    WCHAR OutDirPath[MAX_PATH];
    // The sub path moved into the "file name" field (SplitDest).
    WCHAR OutPathName[256];
    // True when the "file name" sub path was enabled.
    BOOLEAN SplitDestEnable;
} K7_EXTRACT_DIALOG_CONTEXT, *PK7_EXTRACT_DIALOG_CONTEXT;

/**
 * @brief Show the extract dialog.
 * @param ParentWindowHandle A handle to the owner window of the dialog to be
 *                           created. If this parameter is nullptr, the dialog
 *                           has no owner window.
 * @param Context The extract dialog context. The caller fills it with the
 *                current values before calling this function. The dialog
 *                writes the user-modified values back into it. The caller
 *                must keep it valid until this function returns.
 * @return The message loop exit code of the dialog.
 */
EXTERN_C INT WINAPI K7ModernShowExtractDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_EXTRACT_DIALOG_CONTEXT Context);

/**
 * @brief Show the settings dialog.
 * @param ParentWindowHandle A handle to the owner window of the dialog to be
 *                           created. If this parameter is nullptr, the dialog
 *                           has no owner window.
 * @param Context The settings dialog context. The caller fills it with the
 *                current values before calling this function. The dialog
 *                writes the user-modified values back into it. The caller
 *                must keep it valid until this function returns.
 * @return The message loop exit code of the dialog.
 */
EXTERN_C INT WINAPI K7ModernShowSettingsDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_SETTINGS_DIALOG_CONTEXT Context);

#endif // !NANAZIP_MODERN_EXPERIENCE
