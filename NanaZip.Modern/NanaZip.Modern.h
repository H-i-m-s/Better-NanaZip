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
 * @brief Gets the configured FileManager top-level More-menu font size.
 * @return A point size, or zero when the XAML menu uses its default font size.
 */
EXTERN_C UINT32 WINAPI K7ModernGetMoreMenuFontSize();

/**
 * @brief Gets the configured FileManager context-menu font size.
 * @return A point size, or zero when the XAML menu uses its default font size.
 */
EXTERN_C UINT32 WINAPI K7ModernGetContextMenuFontSize();

/**
 * @brief Message posted by the XAML file context menu for a selected command.
 */
#define K7ModernContextMenuCommandMessage (WM_APP + 4)
#define K7ModernContextMenuClosedMessage (WM_APP + 5)
#define K7ModernContextMenuSystemMessage (WM_APP + 6)
#define K7ModernContextMenuSystemCommand 0xFFFFu

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
 * @brief Show a file context menu through the toolbar XAML island.
 * @param ToolBarWindowHandle The NanaZip toolbar XAML host window.
 * @param ParentWindowHandle The FileManager main window receiving commands.
 * @param MenuHandle A fully populated, temporary Win32 menu data source.
 * @param ScreenX The screen x-coordinate of the menu anchor.
 * @param ScreenY The screen y-coordinate of the menu anchor.
 * @param ContextPanelIndex The panel that owns the menu command context.
 * @return TRUE when the XAML menu was shown; otherwise FALSE.
 */
EXTERN_C BOOL WINAPI K7ModernShowContextMenu(
    _In_ HWND ToolBarWindowHandle,
    _In_ HWND ParentWindowHandle,
    _In_ HMENU MenuHandle,
    _In_opt_ HMENU SystemMenuHandle,
    _In_ HWND HostWindowHandle,
    _In_ INT ScreenX,
    _In_ INT ScreenY,
    _In_ UINT ContextPanelIndex,
    _In_ UINT ContextGeneration);

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
    UINT32 FontSizeMoreMenu;
    UINT32 FontSizeContextMenu;
    UINT32 DefaultUiFontPt;

    // --- Integration (menu) page ---
    BOOLEAN ElimDup;
    BOOLEAN ExtractOnOpen;
    // 0..3, 0xFFFFFFFF means "Default" (write -1 to the registry).
    UINT32 WriteZone;
    BOOLEAN ContextFlags[13];
    WCHAR ContextNames[13][192];
    BOOLEAN FileContextMenuFlags[28];
    WCHAR FileContextMenuNames[28][192];
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
    // Default checked state of "share password" in the extract and password
    // dialogs (the parent of the dialog-level check boxes).
    BOOLEAN AutoSharePassword;
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
    // Minimum window track size in physical pixels, computed by the XAML
    // page from its measured content (so dragging can never hide options)
    // and read by the window subclass in WM_GETMINMAXINFO. 0 = not yet set.
    LONG MinTrackW;
    LONG MinTrackH;
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

// Win32 message posted to the compress dialog host window to open the
// options child dialog. The options dialog creates a second XAML island;
// creating it synchronously from the XAML event handler would re-enter the
// single-threaded XAML core and deadlock, so the request is deferred to the
// Win32 message level (see CompressPage::OnOptionsClicked).
#define K7_COMPRESS_OPTIONS_OPEN_MESSAGE (WM_APP + 0x4D)

// Win32 message posted by the options page's first OnLoaded after it has
// populated the controls, so the host can re-measure the real content
// height before the window is shown (the first Measure can run before the
// visual tree is complete). The host consumes this synchronously before
// ShowWindow; no window bounce.
#define K7_COMPRESS_OPTIONS_REFIT_MESSAGE (WM_APP + 0x4E)

