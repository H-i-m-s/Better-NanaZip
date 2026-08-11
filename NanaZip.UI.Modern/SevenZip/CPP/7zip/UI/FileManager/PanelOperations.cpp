// PanelOperations.cpp

#include "StdAfx.h"

#include "../../../Common/DynamicBuffer.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/Wildcard.h"

#include "../../../Windows/COM.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariant.h"
// **************** SSS Modification Start ****************
#include "../../../Windows/Registry.h"
// **************** SSS Modification End ****************

#include "ComboDialog.h"

#include "FSFolder.h"
#include "FormatUtils.h"
#include "LangUtils.h"
#include "Panel.h"
#include "UpdateCallback100.h"
// **************** SSS Modification Start ****************
#include <ShlObj.h> // SHFileOperationW for batch recycle-bin delete
#include "RegistryUtils.h"
#include "SssBatchFolder.h"
#include "../Common/ZipRegistry.h"
#include "../Common/CompressCall.h"
#include "../../../Windows/FileDir.h"
// **************** SSS Modification End ****************

#include "resource.h"

using namespace NWindows;
using namespace NFile;
using namespace NName;

#define MY_CAST_FUNC  (void(*)())
// #define MY_CAST_FUNC

#ifndef _UNICODE
extern bool g_IsNT;
#endif

enum EFolderOpType
{
  FOLDER_TYPE_CREATE_FOLDER = 0,
  FOLDER_TYPE_DELETE = 1,
  FOLDER_TYPE_RENAME = 2
};

class CThreadFolderOperations: public CProgressThreadVirt
{
  HRESULT ProcessVirt();
public:
  EFolderOpType OpType;
  UString Name;
  UInt32 Index;
  CRecordVector<UInt32> Indices;

  CMyComPtr<IFolderOperations> FolderOperations;
  CMyComPtr<IProgress> UpdateCallback;
  CUpdateCallback100Imp *UpdateCallbackSpec;

  CThreadFolderOperations(EFolderOpType opType): OpType(opType) {}
  HRESULT DoOperation(CPanel &panel, const UString &progressTitle, const UString &titleError);
};

HRESULT CThreadFolderOperations::ProcessVirt()
{
  NCOM::CComInitializer comInitializer;
  switch (OpType)
  {
    case FOLDER_TYPE_CREATE_FOLDER:
      return FolderOperations->CreateFolder(Name, UpdateCallback);
    case FOLDER_TYPE_DELETE:
      return FolderOperations->Delete(&Indices.Front(), Indices.Size(), UpdateCallback);
    case FOLDER_TYPE_RENAME:
      return FolderOperations->Rename(Index, Name, UpdateCallback);
    default:
      return E_FAIL;
  }
}


HRESULT CThreadFolderOperations::DoOperation(CPanel &panel, const UString &progressTitle, const UString &titleError)
{
  UpdateCallbackSpec = new CUpdateCallback100Imp;
  UpdateCallback = UpdateCallbackSpec;
  UpdateCallbackSpec->ProgressDialog = this;

  WaitMode = true;
  Sync.FinalMessage.ErrorMessage.Title = titleError;

  UpdateCallbackSpec->Init();

  if (panel._parentFolders.Size() > 0)
  {
    const CFolderLink &fl = panel._parentFolders.Back();
    UpdateCallbackSpec->PasswordIsDefined = fl.UsePassword;
    UpdateCallbackSpec->Password = fl.Password;
  }

  MainWindow = panel._mainWindow; // panel.GetParent()
  MainTitle = "NanaZip"; // LangString(IDS_APP_TITLE);
  MainAddTitle = progressTitle + L' ';

  RINOK(Create(progressTitle, MainWindow));
  return Result;
}

#ifndef _UNICODE
typedef int (WINAPI * Func_SHFileOperationW)(LPSHFILEOPSTRUCTW lpFileOp);
#endif

/*
void CPanel::MessageBoxErrorForUpdate(HRESULT errorCode, UINT resourceID)
{
  if (errorCode == E_NOINTERFACE)
    MessageBox_Error_UnsupportOperation();
  else
    MessageBox_Error_HRESULT_Caption(errorCode, LangString(resourceID));
}
*/

void CPanel::DeleteItems(bool NON_CE_VAR(toRecycleBin))
{
  CDisableTimerProcessing disableTimerProcessing(*this);
  CRecordVector<UInt32> indices;
  GetOperatedItemIndices(indices);
  if (indices.IsEmpty())
    return;
  CSelectedState state;
  SaveSelectedState(state);

  #ifndef UNDER_CE
  // WM6 / SHFileOperationW doesn't ask user! So we use internal delete
  if (IsFSFolder() && toRecycleBin)
  {
    bool useInternalDelete = false;
    #ifndef _UNICODE
    if (!g_IsNT)
    {
      CDynamicBuffer<CHAR> buffer;
      FOR_VECTOR (i, indices)
      {
        const AString path (GetSystemString(GetItemFullPath(indices[i])));
        buffer.AddData(path, path.Len() + 1);
      }
      *buffer.GetCurPtrAndGrow(1) = 0;
      SHFILEOPSTRUCTA fo;
      fo.hwnd = GetParent();
      fo.wFunc = FO_DELETE;
      fo.pFrom = (const CHAR *)buffer;
      fo.pTo = 0;
      fo.fFlags = 0;
      if (toRecycleBin)
        fo.fFlags |= FOF_ALLOWUNDO;
      // fo.fFlags |= FOF_NOCONFIRMATION;
      // fo.fFlags |= FOF_NOERRORUI;
      // fo.fFlags |= FOF_SILENT;
      // fo.fFlags |= FOF_WANTNUKEWARNING;
      fo.fAnyOperationsAborted = FALSE;
      fo.hNameMappings = 0;
      fo.lpszProgressTitle = 0;
      /* int res = */ ::SHFileOperationA(&fo);
    }
    else
    #endif
    {
      CDynamicBuffer<WCHAR> buffer;
      unsigned maxLen = 0;
      const UString prefix = GetFsPath();
      FOR_VECTOR (i, indices)
      {
        // L"\\\\?\\") doesn't work here.
        const UString path = prefix + GetItemRelPath2(indices[i]);
        if (path.Len() > maxLen)
          maxLen = path.Len();
        buffer.AddData(path, path.Len() + 1);
      }
      *buffer.GetCurPtrAndGrow(1) = 0;
      if (maxLen >= MAX_PATH)
      {
        if (toRecycleBin)
        {
          MessageBox_Error_LangID(IDS_ERROR_LONG_PATH_TO_RECYCLE);
          return;
        }
        useInternalDelete = true;
      }
      else
      {
        SHFILEOPSTRUCTW fo;
        fo.hwnd = GetParent();
        fo.wFunc = FO_DELETE;
        fo.pFrom = (const WCHAR *)buffer;
        fo.pTo = 0;
        fo.fFlags = 0;
        if (toRecycleBin)
          fo.fFlags |= FOF_ALLOWUNDO;
        fo.fAnyOperationsAborted = FALSE;
        fo.hNameMappings = 0;
        fo.lpszProgressTitle = 0;
        // int res;
        #ifdef _UNICODE
        /* res = */ ::SHFileOperationW(&fo);
        #else
        Func_SHFileOperationW shFileOperationW = (Func_SHFileOperationW)
          MY_CAST_FUNC
          ::GetProcAddress(::GetModuleHandleW(L"shell32.dll"), "SHFileOperationW");
        if (!shFileOperationW)
          return;
        /* res = */ shFileOperationW(&fo);
        #endif
      }
    }
    /*
    if (fo.fAnyOperationsAborted)
      MessageBox_Error_HRESULT_Caption(result, LangString(IDS_ERROR_DELETING));
    */
    if (!useInternalDelete)
    {
      RefreshListCtrl(state);
      return;
    }
  }
  #endif

  // DeleteItemsInternal

  if (!CheckBeforeUpdate(IDS_ERROR_DELETING))
    return;

  UInt32 titleID, messageID;
  UString messageParam;
  if (indices.Size() == 1)
  {
    int index = indices[0];
    messageParam = GetItemRelPath2(index);
    if (IsItem_Folder(index))
    {
      titleID = IDS_CONFIRM_FOLDER_DELETE;
      messageID = IDS_WANT_TO_DELETE_FOLDER;
    }
    else
    {
      titleID = IDS_CONFIRM_FILE_DELETE;
      messageID = IDS_WANT_TO_DELETE_FILE;
    }
  }
  else
  {
    titleID = IDS_CONFIRM_ITEMS_DELETE;
    messageID = IDS_WANT_TO_DELETE_ITEMS;
    messageParam = NumberToString(indices.Size());
  }
  if (::MessageBoxW(GetParent(), MyFormatNew(messageID, messageParam), LangString(titleID), MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
    return;

  CDisableNotify disableNotify(*this);
  {
    CThreadFolderOperations op(FOLDER_TYPE_DELETE);
    op.FolderOperations = _folderOperations;
    op.Indices = indices;
    op.DoOperation(*this,
        LangString(IDS_DELETING),
        LangString(IDS_ERROR_DELETING));
  }
  RefreshTitleAlways();
  RefreshListCtrl(state);
}

BOOL CPanel::OnBeginLabelEdit(LV_DISPINFOW * lpnmh)
{
  int realIndex = GetRealIndex(lpnmh->item);
  if (realIndex == kParentIndex)
    return TRUE;
  if (IsThereReadOnlyFolder())
    return TRUE;
  return FALSE;
}

static bool IsCorrectFsName(const UString &name)
{
  const UString lastPart = name.Ptr((unsigned)(name.ReverseFind_PathSepar() + 1));
  return
      lastPart != L"." &&
      lastPart != L"..";
}

bool CorrectFsPath(const UString &relBase, const UString &path, UString &result);

bool CPanel::CorrectFsPath(const UString &path2, UString &result)
{
  return ::CorrectFsPath(GetFsPath(), path2, result);
}

BOOL CPanel::OnEndLabelEdit(LV_DISPINFOW * lpnmh)
{
  if (lpnmh->item.pszText == NULL)
    return FALSE;
  CDisableTimerProcessing disableTimerProcessing2(*this);

  if (!CheckBeforeUpdate(IDS_ERROR_RENAMING))
    return FALSE;

  UString newName = lpnmh->item.pszText;
  if (!IsCorrectFsName(newName))
  {
    MessageBox_Error_HRESULT(E_INVALIDARG);
    return FALSE;
  }

  if (IsFSFolder())
  {
    UString correctName;
    if (!CorrectFsPath(newName, correctName))
    {
      MessageBox_Error_HRESULT(E_INVALIDARG);
      return FALSE;
    }
    newName = correctName;
  }

  SaveSelectedState(_selectedState);

  int realIndex = GetRealIndex(lpnmh->item);
  if (realIndex == kParentIndex)
    return FALSE;
  const UString prefix = GetItemPrefix(realIndex);


  CDisableNotify disableNotify(*this);
  {
    CThreadFolderOperations op(FOLDER_TYPE_RENAME);
    op.FolderOperations = _folderOperations;
    op.Index = realIndex;
    op.Name = newName;
    /* HRESULTres = */ op.DoOperation(*this,
        LangString(IDS_RENAMING),
        LangString(IDS_ERROR_RENAMING));
    // fixed in 9.26: we refresh list even after errors
    // (it's more safe, since error can be at different stages, so list can be incorrect).
    /*
    if (res != S_OK)
      return FALSE;
    */
  }

  // Can't use RefreshListCtrl here.
  // RefreshListCtrlSaveFocused();
  _selectedState.FocusedName = prefix + newName;
  _selectedState.FocusedName_Defined = true;
  _selectedState.SelectFocused = true;

  // We need clear all items to disable GetText before Reload:
  // number of items can change.
  // DeleteListItems();
  // But seems it can still call GetText (maybe for current item)
  // so we can't delete items.

  _dontShowMode = true;

  PostMsg(kReLoadMessage);
  return TRUE;
}

bool Dlg_CreateFolder(HWND wnd, UString &destName);

void CPanel::CreateFolder()
{
  if (IsHashFolder())
    return;

  if (!CheckBeforeUpdate(IDS_CREATE_FOLDER_ERROR))
    return;

  CDisableTimerProcessing disableTimerProcessing2(*this);
  CSelectedState state;
  SaveSelectedState(state);

  UString newName;
  if (!Dlg_CreateFolder(GetParent(), newName))
    return;

  if (!IsCorrectFsName(newName))
  {
    MessageBox_Error_HRESULT(E_INVALIDARG);
    return;
  }

  if (IsFSFolder())
  {
    UString correctName;
    if (!CorrectFsPath(newName, correctName))
    {
      MessageBox_Error_HRESULT(E_INVALIDARG);
      return;
    }
    newName = correctName;
  }

  HRESULT res;
  CDisableNotify disableNotify(*this);
  {
    CThreadFolderOperations op(FOLDER_TYPE_CREATE_FOLDER);
    op.FolderOperations = _folderOperations;
    op.Name = newName;
    res = op.DoOperation(*this,
        LangString(IDS_CREATE_FOLDER),
        LangString(IDS_CREATE_FOLDER_ERROR));
    /*
    // fixed for 9.26: we must refresh always
    if (res != S_OK)
      return;
    */
  }
  if (res == S_OK)
  {
    int pos = newName.Find(WCHAR_PATH_SEPARATOR);
    if (pos >= 0)
      newName.DeleteFrom((unsigned)(pos));
    if (!_mySelectMode)
      state.SelectedNames.Clear();
    state.FocusedName = newName;
    state.FocusedName_Defined = true;
    state.SelectFocused = true;
  }
  RefreshTitleAlways();
  RefreshListCtrl(state);
}

void CPanel::CreateFile()
{
  if (IsHashFolder())
    return;

  if (!CheckBeforeUpdate(IDS_CREATE_FILE_ERROR))
    return;

  CDisableTimerProcessing disableTimerProcessing2(*this);
  CSelectedState state;
  SaveSelectedState(state);
  CComboDialog dlg;
  LangString(IDS_CREATE_FILE, dlg.Title);
  LangString(IDS_CREATE_FILE_NAME, dlg.Static);
  LangString(IDS_CREATE_FILE_DEFAULT_NAME, dlg.Value);

  if (dlg.Create(GetParent()) != IDOK)
    return;

  CDisableNotify disableNotify(*this);

  UString newName = dlg.Value;

  if (IsFSFolder())
  {
    UString correctName;
    if (!CorrectFsPath(newName, correctName))
    {
      MessageBox_Error_HRESULT(E_INVALIDARG);
      return;
    }
    newName = correctName;
  }

  HRESULT result = _folderOperations->CreateFile(newName, 0);
  if (result != S_OK)
  {
    MessageBox_Error_HRESULT_Caption(result, LangString(IDS_CREATE_FILE_ERROR));
    // MessageBoxErrorForUpdate(result, IDS_CREATE_FILE_ERROR);
    return;
  }
  int pos = newName.Find(WCHAR_PATH_SEPARATOR);
  if (pos >= 0)
    newName.DeleteFrom((unsigned)pos);
  if (!_mySelectMode)
    state.SelectedNames.Clear();
  state.FocusedName = newName;
  state.FocusedName_Defined = true;
  state.SelectFocused = true;
  RefreshListCtrl(state);
}

void CPanel::RenameFile()
{
  if (!CheckBeforeUpdate(IDS_ERROR_RENAMING))
    return;
  int index = _listView.GetFocusedItem();
  if (index >= 0)
    _listView.EditLabel(index);
}

void CPanel::ChangeComment()
{
  if (IsHashFolder())
    return;
  if (!CheckBeforeUpdate(IDS_COMMENT))
    return;
  CDisableTimerProcessing disableTimerProcessing2(*this);
  int index = _listView.GetFocusedItem();
  if (index < 0)
    return;
  int realIndex = GetRealItemIndex(index);
  if (realIndex == kParentIndex)
    return;
  CSelectedState state;
  SaveSelectedState(state);
  UString comment;
  {
    NCOM::CPropVariant propVariant;
    if (_folder->GetProperty(realIndex, kpidComment, &propVariant) != S_OK)
      return;
    if (propVariant.vt == VT_BSTR)
      comment = propVariant.bstrVal;
    else if (propVariant.vt != VT_EMPTY)
      return;
  }
  UString name = GetItemRelPath2(realIndex);
  CComboDialog dlg;
  dlg.Title = name;
  dlg.Title += " : ";
  AddLangString(dlg.Title, IDS_COMMENT);
  dlg.Value = comment;
  LangString(IDS_COMMENT2, dlg.Static);
  if (dlg.Create(GetParent()) != IDOK)
    return;
  NCOM::CPropVariant propVariant (dlg.Value);

  CDisableNotify disableNotify(*this);
  HRESULT result = _folderOperations->SetProperty(realIndex, kpidComment, &propVariant, NULL);
  if (result != S_OK)
  {
    if (result == E_NOINTERFACE)
      MessageBox_Error_UnsupportOperation();
    else
      MessageBox_Error_HRESULT_Caption(result, L"Set Comment Error");
  }
  RefreshListCtrl(state);
}

// **************** SSS Modification Start ****************
// **************** SSS Modification Start ****************
// Temp file shared between the file manager and 7zG to forward a
// "Yes to All / No to All / Auto rename" overwrite answer to the next
// archive of the same batch. See SssExtractAll below.
static UString SssOwTempFilePath()
{
  wchar_t temp[MAX_PATH];
  UString p;
  if (::GetTempPathW(MAX_PATH, temp) != 0)
  {
    p = temp;
    p += L"sss_batch_ow.txt";
  }
  return p;
}

static UInt32 SssReadOwTempFile(const UString &path)
{
  if (path.IsEmpty())
    return (UInt32)(Int32)-1;
  HANDLE h = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return (UInt32)(Int32)-1;
  char buf[4] = { 0 };
  DWORD read = 0;
  ::ReadFile(h, buf, 2, &read, NULL);
  ::CloseHandle(h);
  if (read >= 1)
  {
    if (buf[0] == 'a')
      return 1; // NOverwriteMode::kOverwrite  -> -aoa
    if (buf[0] == 's')
      return 2; // NOverwriteMode::kSkip      -> -aos
    if (buf[0] == 'u')
      return 3; // NOverwriteMode::kRename    -> -aou
  }
  return (UInt32)(Int32)-1;
}

// 7zG writes %TEMP%\sss_batch_ok.txt when an archive really finished OK
// (see SssWriteBatchOk in NanaZip.Universal ExtractGUI.cpp). The file
// manager deletes the marker before each archive and only marks the archive
// as successfully extracted (and thus deletable) when the marker appears.
static UString SssBatchOkFilePath()
{
  wchar_t temp[MAX_PATH];
  UString p;
  if (::GetTempPathW(MAX_PATH, temp) != 0)
  {
    p = temp;
    p += L"sss_batch_ok.txt";
  }
  return p;
}

static bool SssReadOkFile(const UString &path)
{
  if (path.IsEmpty())
    return false;
  HANDLE h = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return false;
  char buf[4] = { 0 };
  DWORD read = 0;
  ::ReadFile(h, buf, 2, &read, NULL);
  ::CloseHandle(h);
  return read >= 1 && buf[0] == '1';
}

// Delete the successfully extracted archives in one shot after the whole
// batch finished. Recycle Bin via SHFileOperationW (single operation), or
// permanently via NDir::DeleteFileAlways.
static void SssDeleteBatchArchives(const UStringVector &paths, bool permanently)
{
  if (paths.IsEmpty())
    return;
  if (permanently)
  {
    FOR_VECTOR (i, paths)
      NDir::DeleteFileAlways(us2fs(paths[i]));
    return;
  }
  size_t total = 1; // final NUL
  FOR_VECTOR (i, paths)
    total += paths[i].Len() + 1;
  wchar_t *buf = new wchar_t[total];
  wchar_t *p = buf;
  FOR_VECTOR (i, paths)
  {
    MyStringCopy(p, paths[i]);
    p += paths[i].Len() + 1;
  }
  *p = 0;
  SHFILEOPSTRUCTW fo;
  memset(&fo, 0, sizeof(fo));
  fo.wFunc = FO_DELETE;
  fo.pFrom = buf;
  fo.fFlags = FOF_ALLOWUNDO;
  ::SHFileOperationW(&fo);
  delete[] buf;
}

// SSS: arguments for the background extraction loop. The loop runs on a
// worker thread so the file-manager UI thread keeps pumping messages
// while 7zG runs (otherwise the main window freezes / shows "not
// responding" while a dialog is open). UI updates go back to the panel
// through kSssLoopStateMessage / kSssLoopDoneMessage.
struct CSssLoopArgs
{
  HWND PanelHwnd;
  UStringVector Paths;
  CContextMenuInfo Ci;
  bool IsBatch;       // batch view (per-item status column)
  bool ShowDialog;    // SssExtractAll(true): one dialog for all archives
  bool OneByOne;      // per-archive dialog mode
  UString OwTempFile;
  UString OkFile;
  UString DlgFile;     // per-run extract-dialog state (one-by-one only)
  UString DelFile;     // per-archive delete mark (one-by-one only)
  bool DeleteAfter;
  bool DeletePermanently;
};

// Path of the per-run dialog state file shared across the archives of a
// one-by-one extraction loop (7zG writes/reads it - see ExtractGUI.cpp).
static UString SssDlgStateFilePath()
{
  wchar_t temp[MAX_PATH];
  UString p;
  if (::GetTempPathW(MAX_PATH, temp) != 0)
  {
    p = temp;
    p += L"sss_batch_dlg.txt";
  }
  return p;
}

// Per-archive delete mark: 7zG writes '1' here when the dialog asked to
// delete the archive after extraction; the file manager collects the
// marked archives and deletes them all together at the end of the loop.
static UString SssDelFilePath()
{
  wchar_t temp[MAX_PATH];
  UString p;
  if (::GetTempPathW(MAX_PATH, temp) != 0)
  {
    p = temp;
    p += L"sss_batch_del.txt";
  }
  return p;
}

static bool SssReadDelFile(const UString &path)
{
  if (path.IsEmpty())
    return false;
  HANDLE h = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return false;
  char buf[4] = { 0 };
  DWORD read = 0;
  ::ReadFile(h, buf, 2, &read, NULL);
  ::CloseHandle(h);
  return read >= 1 && buf[0] == '1';
}

// SSS: "select files" prompt. The modal box runs its own message loop:
// clicks made while it is up get dispatched straight into the panel's
// WndProc (nested call), which would re-trigger this prompt. A re-entrancy
// guard collapses those nested calls; the drain below then drops any
// toolbar clicks left in the queue (the user already saw the prompt) and
// re-dispatches everything else in FIFO order so no other input is lost.
void CPanel::ShowNoSelectionMessage() const
{
  static bool s_inPrompt = false;
  if (s_inPrompt)
    return; // nested dispatch from the modal box's message loop
  s_inPrompt = true;
  MessageBox_Error_LangID(IDS_SELECT_FILES);
  s_inPrompt = false;
  MSG msg;
  while (::PeekMessageW(&msg, NULL, WM_COMMAND, WM_COMMAND, PM_REMOVE))
  {
    const WORD cmd = (WORD)msg.wParam;
    if (cmd >= 1070 && cmd <= 1073)
      continue; // queued duplicate toolbar click that also needs a selection
    ::TranslateMessage(&msg);
    ::DispatchMessageW(&msg);
  }
}

static void SssPostLoopState(HWND hwnd, unsigned index, unsigned state)
{
  if (::IsWindow(hwnd))
    ::PostMessageW(hwnd, kSssLoopStateMessage, (WPARAM)index, (LPARAM)state);
}

static DWORD WINAPI SssExtractLoopThread(void *param)
{
  CSssLoopArgs *a = (CSssLoopArgs *)param;
  UStringVector okPaths;
  unsigned done = 0;
  unsigned failed = 0;

  if (a->ShowDialog)
  {
    // One dialog for ALL archives: 7zG shows a single "Extract to..."
    // dialog and extracts every archive into the chosen folder.
    UStringVector all;
    UString firstParent;
    for (unsigned i = 0; i < a->Paths.Size(); i++)
    {
      FString fullPathF;
      FString parentFolder;
      if (NFile::NName::GetFullPath(us2fs(a->Paths[i]), fullPathF) &&
          NFile::NDir::GetOnlyDirPrefix(fullPathF, parentFolder))
      {
        all.Add(fs2us(fullPathF));
        if (i == 0)
          firstParent = fs2us(parentFolder);
      }
    }
    if (!all.IsEmpty())
    {
      for (unsigned i = 0; i < a->Paths.Size(); i++)
        SssPostLoopState(a->PanelHwnd, i, 1);
      ::ExtractArchives(all, firstParent, true, false, a->Ci.WriteZone, true, false, (UInt32)(Int32)-1, true);
      // Dialog mode: 7zG owns the result; we can't track per-archive
      // success, so mark everything as done.
      for (unsigned i = 0; i < a->Paths.Size(); i++)
        SssPostLoopState(a->PanelHwnd, i, 2);
    }
  }
  else
  {
    // SSS: per-run temp state starts clean.
    if (a->OneByOne)
    {
      NDir::DeleteFileAlways(us2fs(a->OwTempFile));
      NDir::DeleteFileAlways(us2fs(a->DlgFile));
      NDir::DeleteFileAlways(us2fs(a->DelFile));
    }
    // Per-batch overwrite policy, shared across archives through a temp
    // file: each archive runs in its own 7zG process, so "Yes to All"
    // picked in one archive is forwarded to the next ones.
    UInt32 batchMode = (UInt32)(Int32)-1; // -1: let 7zG ask
    for (unsigned i = 0; i < a->Paths.Size(); i++)
    {
      if (!::IsWindow(a->PanelHwnd))
        break; // panel is gone: stop quietly
      SssPostLoopState(a->PanelHwnd, i, 1);
      bool ok = false;
      FString fullPathF;
      FString parentFolder;
      if (NFile::NName::GetFullPath(us2fs(a->Paths[i]), fullPathF) &&
          NFile::NDir::GetOnlyDirPrefix(fullPathF, parentFolder))
      {
        UStringVector single;
        single.Add(fs2us(fullPathF));
        if (a->OneByOne)
        {
          // Dialog per archive; advance only on real success (7zG wrote
          // the ok marker). Any cancel or failure stops the sequence.
          // overwriteMode forwards a "to all" overwrite answer from a
          // previous archive; useDlgState carries the previous dialog's
          // choices (path, modes, checkboxes, password) into this one.
          // suppressDelete=true: 7zG must NOT delete yet - the archives
          // marked for deletion are removed together at the end.
          NDir::DeleteFileAlways(us2fs(a->OkFile));
          NDir::DeleteFileAlways(us2fs(a->DelFile)); // this archive's delete mark
          ::ExtractArchives(single, fs2us(parentFolder), true, false, a->Ci.WriteZone, true, false, batchMode, true, true, true);
          ok = SssReadOkFile(a->OkFile);
          if (ok && SssReadDelFile(a->DelFile))
            okPaths.Add(fs2us(fullPathF)); // marked for deletion
          if (batchMode == (UInt32)(Int32)-1)
            batchMode = SssReadOwTempFile(a->OwTempFile); // "to all" answer
        }
        else
        {
          if (batchMode == (UInt32)(Int32)-1)
            NDir::DeleteFileAlways(us2fs(a->OwTempFile)); // drop leftovers
          NDir::DeleteFileAlways(us2fs(a->OkFile));       // this archive's marker
          ::ExtractArchives(single, fs2us(parentFolder), false, false, a->Ci.WriteZone, true, false, batchMode, true, true);
          ok = SssReadOkFile(a->OkFile);
          if (batchMode == (UInt32)(Int32)-1)
            batchMode = SssReadOwTempFile(a->OwTempFile);
        }
      }
      SssPostLoopState(a->PanelHwnd, i, ok ? 2 : 3);
      if (ok)
      {
        done++;
        if (!a->OneByOne)
          okPaths.Add(fs2us(fullPathF)); // batch: all successful archives
      }
      else
      {
        failed++;
        break; // cancelled or failed: stop the whole run
      }
    }
    NDir::DeleteFileAlways(us2fs(a->OwTempFile));
    NDir::DeleteFileAlways(us2fs(a->OkFile));
    if (a->OneByOne && !a->DlgFile.IsEmpty())
      NDir::DeleteFileAlways(us2fs(a->DlgFile));
    if (a->OneByOne && !a->DelFile.IsEmpty())
      NDir::DeleteFileAlways(us2fs(a->DelFile));
    // Delete the marked archives together, in one shot, after the whole
    // run has finished (single Recycle Bin operation).
    if (a->OneByOne)
    {
      if (!okPaths.IsEmpty())
        SssDeleteBatchArchives(okPaths, a->DeletePermanently);
    }
    else if (a->DeleteAfter && !okPaths.IsEmpty())
      SssDeleteBatchArchives(okPaths, a->DeletePermanently);
    // Only report a summary when at least one archive was actually
    // processed. If the run was cancelled or failed right away (e.g. the
    // user opened the dialog and closed it without extracting anything),
    // a "0 processed" popup would just be noise.
    if (done > 0)
    {
      UString msg = L"已解压 ";
      msg.Add_UInt32(done);
      msg += L" 个归档";
      if (failed > 0)
      {
        msg += L"，";
        msg.Add_UInt32(failed);
        msg += a->OneByOne ? L" 个未完成" : L" 个失败";
      }
      MessageBoxW(0, msg, a->OneByOne ? L"逐个提取" : L"批量解压", MB_ICONINFORMATION);
    }
  }

  ::PostMessageW(a->PanelHwnd, kSssLoopDoneMessage, 0, 0);
  delete a;
  return 0;
}

static void SssStartLoop(HWND hwnd, CSssLoopArgs *args)
{
  args->PanelHwnd = hwnd;
  HANDLE h = ::CreateThread(NULL, 0, SssExtractLoopThread, (void *)args, 0, NULL);
  if (!h)
    delete args;
  else
    ::CloseHandle(h); // detached: the thread owns its args
}

void CPanel::SssExtractAll(bool showDialog)
{
  if (_sssLoopRunning)
    return;
  if (!IsSssBatchFolder())
    return;
  CSssBatchFolder *folder = static_cast<CSssBatchFolder *>((IFolderFolder *)_folder);
  const UStringVector &paths = folder->GetPaths();
  if (paths.IsEmpty())
    return;
  CContextMenuInfo ci;
  ci.Load();

  CSssLoopArgs *args = new CSssLoopArgs;
  args->Paths = paths;
  args->Ci = ci;
  args->IsBatch = true;
  args->ShowDialog = showDialog;
  args->OneByOne = false;
  args->OwTempFile = SssOwTempFilePath();
  args->OkFile = SssBatchOkFilePath();
  args->DeleteAfter = WantDeleteAfterExtract();
  args->DeletePermanently = WantDeletePermanently();
  _sssLoopRunning = true;
  SssStartLoop((HWND)*this, args);
}

// **************** SSS Modification End ****************
// **************** SSS Modification End ****************

// SSS: one-by-one extraction - one 7zG dialog per archive. Runs on a
// background thread (see SssExtractLoopThread) so the file-manager UI
// stays responsive while a dialog is open. The sequence advances to the
// next archive only when the previous one really finished (7zG wrote the
// ok marker - success only). Cancelling a dialog, or any archive failure,
// stops the whole sequence. Target folder defaults to the smart-extract
// folder of each archive (the dialog lets the user change it). Like the
// batch path, deletion of the archives happens together at the end when
// "delete after extract" is on.
void CPanel::SssExtractOneByOne()
{
  if (_sssLoopRunning)
    return;
  UStringVector paths;
  if (IsSssBatchFolder())
  {
    CSssBatchFolder *folder = static_cast<CSssBatchFolder *>((IFolderFolder *)_folder);
    paths = folder->GetPaths();
  }
  else
  {
    if (_parentFolders.Size() > 0)
    {
      // Inside an archive there is no per-item "extract one by one"
      // concept - fall back to the classic extraction of the selection.
      ExtractFromArchive();
      return;
    }
    CRecordVector<UInt32> indices;
    GetOperatedItemIndices(indices);
    if (indices.Size() == 0)
    {
      ShowNoSelectionMessage();
      return;
    }
    GetFilePaths(indices, paths);
  }
  if (paths.IsEmpty())
    return; // the prompt was already shown by GetFilePaths (folders-only case)

  CContextMenuInfo ci;
  ci.Load();
  CSssLoopArgs *args = new CSssLoopArgs;
  args->Paths = paths;
  args->Ci = ci;
  args->IsBatch = IsSssBatchFolder();
  args->ShowDialog = false;
  args->OneByOne = true;
  args->OwTempFile = SssOwTempFilePath();
  args->OkFile = SssBatchOkFilePath();
  args->DlgFile = SssDlgStateFilePath();
  args->DelFile = SssDelFilePath();
  args->DeleteAfter = WantDeleteAfterExtract();
  args->DeletePermanently = WantDeletePermanently();
  _sssLoopRunning = true;
  SssStartLoop((HWND)*this, args);
}
// **************** SSS Modification End ****************