// Win32 message posted by the options page's second OnLoaded (the window
// is visible, layout complete) so the host can grow the window by the real
// viewport/content delta and eliminate the last pixel of scrollbar.
#define K7_COMPRESS_OPTIONS_REFIT2_MESSAGE (WM_APP + 0x4F)

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

    // --- Archive path history (most recent first, up to 16 entries) ---
    WCHAR Paths[16][MAX_PATH];
    UINT32 NumPaths;
    // History entries the user removed with the "x" in the drop-down
    // (written back by the caller regardless of OK/cancel).
    WCHAR RemovedPaths[16][MAX_PATH];
    UINT32 NumRemovedPaths;

    // --- Dialog behavior ---
    UINT32 FontSizeDialog;
    LONG MinTrackW;
    LONG MinTrackH;
    // Initial/final window rectangle in physical pixels. The Universal side
    // persists this value so the dialog reopens at its last position and size.
    RECT WindowRect;
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
    // Initial checked state of "share password" (input; the caller reads
    // the "auto share password" setting, the dialog changes never write
    // back).
    BOOLEAN SharePassword;
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

/**
 * @brief Show the compression options dialog (the "Options" child dialog of
 *        the compression dialog).
 * @param ParentWindowHandle A handle to the owner window of the dialog to be
 *                           created. If this parameter is nullptr, the dialog
 *                           has no owner window.
 * @param Context The compression options context. The caller fills the input
 *                fields from CCompressDialogCore and CFormatOptions (the
 *                precision combo items are generated by the caller because
 *                the names come from 7-Zip language resources) before calling
 *                this function. The dialog writes the user-modified values
 *                back into it. The caller must keep it valid until this
 *                function returns.
 * @return The message loop exit code of the dialog.
 */
#define K7_COMPRESS_OPTIONS_MAX_PREC 16
#define K7_COMPRESS_OPTIONS_MAX_TEXT 96

typedef struct _K7_COMPRESS_OPTIONS_PREC_ITEM
{
    WCHAR Text[K7_COMPRESS_OPTIONS_MAX_TEXT];
    UINT32 Value;      // precision value written back when selected
    BOOLEAN IsDefault; // the auto/default entry
} K7_COMPRESS_OPTIONS_PREC_ITEM, *PK7_COMPRESS_OPTIONS_PREC_ITEM;

typedef struct _K7_COMPRESS_OPTIONS_DIALOG_CONTEXT
{
    // --- NTFS group ---
    BOOLEAN NtSymLinksSupported;
    BOOLEAN NtSymLinks;
    BOOLEAN NtHardLinksSupported;
    BOOLEAN NtHardLinks;
    BOOLEAN NtAltStreamsSupported;
    BOOLEAN NtAltStreams;
    BOOLEAN NtSecuritySupported;
    BOOLEAN NtSecurity;
    BOOLEAN PreserveATime;

    // --- Time group: precision combo (caller-generated items) ---
    BOOLEAN TimePrecSet;   // the "set" check box
    UINT32 TimePrec;       // current precision (0xFFFFFFFF = auto)
    UINT32 DefaultTimePrec; // the caller's auto value
    UINT32 PrecCount;
    K7_COMPRESS_OPTIONS_PREC_ITEM PrecItems[K7_COMPRESS_OPTIONS_MAX_PREC];

    // --- Time group: modification/creation/access/archive time ---
    BOOLEAN MTimeSupported;
    BOOLEAN MTimeIsSet;
    BOOLEAN MTimeVal;
    BOOLEAN MTimeDefault;
    BOOLEAN CTimeSupported;
    BOOLEAN CTimeIsSet;
    BOOLEAN CTimeVal;
    BOOLEAN CTimeDefault;
    BOOLEAN ATimeSupported;
    BOOLEAN ATimeIsSet;
    BOOLEAN ATimeVal;
    BOOLEAN ATimeDefault;
    BOOLEAN ZTimeSupported;
    BOOLEAN ZTimeIsSet;
    BOOLEAN ZTimeVal;
    BOOLEAN ZTimeDefault;

    // --- Info line (e.g. "type: 7z") ---
    WCHAR InfoText[256];

    // --- Localized texts (filled by the caller from the 7-Zip language
    // resources; the XAML page cannot reach those) ---
    WCHAR GroupNtfsText[32];
    WCHAR GroupTimeText[32];
    WCHAR NtSymLinksText[96];
    WCHAR NtHardLinksText[96];
    WCHAR NtAltStreamsText[96];
    WCHAR NtSecurityText[96];
    WCHAR PreserveATimeText[96];
    WCHAR MTimeText[96];
    WCHAR CTimeText[96];
    WCHAR ATimeText[96];
    WCHAR ZTimeText[96];
    WCHAR PrecLabelText[96];
    WCHAR OkText[32];
    WCHAR CancelText[32];

    // --- Format flags for the interactive zip/tar special cases ---
    BOOLEAN IsZip;        // zip: a non-Windows precision hides C/A time
    BOOLEAN IsTar;        // tar: C/A time hidden (A only for the posix method)
    BOOLEAN TarPosix;     // tar + posix method: A time allowed
    BOOLEAN IsSingleFile; // keep-name formats hide the MTime "set" box

    // --- Dialog font size in points (0 = default) ---
    UINT32 FontSizePt;

    // --- Result ---
    BOOLEAN OK;
} K7_COMPRESS_OPTIONS_DIALOG_CONTEXT, *PK7_COMPRESS_OPTIONS_DIALOG_CONTEXT;

EXTERN_C INT WINAPI K7ModernShowCompressOptionsDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_COMPRESS_OPTIONS_DIALOG_CONTEXT Context);

/**
 * @brief The split dialog command callback. Invoked on the XAML UI thread
 *        while the split dialog is open. Returning FALSE rejects the
 *        command and the page keeps the dialog open.
 */
typedef BOOLEAN (WINAPI *K7_SPLIT_DIALOG_COMMAND_CALLBACK)(
    _In_ LPVOID CallbackContext,
    _In_ UINT32 Command,
    _In_ INT64 Value,
    _In_opt_ LPCWSTR SemanticText);

// Validate the volume-size text. SemanticText carries the raw text and the
// callback writes the parsed sizes back into the context; it returns FALSE
// when the text cannot be parsed or yields no sizes.
#define K7_SPLIT_DIALOG_COMMAND_VALIDATE_VOLUME 0u

#define K7_SPLIT_MAX_VOLUME_SIZES 16

/**
 * @brief The split dialog context structure. The caller fills it with the
 *        current values before calling K7ModernShowSplitDialog, and the
 *        dialog writes the user-modified values back into it.
 */
typedef struct _K7_SPLIT_DIALOG_CONTEXT
{
    // --- Input ---
    // The source file name, appended to the dialog title.
    WCHAR FileName[MAX_PATH];
    // The initial output directory.
    WCHAR DirPath[MAX_PATH];
    // Dialog font size in points (0 = follow system), read from the registry
    // by the caller so the XAML page does not touch the registry.
    UINT32 FontSizeDialog;
    // Minimum window track size in physical pixels, computed by the XAML
    // page from its measured content and read by the window subclass in
    // WM_GETMINMAXINFO. 0 = not yet set.
    LONG MinTrackW;
    LONG MinTrackH;

    // --- Command ---
    K7_SPLIT_DIALOG_COMMAND_CALLBACK CommandCallback;
    LPVOID CallbackContext;

    // --- Output ---
    // True when the user pressed OK; false otherwise (cancel / X close).
    BOOLEAN OK;
    // The final output directory.
    WCHAR OutDirPath[MAX_PATH];
    // The parsed volume sizes written by the command callback.
    UINT64 VolumeSizes[K7_SPLIT_MAX_VOLUME_SIZES];
    UINT32 VolumeSizesCount;
} K7_SPLIT_DIALOG_CONTEXT, *PK7_SPLIT_DIALOG_CONTEXT;

/**
 * @brief Show the split dialog.
 * @param ParentWindowHandle The owner window, or nullptr.
 * @param Context The caller-owned snapshot and result context. The caller
 *                must keep it valid until this function returns.
 * @return The XAML message-loop result, or -1 when Modern is unavailable.
 */
EXTERN_C INT WINAPI K7ModernShowSplitDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_SPLIT_DIALOG_CONTEXT Context);

/**
 * @brief The password dialog context structure. The caller fills it with
 *        the initial password and the show-password state before calling
 *        K7ModernShowPasswordDialog, and the dialog writes the user's
 *        input back into it.
 */
#define K7_PASSWORD_MAX_PASSWORD_LENGTH 512

typedef struct _K7_PASSWORD_DIALOG_CONTEXT
{
    // --- Input ---
    // Dialog font size in points (0 = follow system), read from the registry
    // by the caller so the XAML page does not touch the registry.
    UINT32 FontSizeDialog;
    // The initial password (may be empty). The caller truncates to
    // K7_PASSWORD_MAX_PASSWORD_LENGTH - 1 before writing.
    WCHAR Password[K7_PASSWORD_MAX_PASSWORD_LENGTH];
    // The initial show-password state.
    BOOLEAN ShowPassword;
    // The initial share-password state (input). The host fills it from the
    // "auto share password" setting; the dialog-level changes never write
    // back to the setting.
    BOOLEAN SharePassword;
    // Minimum window track size in physical pixels, computed by the XAML
    // page from its measured content and read by the window subclass in
    // WM_GETMINMAXINFO. 0 = not yet set.
    LONG MinTrackW;
    LONG MinTrackH;

    // --- Output ---
    // True when the user pressed OK; false otherwise (cancel / X close).
    BOOLEAN OK;
} K7_PASSWORD_DIALOG_CONTEXT, *PK7_PASSWORD_DIALOG_CONTEXT;

/**
 * @brief Show the password dialog.
 * @param ParentWindowHandle The owner window, or nullptr.
 * @param Context The caller-owned snapshot and result context. The caller
 *                must keep it valid until this function returns.
 * @return The XAML message-loop result, or -1 when Modern is unavailable.
 */
EXTERN_C INT WINAPI K7ModernShowPasswordDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_PASSWORD_DIALOG_CONTEXT Context);

/**
 * @brief The combo (single-line input) dialog context structure, matching
 *        the Win32 CComboDialog. The caller fills it with the title, the
 *        static prompt, the initial value and the optional history items
 *        before calling K7ModernShowComboDialog; the dialog writes the
 *        user's input back into Value.
 */
#define K7_COMBO_MAX_TEXT_LENGTH 512
#define K7_COMBO_MAX_HISTORY_ITEMS 8

typedef struct _K7_COMBO_DIALOG_CONTEXT
{
    // --- Input ---
    // Dialog font size in points (0 = follow system), read from the registry
    // by the caller so the XAML page does not touch the registry.
    UINT32 FontSizeDialog;
    // Window title (the caller owns the language string, e.g. LangString).
    WCHAR Title[K7_COMBO_MAX_TEXT_LENGTH];
    // Static prompt text above the combo.
    WCHAR Static[K7_COMBO_MAX_TEXT_LENGTH];
    // Initial value; the dialog writes the user's input back here.
    WCHAR Value[K7_COMBO_MAX_TEXT_LENGTH];
    // Optional drop-down history items (may be 0).
    UINT32 HistoryCount;
    WCHAR History[K7_COMBO_MAX_HISTORY_ITEMS][K7_COMBO_MAX_TEXT_LENGTH];
    // Minimum window track size in physical pixels, computed by the XAML
    // page from its measured content and read by the window subclass in
    // WM_GETMINMAXINFO. 0 = not yet set.
    LONG MinTrackW;
    LONG MinTrackH;

    // --- Output ---
    // True when the user pressed OK; false otherwise (cancel / X close).
    BOOLEAN OK;
} K7_COMBO_DIALOG_CONTEXT, *PK7_COMBO_DIALOG_CONTEXT;

/**
 * @brief Show the combo (single-line input) dialog.
 * @param ParentWindowHandle The owner window, or nullptr.
 * @param Context The caller-owned snapshot and result context. The caller
 *                must keep it valid until this function returns.
 * @return The XAML message-loop result, or -1 when Modern is unavailable.
 */
EXTERN_C INT WINAPI K7ModernShowComboDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_COMBO_DIALOG_CONTEXT Context);

/**
 * @brief The copy / move destination dialog context structure, matching
 *        the Win32 CCopyDialog. The caller fills it with the title, the
 *        static prompt, the initial value, the additional information text
 *        and the optional history items before calling
 *        K7ModernShowCopyLocationDialog; the dialog writes the user's
 *        destination back into Value.
 */
#define K7_COPY_MAX_TEXT_LENGTH 512
#define K7_COPY_MAX_HISTORY_ITEMS 16

typedef struct _K7_COPY_DIALOG_CONTEXT
{
    // --- Input ---
    // Dialog font size in points (0 = follow system), read from the registry
    // by the caller so the XAML page does not touch the registry.
    UINT32 FontSizeDialog;
    // Window title (the caller owns the language string, e.g. LangString).
    WCHAR Title[K7_COPY_MAX_TEXT_LENGTH];
    // Static prompt text above the combo ("Copy to:").
    WCHAR Static[K7_COPY_MAX_TEXT_LENGTH];
    // Initial value; the dialog writes the user's destination back here.
    WCHAR Value[K7_COPY_MAX_TEXT_LENGTH];
    // Additional information text under the input row (e.g. file count).
    WCHAR Info[K7_COPY_MAX_TEXT_LENGTH];
    // Optional drop-down history items (may be 0).
    UINT32 HistoryCount;
    WCHAR History[K7_COPY_MAX_HISTORY_ITEMS][K7_COPY_MAX_TEXT_LENGTH];
    // Minimum window track size in physical pixels, computed by the XAML
    // page from its measured content and read by the window subclass in
    // WM_GETMINMAXINFO. 0 = not yet set.
    LONG MinTrackW;
    LONG MinTrackH;

    // --- Output ---
    // True when the user pressed OK; false otherwise (cancel / X close).
    BOOLEAN OK;
} K7_COPY_DIALOG_CONTEXT, *PK7_COPY_DIALOG_CONTEXT;

/**
 * @brief Show the copy / move destination location dialog.
 * @param ParentWindowHandle The owner window, or nullptr.
 * @param Context The caller-owned snapshot and result context. The caller
 *                must keep it valid until this function returns.
 * @return The XAML message-loop result, or -1 when Modern is unavailable.
 */
EXTERN_C INT WINAPI K7ModernShowCopyLocationDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_COPY_DIALOG_CONTEXT Context);

/**
 * @brief The link (hard link / symbolic link / junction / WSL) creation
 *        callback. The XAML page calls it when the user presses the Link
 *        button; the 7-Zip side owns the creation logic and writes a
 *        localized error message on failure (the dialog stays open with
 *        the error shown inline).
 * @param From The source path (may be relative; the callback resolves it
 *             against the context's CurDirPrefix).
 * @param To The link target path.
 * @param LinkType The selected link type (0 = Hard, 1 = SymFile,
 *                 2 = SymDir, 3 = Junction, 4 = WSL).
 * @param CallbackContext The callback context passed by the caller.
 * @param ErrorBuffer Receives the error message on failure (empty on
 *                    success).
 * @param ErrorBufferSize Size of ErrorBuffer in characters.
 * @return TRUE on success, FALSE on failure.
 */
#define K7_LINK_MAX_TEXT_LENGTH 512
#define K7_LINK_MAX_ERROR_LENGTH 1024
#define K7_LINK_MAX_TYPE_COUNT 5
#define K7_LINK_TYPE_NAME_LENGTH 64

typedef BOOL (WINAPI *K7_LINK_DIALOG_COMMAND_CALLBACK)(
    _In_ LPCWSTR From,
    _In_ LPCWSTR To,
    _In_ UINT32 LinkType,
    _In_opt_ LPVOID CallbackContext,
    _Out_writes_z_(K7_LINK_MAX_ERROR_LENGTH) LPWSTR ErrorBuffer,
    _In_ UINT32 ErrorBufferSize);

/**
 * @brief The link dialog context structure, matching the Win32
 *        CLinkDialog. The caller fills it with the paths, the localized
 *        texts and the initial link type before calling
 *        K7ModernShowLinkDialog; the dialog writes the user's final
 *        selection back into From / To / LinkType.
 */
typedef struct _K7_LINK_DIALOG_CONTEXT
{
    // --- Input ---
    // Dialog font size in points (0 = follow system), read from the registry
    // by the caller so the XAML page does not touch the registry.
    UINT32 FontSizeDialog;
    // Window title (the caller owns the language string).
    WCHAR Title[K7_LINK_TYPE_NAME_LENGTH];
    // Source path (initial value; the dialog writes the user's input back).
    WCHAR From[K7_LINK_MAX_TEXT_LENGTH];
    // Link target path (initial value; output too).
    WCHAR To[K7_LINK_MAX_TEXT_LENGTH];
    // Prefix used by the callback to resolve a relative source path.
    WCHAR CurDirPrefix[K7_LINK_MAX_TEXT_LENGTH];
    // Localized label texts.
    WCHAR FromLabel[K7_LINK_TYPE_NAME_LENGTH];
    WCHAR ToLabel[K7_LINK_TYPE_NAME_LENGTH];
    WCHAR TypeGroupLabel[K7_LINK_TYPE_NAME_LENGTH];
    WCHAR LinkButtonText[K7_LINK_TYPE_NAME_LENGTH];
    // Current link information (shown when editing an existing reparse
    // point; may be empty).
    WCHAR Hint[K7_LINK_MAX_TEXT_LENGTH];
    // Localized names of the link types (indexed by LinkType).
    WCHAR TypeNames[K7_LINK_MAX_TYPE_COUNT][K7_LINK_TYPE_NAME_LENGTH];
    // The initially selected link type (0-4).
    UINT32 InitialLinkType;
    // Minimum window track size in physical pixels, computed by the XAML
    // page from its measured content and read by the window subclass in
    // WM_GETMINMAXINFO. 0 = not yet set.
    LONG MinTrackW;
    LONG MinTrackH;

    // --- Command ---
    // The link creation callback (7-Zip side). May be null.
    K7_LINK_DIALOG_COMMAND_CALLBACK CommandCallback;
    // Callback context (e.g. a pointer to this context).
    LPVOID CallbackContext;

    // --- Output ---
    // The selected link type (0-4).
    UINT32 LinkType;
    // True when the user pressed Link and the callback succeeded; false
    // otherwise (cancel / X close).
    BOOLEAN OK;
} K7_LINK_DIALOG_CONTEXT, *PK7_LINK_DIALOG_CONTEXT;

/**
 * @brief Show the link dialog.
 * @param ParentWindowHandle The owner window, or nullptr.
 * @param Context The caller-owned snapshot and result context. The caller
 *                must keep it valid until this function returns.
 * @return The XAML message-loop result, or -1 when Modern is unavailable.
 */
EXTERN_C INT WINAPI K7ModernShowLinkDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_LINK_DIALOG_CONTEXT Context);

/**
 * @brief The benchmark window command IDs sent by the XAML page to the
 *        7-Zip window subclass (WM_COMMAND, BN_CLICKED).
 */
#define K7_BENCH_WINDOW_COMMAND_RESTART 1u
#define K7_BENCH_WINDOW_COMMAND_STOP 2u
#define K7_BENCH_WINDOW_COMMAND_CANCEL 3u

/**
 * @brief The benchmark option-change messages sent by the XAML page to
 *        the 7-Zip window subclass (WM_APP + N). wParam carries the new
 *        combo selection (index for the dictionary, value for threads /
 *        passes).
 */
#define K7_BENCH_WINDOW_MSG_SET_DICTIONARY (WM_APP + 10u)
#define K7_BENCH_WINDOW_MSG_SET_THREADS (WM_APP + 11u)
#define K7_BENCH_WINDOW_MSG_SET_PASSES (WM_APP + 12u)

#define K7_BENCH_MAX_TEXT_LENGTH 512
#define K7_BENCH_MAX_SHORT_TEXT_LENGTH 64
#define K7_BENCH_MAX_COMBO_ITEMS 32
#define K7_BENCH_MAX_LOG_LENGTH 16384

/**
 * @brief The benchmark dialog context structure (input). The caller fills
 *        the system information, the combo options and the initial values
 *        before calling K7ModernShowBenchmarkDialog.
 */
typedef struct _K7_BENCHMARK_DIALOG_CONTEXT
{
    // Dialog font size in points (0 = follow system), read from the registry
    // by the caller so the XAML page does not touch the registry.
    UINT32 FontSizeDialog;
    // True for the command-line total mode (log-only window).
    BOOLEAN TotalMode;
    // Initial values; the XAML page writes the final selections back.
    UINT64 DictSize;
    UINT32 NumThreads;
    UINT32 NumPassesLimit;
    // System information lines.
    WCHAR Version[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR Cpu[K7_BENCH_MAX_TEXT_LENGTH];
    WCHAR Sys1[K7_BENCH_MAX_TEXT_LENGTH];
    WCHAR Sys2[K7_BENCH_MAX_TEXT_LENGTH];
    WCHAR Features[K7_BENCH_MAX_TEXT_LENGTH];
    WCHAR HardwareThreads[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    // Dictionary combo options with their memory-usage texts.
    UINT32 DictItemsCount;
    WCHAR DictItems[K7_BENCH_MAX_COMBO_ITEMS][K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR DictMemoryItems[K7_BENCH_MAX_COMBO_ITEMS][K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    // Threads combo options.
    UINT32 ThreadItemsCount;
    WCHAR ThreadItems[K7_BENCH_MAX_COMBO_ITEMS][K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    // Passes combo options.
    UINT32 PassesItemsCount;
    WCHAR PassesItems[K7_BENCH_MAX_COMBO_ITEMS][K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    // Initial combo selections (indexes).
    UINT32 DictIndex;
    UINT32 ThreadIndex;
    UINT32 PassesIndex;
    // Minimum window track size in physical pixels, computed by the XAML
    // page from its measured content and read by the window subclass in
    // WM_GETMINMAXINFO. 0 = not yet set.
    LONG MinTrackW;
    LONG MinTrackH;
    // Window position persistence (input): when HasInitialPos is true the
    // host places the dialog at InitialX/InitialY (clamped to the work
    // area) instead of centering it. The caller persists the position in
    // its own registry (the 7-Zip side saves it on WM_CLOSE).
    BOOLEAN HasInitialPos;
    INT32 InitialX;
    INT32 InitialY;
} K7_BENCHMARK_DIALOG_CONTEXT, *PK7_BENCHMARK_DIALOG_CONTEXT;

/**
 * @brief The benchmark status structure, refreshed once per second by the
 *        7-Zip side (the caller formats the texts).
 */
typedef struct _K7_BENCHMARK_STATUS
{
    // Runtime state.
    BOOLEAN Running;   // benchmark is currently running
    BOOLEAN Finished;  // all passes finished
    BOOLEAN Stopped;   // user stopped the benchmark
    BOOLEAN HasError;  // a fatal error occurred
    WCHAR Error[K7_BENCH_MAX_TEXT_LENGTH];
    // Progress.
    UINT32 PassesFinished;
    WCHAR Elapsed[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    // Compression results (1 = current, 2 = resulting).
    WCHAR EncSpeed1[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR EncSpeed2[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR EncRating1[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR EncRating2[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR EncUsage1[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR EncUsage2[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR EncRpu1[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR EncRpu2[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR EncSize1[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR EncSize2[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    // Decompression results.
    WCHAR DecSpeed1[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR DecSpeed2[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR DecRating1[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR DecRating2[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR DecUsage1[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR DecUsage2[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR DecRpu1[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR DecRpu2[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR DecSize1[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR DecSize2[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    // Totals.
    WCHAR TotalRating[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR TotalRpu[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    WCHAR TotalUsage[K7_BENCH_MAX_SHORT_TEXT_LENGTH];
    // Rating-vector log (full text).
    WCHAR Log[K7_BENCH_MAX_LOG_LENGTH];
} K7_BENCHMARK_STATUS, *PK7_BENCHMARK_STATUS;

/**
 * @brief Show the benchmark dialog.
 * @param ParentWindowHandle The owner window, or nullptr.
 * @param Context The caller-owned input context. The caller must keep it
 *                valid until this function returns.
 * @param WindowSubclassHandler The window subclass procedure for the
 *                              benchmark window.
 * @param WindowSubclassContext The context pointer for the window subclass
 *                              procedure.
 * @return The XAML message-loop result, or -1 when Modern is unavailable.
 */
EXTERN_C INT WINAPI K7ModernShowBenchmarkDialog(
    _In_opt_ HWND ParentWindowHandle,
    _Inout_ PK7_BENCHMARK_DIALOG_CONTEXT Context,
    _In_ SUBCLASSPROC WindowSubclassHandler,
    _In_ LPVOID WindowSubclassContext);

/**
 * @brief Refresh the benchmark window status.
 * @param WindowHandle A handle to the benchmark window.
 * @param Status The formatted status to apply.
 */
EXTERN_C VOID WINAPI K7ModernUpdateBenchmarkStatus(
    _In_ HWND WindowHandle,
    _In_ PK7_BENCHMARK_STATUS Status);

#endif // !NANAZIP_MODERN_EXPERIENCE
