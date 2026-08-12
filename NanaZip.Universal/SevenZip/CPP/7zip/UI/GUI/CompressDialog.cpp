// CompressDialog.cpp
// 压缩对话框 Win32 壳：负责控件显示、事件收集与浏览/确认交互。
// 所有规则与状态在 CompressDialogCore，壳不复制任何规则。

#include "StdAfx.h"

#include "../../../Common/StringConvert.h"

#include "../../Common/MethodProps.h"

#include "../FileManager/BrowseDialog.h"
#include "../FileManager/PropertyName.h"
#include "../FileManager/SplitUtils.h"
#include "../FileManager/resourceGui.h"

#include "../FileManager/LangUtils.h"

#include "CompressDialog.h"

#include "CompressDialogRes.h"
#include "ExtractRes.h"
#include "resource2.h"

// #define PRINT_PARAMS

#ifdef Z7_LANG

static const UInt32 kLangIDs[] =
{
  IDT_COMPRESS_ARCHIVE,
  IDT_COMPRESS_UPDATE_MODE,
  IDT_COMPRESS_FORMAT,
  IDT_COMPRESS_LEVEL,
  IDT_COMPRESS_METHOD,
  IDT_COMPRESS_DICTIONARY,
  IDT_COMPRESS_ORDER,
  IDT_COMPRESS_SOLID,
  IDT_COMPRESS_THREADS,
  IDT_COMPRESS_PARAMETERS,

  IDB_COMPRESS_OPTIONS, // IDS_OPTIONS

  IDG_COMPRESS_OPTIONS,
  IDX_COMPRESS_SFX,
  IDX_COMPRESS_SHARED,
  IDX_COMPRESS_DEL,

  IDT_COMPRESS_MEMORY,
  IDT_COMPRESS_MEMORY_DE,

  IDG_COMPRESS_ENCRYPTION,
  IDT_COMPRESS_ENCRYPTION_METHOD,
  IDX_COMPRESS_ENCRYPT_FILE_NAMES,

  IDT_PASSWORD_ENTER,
  IDT_PASSWORD_REENTER,
  IDX_PASSWORD_SHOW,

  IDT_SPLIT_TO_VOLUMES,
  IDT_COMPRESS_PATH_MODE,
};
#endif

using namespace NWindows;
using namespace NFile;
using namespace NDir;

static const UINT k_Message_ArcChanged = WM_APP + 1;

// in ExtractDialog.cpp
extern void AddComboItems(NControl::CComboBox &combo, const UInt32 *langIDs, unsigned numItems, const int *values, int curVal);

static void AddFilter(CObjectVector<CBrowseFilterInfo> &filters,
    const UString &description, const UString &ext)
{
  CBrowseFilterInfo &f = filters.AddNew();
  UString mask ("*.");
  mask += ext;
  f.Masks.Add(mask);
  f.Description = description;
  f.Description += " (";
  f.Description += mask;
  f.Description += ")";
}


static const char * const k_DontSave_Exts =
  "xpi odt ods docx xlsx ";
bool CCompressDialog::OnInit()
{
  #ifdef Z7_LANG
  LangSetWindowText(*this, IDD_COMPRESS);
  LangSetDlgItems(*this, kLangIDs, Z7_ARRAY_SIZE(kLangIDs));
  #endif

  Core.Initialize();

  _password1Control.Attach(GetItem(IDE_COMPRESS_PASSWORD1));
  _password2Control.Attach(GetItem(IDE_COMPRESS_PASSWORD2));
  _password1Control.SetText(Core.Info.Password);
  _password2Control.SetText(Core.Info.Password);
  _encryptionMethod.Attach(GetItem(IDC_COMPRESS_ENCRYPTION_METHOD));

  m_ArchivePath.Attach(GetItem(IDC_COMPRESS_ARCHIVE));
  m_Format.Attach(GetItem(IDC_COMPRESS_FORMAT)); // that combo has CBS_SORT style in resources
  m_Level.Attach(GetItem(IDC_COMPRESS_LEVEL));
  m_Method.Attach(GetItem(IDC_COMPRESS_METHOD));
  m_Dictionary.Attach(GetItem(IDC_COMPRESS_DICTIONARY));

  _dictionaryCombo_left = 0; // 230;

  m_Order.Attach(GetItem(IDC_COMPRESS_ORDER));
  m_Solid.Attach(GetItem(IDC_COMPRESS_SOLID));
  m_NumThreads.Attach(GetItem(IDC_COMPRESS_THREADS));
  m_MemUse.Attach(GetItem(IDC_COMPRESS_MEM_USE));

  m_UpdateMode.Attach(GetItem(IDC_COMPRESS_UPDATE_MODE));
  m_PathMode.Attach(GetItem(IDC_COMPRESS_PATH_MODE));

  m_Volume.Attach(GetItem(IDC_COMPRESS_VOLUME));
  m_Params.Attach(GetItem(IDE_COMPRESS_PARAMETERS));

  AddVolumeItems(m_Volume);

  Core.ShowPassword = Core.RegistryInfo.ShowPassword;
  CheckButton(IDX_PASSWORD_SHOW, Core.ShowPassword);
  Core.EncryptHeadersChecked = Core.RegistryInfo.EncryptHeaders;
  CheckButton(IDX_COMPRESS_ENCRYPT_FILE_NAMES, Core.EncryptHeadersChecked);

  UpdatePasswordControl();

  Core.CalcFormats();
  SyncFormat();

  Core.SfxChecked = Core.Info.SFXMode;
  CheckButton(IDX_COMPRESS_SFX, Core.SfxChecked);

  {
    UString fileName;
    SetArcPathFields(Core.Info.ArcPath, fileName, true);
    Core.StartDirPrefix = Core.DirPrefix;
    SetArchiveName(fileName);
  }

  for (unsigned i = 0; i < Core.RegistryInfo.ArcPaths.Size() && i < kHistorySize; i++)
    m_ArchivePath.AddString(Core.RegistryInfo.ArcPaths[i]);

  AddComboItems(m_UpdateMode, k_UpdateMode_IDs, Z7_ARRAY_SIZE(k_UpdateMode_IDs),
      k_UpdateMode_Vals, Core.Info.UpdateMode);

  AddComboItems(m_PathMode, k_PathMode_IDs, Z7_ARRAY_SIZE(k_PathMode_IDs),
      k_PathMode_Vals, Core.Info.PathMode);

  Core.OpenShareForWrite = Core.Info.OpenShareForWrite;
  CheckButton(IDX_COMPRESS_SHARED, Core.OpenShareForWrite);
  Core.DeleteAfterCompressing = Core.Info.DeleteAfterCompressing;
  CheckButton(IDX_COMPRESS_DEL, Core.DeleteAfterCompressing);

  FormatChanged(false); // isChanged

  NormalizePosition();

  return CModalDialog::OnInit();
}


void CCompressDialog::CollectTexts()
{
  UString s;
  _password1Control.GetText(Core.Password);
  _password2Control.GetText(Core.PasswordConfirmation);
  Core.ShowPassword = IsShowPasswordChecked();
  m_Volume.GetText(Core.VolumeText);
  m_Params.GetText(Core.Info.Options);
  m_ArchivePath.GetText(Core.ArchiveName);
}


void CCompressDialog::UpdatePasswordControl()
{
  const bool showPassword = IsShowPasswordChecked();
  Core.ShowPassword = showPassword;
  const TCHAR c = showPassword ? (TCHAR)0: TEXT('*');
  _password1Control.SetPasswordChar((WPARAM)c);
  _password2Control.SetPasswordChar((WPARAM)c);
  UString password;
  _password1Control.GetText(password);
  _password1Control.SetText(password);
  _password2Control.GetText(password);
  _password2Control.SetText(password);

  ShowItem_Bool(IDT_PASSWORD_REENTER, !showPassword);
  _password2Control.Show_Bool(!showPassword);
}


void CCompressDialog::SetCurSelByValue(NControl::CComboBox &combo,
    const CObjectVector<CCompressDialogCore::COptionItem> &items, UInt64 value)
{
  for (int i = (int)items.Size() - 1; i >= 0; i--)
    if (items[i].Value == value)
    {
      combo.SetCurSel(i);
      return;
    }
  if (items.Size() > 0)
    combo.SetCurSel(0);
}


void CCompressDialog::SyncFormat()
{
  m_Format.ResetContent();
  int curSel = 0;
  FOR_VECTOR (i, Core.FormatItems)
  {
    const int index = (int)m_Format.AddString_SetItemData(
        GetSystemString(Core.FormatItems[i].Display), (LPARAM)Core.FormatItems[i].Value);
    if (Core.FormatItems[i].Value == (UInt64)(UInt32)Core.FormatIndex)
      curSel = index;
  }
  m_Format.SetCurSel(curSel);
}


void CCompressDialog::SyncLevel()
{
  m_Level.ResetContent();
  FOR_VECTOR (i, Core.LevelItems)
    m_Level.AddString_SetItemData(GetSystemString(Core.LevelItems[i].Display), (LPARAM)Core.LevelItems[i].Value);
  SetCurSelByValue(m_Level, Core.LevelItems, Core.Level);
  EnableMultiCombo(IDC_COMPRESS_LEVEL);
}


void CCompressDialog::SyncMethod()
{
  m_Method.ResetContent();
  FOR_VECTOR (i, Core.MethodItems)
    m_Method.AddString_SetItemData(GetSystemString(Core.MethodItems[i].Display), (LPARAM)Core.MethodItems[i].Value);
  SetCurSelByValue(m_Method, Core.MethodItems, (UInt64)(Int64)Core.MethodID);
  EnableMultiCombo(IDC_COMPRESS_METHOD);
}


void CCompressDialog::SyncDictionary()
{
  m_Dictionary.ResetContent();
  FOR_VECTOR (i, Core.DictionaryItems)
    m_Dictionary.AddString_SetItemData(GetSystemString(Core.DictionaryItems[i].Display), (LPARAM)Core.DictionaryItems[i].Value);
  SetCurSelByValue(m_Dictionary, Core.DictionaryItems, Core.Dict64);
  EnableMultiCombo(IDC_COMPRESS_DICTIONARY);
}


void CCompressDialog::SyncOrder()
{
  m_Order.ResetContent();
  FOR_VECTOR (i, Core.OrderItems)
    m_Order.AddString_SetItemData(GetSystemString(Core.OrderItems[i].Display), (LPARAM)Core.OrderItems[i].Value);
  SetCurSelByValue(m_Order, Core.OrderItems, Core.Order);
  EnableMultiCombo(IDC_COMPRESS_ORDER);
}


void CCompressDialog::SyncSolid()
{
  m_Solid.ResetContent();
  FOR_VECTOR (i, Core.SolidItems)
    m_Solid.AddString_SetItemData(GetSystemString(Core.SolidItems[i].Display), (LPARAM)Core.SolidItems[i].Value);
  SetCurSelByValue(m_Solid, Core.SolidItems, Core.BlockLogSize);
  EnableMultiCombo(IDC_COMPRESS_SOLID);
}


void CCompressDialog::SyncThreads()
{
  m_NumThreads.ResetContent();
  FOR_VECTOR (i, Core.ThreadItems)
    m_NumThreads.AddString_SetItemData(GetSystemString(Core.ThreadItems[i].Display), (LPARAM)Core.ThreadItems[i].Value);
  SetCurSelByValue(m_NumThreads, Core.ThreadItems, Core.NumThreads);
  EnableMultiCombo(IDC_COMPRESS_THREADS);
}


void CCompressDialog::SyncMemUse()
{
  m_MemUse.ResetContent();
  FOR_VECTOR (i, Core.MemUseItems)
    m_MemUse.AddString_SetItemData(GetSystemString(Core.MemUseItems[i].Display), (LPARAM)Core.MemUseItems[i].Value);
  SetCurSelByValue(m_MemUse, Core.MemUseItems, (UInt64)(Int64)Core.MemUseIndex);
  EnableMultiCombo(IDC_COMPRESS_MEM_USE);
}


void CCompressDialog::SyncEncryptionMethod()
{
  _encryptionMethod.ResetContent();
  FOR_VECTOR (i, Core.EncryptionMethodItems)
    _encryptionMethod.AddString_SetItemData(GetSystemString(Core.EncryptionMethodItems[i].Display), (LPARAM)Core.EncryptionMethodItems[i].Value);
  SetCurSelByValue(_encryptionMethod, Core.EncryptionMethodItems, (UInt64)(Int64)Core.EncryptionMethodIndex);
  EnableMultiCombo(IDC_COMPRESS_ENCRYPTION_METHOD);
}


void CCompressDialog::SyncMemoryUsage()
{
  Core.UpdateMemoryTexts();
  SetItemText(IDT_COMPRESS_MEMORY_VALUE, Core.MemoryValueText);
  SetItemText(IDT_COMPRESS_MEMORY_DE_VALUE, Core.DecompressMemoryText);
}


void CCompressDialog::SyncOptionsSummary()
{
  Core.UpdateOptionsSummary();
  SetItemText(IDT_COMPRESS_OPTIONS, Core.OptionsSummaryText);
}


void CCompressDialog::SyncParams()
{
  const NCompression::CFormatOptions &fo = Core.Get_FormatOptions();
  m_Params.SetText(fo.Options);
}


void CCompressDialog::SyncArcPathFields()
{
  SetItemText(IDT_COMPRESS_ARCHIVE_FOLDER, Core.DirPrefix);
  m_ArchivePath.SetText(Core.ArchiveName);
}


void CCompressDialog::SyncHardwareThreads()
{
  SetItemText(IDT_COMPRESS_HARDWARE_THREADS, Core.HardwareThreadsText);
}


void CCompressDialog::EnableMultiCombo(unsigned id)
{
  NWindows::NControl::CComboBox combo;
  combo.Attach(GetItem(id));
  const bool enable = (combo.GetCount() > 1);
  EnableItem(id, enable);
}


void CCompressDialog::FormatChangedControls()
{
  SyncFormat();
  SyncLevel();
  SyncMethod();
  SyncDictionary();
  SyncOrder();
  SyncSolid();
  SyncThreads();
  SyncMemUse();
  SyncEncryptionMethod();
  CheckSFXControlsEnable();
  SyncMemoryUsage();
  SyncOptionsSummary();
  SyncParams();
  SyncHardwareThreads();
  SyncArcPathFields();

  EnableItem(IDG_COMPRESS_ENCRYPTION, Core.EncryptSupported);
  EnableItem(IDT_PASSWORD_ENTER, Core.EncryptSupported);
  EnableItem(IDT_PASSWORD_REENTER, Core.EncryptSupported);
  EnableItem(IDE_COMPRESS_PASSWORD1, Core.EncryptSupported);
  EnableItem(IDE_COMPRESS_PASSWORD2, Core.EncryptSupported);
  EnableItem(IDX_PASSWORD_SHOW, Core.EncryptSupported);

  EnableItem(IDT_COMPRESS_ENCRYPTION_METHOD, Core.EncryptSupported);
  EnableItem(IDC_COMPRESS_ENCRYPTION_METHOD, Core.EncryptSupported);
  EnableItem(IDX_COMPRESS_ENCRYPT_FILE_NAMES, Core.EncryptFileNamesSupported);

  ShowItem_Bool(IDX_COMPRESS_ENCRYPT_FILE_NAMES, Core.EncryptFileNamesSupported);

  const bool memEnable = Core.MemUseSupported;
  ShowItem_Bool(IDT_COMPRESS_MEMORY, memEnable);
  ShowItem_Bool(IDT_COMPRESS_MEMORY_VALUE, memEnable);
  ShowItem_Bool(IDT_COMPRESS_MEMORY_DE, memEnable);
  ShowItem_Bool(IDT_COMPRESS_MEMORY_DE_VALUE, memEnable);
  ShowItem_Bool(IDC_COMPRESS_MEM_USE, memEnable);
  EnableItem(IDC_COMPRESS_MEM_USE, memEnable);
}


void CCompressDialog::FormatChanged(bool isChanged)
{
  Core.FormatChanged(isChanged);
  FormatChangedControls();
}


void CCompressDialog::CheckSFXControlsEnable()
{
  Core.CheckSFX();
  if (!Core.SfxSupported)
    CheckButton(IDX_COMPRESS_SFX, false);
  EnableItem(IDX_COMPRESS_SFX, Core.SfxSupported);
}


void CCompressDialog::CheckSFXNameChange()
{
  UString s;
  m_ArchivePath.GetText(s);
  Core.ArchiveName = s;
  const bool isSFX = Core.IsSfx();
  Core.CheckSFXNameChange();
  CheckSFXControlsEnable();
  if (isSFX != Core.IsSfx())
    m_ArchivePath.SetText(Core.ArchiveName);
}


bool CCompressDialog::OnButtonClicked(unsigned buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDB_COMPRESS_SET_ARCHIVE:
    {
      OnButtonSetArchive();
      return true;
    }
    case IDX_COMPRESS_SFX:
    {
      Core.OnSfxChecked(IsButtonCheckedBool(IDX_COMPRESS_SFX));
      SyncMethod();
      SyncDictionary();
      SyncOrder();
      SyncArcPathFields();
      SyncMemoryUsage();
      return true;
    }
    case IDX_PASSWORD_SHOW:
    {
      UpdatePasswordControl();
      return true;
    }
    case IDB_COMPRESS_OPTIONS:
    {
      COptionsDialog dialog(&Core);
      if (dialog.Create(*this) == IDOK)
        ShowOptionsString();
      return true;
    }
  }
  return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
}


bool CCompressDialog::GetFinalPath_Smart(UString &resPath) const
{
  return Core.GetFinalPath_Smart(resPath);
}


bool CCompressDialog::SetArcPathFields(const UString &path)
{
  UString name;
  return SetArcPathFields(path, name, true); // always
}


bool CCompressDialog::SetArcPathFields(const UString &path, UString &name, bool always)
{
  const bool res = Core.SetArcPathFields(path, name, always);
  SetItemText(IDT_COMPRESS_ARCHIVE_FOLDER, Core.DirPrefix);
  m_ArchivePath.SetText(name);
  return res;
}


void CCompressDialog::OnButtonSetArchive()
{
  UString path;
  CollectTexts();
  if (!GetFinalPath_Smart(path))
  {
    ShowErrorMessage(*this, k_IncorrectPathMessage);
    return;
  }

  int filterIndex;
  CObjectVector<CBrowseFilterInfo> filters;
  unsigned numFormats = 0;

  const bool isSFX = Core.IsSfx();
  if (isSFX)
  {
    filterIndex = 0;
    const UString ext ("exe");
    AddFilter(filters, ext, ext);
  }
  else
  {
    filterIndex = m_Format.GetCurSel();
    numFormats = (unsigned)m_Format.GetCount();

    // filters [0, ... numFormats - 1] corresponds to items in m_Format combo
    UString desc;
    UStringVector masks;
    CStringFinder finder;

    for (unsigned i = 0; i < numFormats; i++)
    {
      const CArcInfoEx &ai = (*Core.ArcFormats)[(unsigned)m_Format.GetItemData(i)];
      CBrowseFilterInfo &f = filters.AddNew();
      f.Description = ai.Name;
      f.Description += " (";
      bool needSpace_desc = false;

      FOR_VECTOR (k, ai.Exts)
      {
        const UString &ext = ai.Exts[k].Ext;
        UString mask ("*.");
        mask += ext;

        if (finder.FindWord_In_LowCaseAsciiList_NoCase(k_DontSave_Exts, ext))
          continue;

        f.Masks.Add(mask);
        masks.Add(mask);
        if (needSpace_desc)
          f.Description.Add_Space();
        needSpace_desc = true;
        f.Description += ext;
      }
      f.Description += ")";
      // we use only main ext in desc to reduce the size of list
      if (i != 0)
        desc.Add_Space();
      desc += ai.GetMainExt();
    }

    CBrowseFilterInfo &f = filters.AddNew();
    f.Description = LangString(IDT_COMPRESS_ARCHIVE); // IDS_ARCHIVES_COLON;
    if (f.Description.IsEmpty())
      GetItemText(IDT_COMPRESS_ARCHIVE, f.Description);
    f.Description.RemoveChar(L'&');
    // f.Description = "archive";
    f.Description += " (";
    f.Description += desc;
    f.Description += ")";
    f.Masks = masks;
  }

  AddFilter(filters, LangString(IDS_OPEN_TYPE_ALL_FILES), UString("*"));
  if (filterIndex < 0)
    filterIndex = (int)filters.Size() - 1;

  const UString title = LangString(IDS_COMPRESS_SET_ARCHIVE_BROWSE);
  CBrowseInfo bi;
  bi.lpstrTitle = title;
  bi.SaveMode = true;
  bi.FilterIndex = filterIndex;
  bi.hwndOwner = *this;
  bi.FilePath = path;

  if (!bi.BrowseForFile(filters))
    return;

  path = bi.FilePath;

  if (isSFX)
  {
    const int dotPos = GetExtDotPos(path);
    if (dotPos >= 0)
      path.DeleteFrom(dotPos);
    path += kExeExt;
  }
  else
  if ((unsigned)bi.FilterIndex < numFormats)
  {
    // archive format was confirmed. So we try to set format extension
    bool needAddExt = true;
    const CArcInfoEx &ai = (*Core.ArcFormats)[(unsigned)m_Format.GetItemData((unsigned)bi.FilterIndex)];
    const int dotPos = GetExtDotPos(path);
    if (dotPos >= 0)
    {
      const UString ext = path.Ptr(dotPos + 1);
      if (ai.FindExtension(ext) >= 0)
        needAddExt = false;
    }
    if (needAddExt)
    {
      if (path.IsEmpty() || path.Back() != '.')
        path.Add_Dot();
      path += ai.GetMainExt();
    }
  }

  SetArcPathFields(path);

  if (!isSFX)
  if ((unsigned)bi.FilterIndex < numFormats)
  if (bi.FilterIndex != m_Format.GetCurSel())
  {
    Core.OnFormatSelected((int)m_Format.GetItemData((unsigned)bi.FilterIndex));
    FormatChangedControls();
    SyncArcPathFields();
    return;
  }

  ArcPath_WasChanged(path);
}


bool CCompressDialog::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
    case k_Message_ArcChanged:
    {
      const int select = m_ArchivePath.GetCurSel();
      if ((unsigned)select < Core.RegistryInfo.ArcPaths.Size())
      {
        const UString &path = Core.RegistryInfo.ArcPaths[select];
        SetArcPathFields(path);
      }
      return 0;
    }
  }
  return CModalDialog::OnMessage(message, wParam, lParam);
}


bool CCompressDialog::OnCommand(unsigned code, unsigned itemID, LPARAM lParam)
{
  if (code == CBN_SELCHANGE)
  {
    switch (itemID)
    {
      case IDC_COMPRESS_ARCHIVE:
      {
        /* CBN_SELCHANGE is called before actual value of combo text will be changed.
           So GetText() here returns old value (before change) of combo text.
           So here we can change all controls except of m_ArchivePath.
        */
        const int select = m_ArchivePath.GetCurSel();
        if ((unsigned)select < Core.RegistryInfo.ArcPaths.Size())
        {
          const UString &path = Core.RegistryInfo.ArcPaths[select];
          ArcPath_WasChanged(path);
          // we use PostMessage(k_Message_ArcChanged) here that later will change m_ArchivePath control
          PostMsg(k_Message_ArcChanged);
        }
        return true;
      }

      case IDC_COMPRESS_FORMAT:
      {
        CollectTexts();
        Core.OnFormatSelected((int)m_Format.GetItemData_of_CurSel());
        FormatChangedControls();
        return true;
      }

      case IDC_COMPRESS_LEVEL:
      {
        CollectTexts();
        Core.OnLevelSelected((UInt32)m_Level.GetItemData_of_CurSel());
        SyncDictionary();
        SyncOrder();
        SyncSolid();
        SyncThreads();
        CheckSFXNameChange();
        SyncMemoryUsage();
        return true;
      }

      case IDC_COMPRESS_METHOD:
      {
        CollectTexts();
        Core.OnMethodSelected((int)(Int32)(UInt32)m_Method.GetItemData_of_CurSel());
        SyncDictionary();
        SyncOrder();
        SyncLevel();
        SyncSolid();
        SyncThreads();
        CheckSFXNameChange();
        SyncMemoryUsage();
        return true;
      }

      case IDC_COMPRESS_DICTIONARY:
      {
        CollectTexts();
        Core.OnDictionarySelected((UInt64)(size_t)m_Dictionary.GetItemData_of_CurSel());
        SyncDictionary();
        SyncSolid();
        SyncThreads();
        SyncMemoryUsage();
        return true;
      }

      case IDC_COMPRESS_ORDER:
      {
        Core.OnOrderSelected((UInt32)m_Order.GetItemData_of_CurSel());
       #ifdef PRINT_PARAMS
        Print_Params();
       #endif
        return true;
      }

      case IDC_COMPRESS_SOLID:
      {
        Core.OnSolidSelected((UInt32)m_Solid.GetItemData_of_CurSel());
        SyncMemoryUsage();
        return true;
      }

      case IDC_COMPRESS_THREADS:
      {
        Core.OnThreadsSelected((UInt32)m_NumThreads.GetItemData_of_CurSel());
        SyncMemoryUsage();
        return true;
      }

      case IDC_COMPRESS_MEM_USE:
      {
        CollectTexts();
        Core.OnMemUseSelected((int)m_MemUse.GetItemData_of_CurSel());
        SyncThreads();
        SyncMemoryUsage();
        return true;
      }
    }
  }
  return CModalDialog::OnCommand(code, itemID, lParam);
}


void CCompressDialog::ArcPath_WasChanged(const UString &path)
{
  UString s;
  m_ArchivePath.GetText(s);
  Core.ArchiveName = s;
  if (Core.ArcPathChanged(path))
    FormatChangedControls();
}


void CCompressDialog::SetArchiveName2(bool prevWasSFX)
{
  UString fileName;
  m_ArchivePath.GetText(fileName);
  Core.ArchiveName = fileName;
  Core.SetArchiveName2(prevWasSFX);
  m_ArchivePath.SetText(Core.ArchiveName);
}


void CCompressDialog::SetArchiveName(const UString &name)
{
  Core.SetArchiveName(name);
  m_ArchivePath.SetText(Core.ArchiveName);
}


void CCompressDialog::SaveOptionsInMem()
{
  m_Params.GetText(Core.Info.Options);
  Core.SaveOptionsInMem();
}


void CCompressDialog::ShowOptionsString()
{
  Core.UpdateOptionsSummary();
  SetItemText(IDT_COMPRESS_OPTIONS, Core.OptionsSummaryText);
}


void CCompressDialog::OnOK()
{
  CollectTexts();

  const int umSel = m_UpdateMode.GetCurSel();
  if (umSel >= 0)
    Core.Info.UpdateMode = (NCompressDialog::NUpdateMode::EEnum)k_UpdateMode_Vals[umSel];
  const int pmSel = m_PathMode.GetCurSel();
  if (pmSel >= 0)
    Core.Info.PathMode = (NWildcard::ECensorPathMode)k_PathMode_Vals[pmSel];

  Core.EncryptHeadersChecked = IsButtonCheckedBool(IDX_COMPRESS_ENCRYPT_FILE_NAMES);
  Core.OpenShareForWrite = IsButtonCheckedBool(IDX_COMPRESS_SHARED);
  Core.DeleteAfterCompressing = IsButtonCheckedBool(IDX_COMPRESS_DEL);

  UString errorMessage;
  CCompressDialogCore::ECommitResult res = Core.ValidateAndCommit(errorMessage);
  if (res == CCompressDialogCore::kCommitBlocked)
  {
    if (!errorMessage.IsEmpty())
      MessageBoxError(errorMessage);
    return;
  }
  if (res == CCompressDialogCore::kCommitNeedVolumeConfirm)
  {
    if (::MessageBoxW(*this, Core.VolumeConfirmText,
        // **************** NanaZip Modification Start ****************
        //L"7-Zip", MB_YESNOCANCEL | MB_ICONQUESTION) != IDYES)
        L"NanaZip", MB_YESNOCANCEL | MB_ICONQUESTION) != IDYES)
        // **************** NanaZip Modification End ****************
      return;
    Core.VolumeConfirmed = true;
    res = Core.ValidateAndCommit(errorMessage);
    if (res == CCompressDialogCore::kCommitBlocked)
    {
      if (!errorMessage.IsEmpty())
        MessageBoxError(errorMessage);
      return;
    }
  }

  CModalDialog::OnOK();
}


// ---------- OPTIONS ----------


void COptionsDialog::CheckButton_Bool1(UINT id, const CBool1 &b1)
{
  CheckButton(id, b1.Val);
}

void COptionsDialog::GetButton_Bool1(UINT id, CBool1 &b1)
{
  b1.Val = IsButtonCheckedBool(id);
}


void COptionsDialog::CheckButton_BoolBox(
    bool supported, const CBoolPair &b2, CBoolBox &bb)
{
  const bool isSet = b2.Def;
  const bool val = isSet ? b2.Val : bb.DefaultVal;

  bb.IsSupported = supported;

  CheckButton (bb.Set_Id, isSet);
  ShowItem_Bool (bb.Set_Id, supported);
  CheckButton (bb.Id, val);
  EnableItem (bb.Id, isSet);
  ShowItem_Bool (bb.Id, supported);
}

void COptionsDialog::GetButton_BoolBox(CBoolBox &bb)
{
  // we save value for invisible buttons too
  bb.BoolPair.Val = IsButtonCheckedBool (bb.Id);
  bb.BoolPair.Def = IsButtonCheckedBool (bb.Set_Id);
}


void COptionsDialog::Store_TimeBoxes()
{
  TimePrec = GetPrecSpec();
  GetButton_BoolBox (MTime);
  GetButton_BoolBox (CTime);
  GetButton_BoolBox (ATime);
  GetButton_BoolBox (ZTime);
}


UInt32 COptionsDialog::GetComboValue(NWindows::NControl::CComboBox &c, int defMax)
{
  if (c.GetCount() <= defMax)
    return (UInt32)(Int32)-1;
  return (UInt32)c.GetItemData_of_CurSel();
}

static const unsigned kTimePrec_Win  = 0;
static const unsigned kTimePrec_Unix = 1;
static const unsigned kTimePrec_DOS  = 2;
static const unsigned kTimePrec_1ns  = 3;

static void AddTimeOption(UString &s, UInt32 val, const UString &unit, const char *sys = NULL)
{
  // s += " : ";
  s.Add_UInt32(val);
  s.Add_Space();
  s += unit;
  if (sys)
  {
    s += " : ";
    s += sys;
  }
}

int COptionsDialog::AddPrec(unsigned prec, bool isDefault)
{
  UString s;
  UInt32 writePrec = prec;
  if (isDefault)
  {
    // s += "* ";
    // writePrec = (UInt32)(Int32)-1;
  }
       if (prec == kTimePrec_Win)  AddTimeOption(s, 100, NsString, "Windows");
  else if (prec == kTimePrec_Unix) AddTimeOption(s, 1, SecString, "Unix");
  else if (prec == kTimePrec_DOS)  AddTimeOption(s, 2, SecString, "DOS");
  else if (prec == kTimePrec_1ns)  AddTimeOption(s, 1, NsString, "Linux");
  else if (prec == k_PropVar_TimePrec_Base) AddTimeOption(s, 1, SecString);
  else if (prec >= k_PropVar_TimePrec_Base)
  {
    UInt32 d = 1;
    for (unsigned i = prec; i < k_PropVar_TimePrec_Base + 9; i++)
      d *= 10;
    AddTimeOption(s, d, NsString);
  }
  else
    s.Add_UInt32(prec);
  return (int)m_Prec.AddString_SetItemData(s, (LPARAM)writePrec);
}


void COptionsDialog::SetPrec()
{
  const CArcInfoEx &ai = cd->Get_ArcInfoEx();

  UInt32 flags = ai.Get_TimePrecFlags();
  UInt32 defaultPrec = ai.Get_DefaultTimePrec();
  if (defaultPrec != 0)
    flags |= ((UInt32)1 << defaultPrec);

  if (ai.Is_GZip())
    defaultPrec = kTimePrec_Unix;

  {
    UString s;
    s += GetNameOfProperty(kpidType, L"type");
    s += ": ";
    s += ai.Name;
    if (ai.Is_Tar())
    {
      const int methodID = cd->GetMethodID();
      UString estimatedName;
      cd->GetMethodSpec(estimatedName);

      s.Add_Colon();
      if (methodID >= 0 && !estimatedName.IsEmpty())
        s += estimatedName;
    }
    else
    {
      // if (is_for_MethodChanging) return;
    }

    SetItemText(IDT_COMPRESS_TIME_INFO, s);
  }

  m_Prec.ResetContent();
  _auto_Prec = defaultPrec;

  unsigned selectedPrec = defaultPrec;
  {
    // if (TimePrec >= kTimePrec_Win && TimePrec <= kTimePrec_DOS)
    if ((Int32)TimePrec >= 0)
      selectedPrec = TimePrec;
  }

  int curSel = -1;
  int defaultPrecIndex = -1;
  for (unsigned prec = 0;
      // prec <= k_PropVar_TimePrec_HighPrec;
      prec <= k_PropVar_TimePrec_1ns;
      prec++)
  {
    if (((flags >> prec) & 1) == 0)
      continue;
    const bool isDefault = (defaultPrec == prec);
    const int index = AddPrec(prec, isDefault);
    if (isDefault)
      defaultPrecIndex = index;
    if (selectedPrec == prec)
      curSel = index;
  }

  if (curSel < 0 && selectedPrec > kTimePrec_DOS)
    curSel = AddPrec(selectedPrec, false); // isDefault
  if (curSel < 0)
    curSel = defaultPrecIndex;
  if (curSel >= 0)
    m_Prec.SetCurSel(curSel);

  {
    const bool isSet = IsSet_TimePrec();
    const int count = m_Prec.GetCount();
    const bool showPrec = (count != 0);
    ShowItem_Bool(IDC_COMPRESS_TIME_PREC, showPrec);
    ShowItem_Bool(IDT_COMPRESS_TIME_PREC, showPrec);
    EnableItem(IDC_COMPRESS_TIME_PREC, isSet && (count > 1));

    CheckButton(IDX_COMPRESS_PREC_SET, isSet);
    const bool setIsSupported = isSet || (count > 1);
    EnableItem(IDX_COMPRESS_PREC_SET, setIsSupported);
    ShowItem_Bool(IDX_COMPRESS_PREC_SET, setIsSupported);
  }

  SetTimeMAC();
}


void COptionsDialog::SetTimeMAC()
{
  const CArcInfoEx &ai = cd->Get_ArcInfoEx();

  const
  bool m_allow = ai.Flags_MTime();
  bool c_allow = ai.Flags_CTime();
  bool a_allow = ai.Flags_ATime();

  if (ai.Is_Tar())
  {
    const int methodID = cd->GetMethodID();
    c_allow = false;
    a_allow = false;
    if (methodID == kPosix)
    {
      // c_allow = true; // do we need it as change time ?
      a_allow = true;
    }
  }

  if (ai.Is_Zip())
  {
    UInt32 prec = GetPrec();
    if (prec == (UInt32)(Int32)-1)
      prec = _auto_Prec;
    if (prec != kTimePrec_Win)
    {
      c_allow = false;
      a_allow = false;
    }
  }

  MTime.DefaultVal = ai.Flags_MTime_Default();
  CTime.DefaultVal = ai.Flags_CTime_Default();
  ATime.DefaultVal = ai.Flags_ATime_Default();

  ZTime.DefaultVal = false;

  const NCompression::CFormatOptions &fo = cd->Get_FormatOptions();

  CheckButton_BoolBox (m_allow, fo.MTime, MTime );
  CheckButton_BoolBox (c_allow, fo.CTime, CTime );
  CheckButton_BoolBox (a_allow, fo.ATime, ATime );
  CheckButton_BoolBox (true, fo.SetArcMTime, ZTime);

  if (m_allow && !fo.MTime.Def)
  {
    const bool isSingleFile = ai.Flags_KeepName();
    if (!isSingleFile)
    {
      // we can hide changing checkboxes for MTime here:
      ShowItem_Bool (MTime.Set_Id, false);
      EnableItem (MTime.Id, false);
    }
  }
  // On_CheckBoxSet_Prec_Clicked();
  // const bool isSingleFile = ai.Flags_KeepName();
  // mtime for Gz can be
}



void COptionsDialog::On_CheckBoxSet_Prec_Clicked()
{
  const bool isSet = IsButtonCheckedBool(IDX_COMPRESS_PREC_SET);
  if (!isSet)
  {
    // We save current MAC boxes to memory before SetPrec()
    Store_TimeBoxes();
    Reset_TimePrec();
    SetPrec();
  }
  EnableItem(IDC_COMPRESS_TIME_PREC, isSet);
}

void COptionsDialog::On_CheckBoxSet_Clicked(const CBoolBox &bb)
{
  const bool isSet = IsButtonCheckedBool(bb.Set_Id);
  if (!isSet)
    CheckButton(bb.Id, bb.DefaultVal);
  EnableItem(bb.Id, isSet);
}




#ifdef Z7_LANG
static const UInt32 kLangIDs_Options[] =
{
  IDX_COMPRESS_NT_SYM_LINKS,
  IDX_COMPRESS_NT_HARD_LINKS,
  IDX_COMPRESS_NT_ALT_STREAMS,
  IDX_COMPRESS_NT_SECUR,

  IDG_COMPRESS_TIME,
  IDT_COMPRESS_TIME_PREC,
  IDX_COMPRESS_MTIME,
  IDX_COMPRESS_CTIME,
  IDX_COMPRESS_ATIME,
  IDX_COMPRESS_ZTIME,
  IDX_COMPRESS_PRESERVE_ATIME
};
#endif


bool COptionsDialog::OnInit()
{
  #ifdef Z7_LANG
  LangSetWindowText(*this, IDB_COMPRESS_OPTIONS); // IDS_OPTIONS
  LangSetDlgItems(*this, kLangIDs_Options, Z7_ARRAY_SIZE(kLangIDs_Options));
  #endif

  LangString(IDS_COMPRESS_SEC, SecString);
  if (SecString.IsEmpty())
    SecString = "sec";
  LangString(IDS_COMPRESS_NS, NsString);
  if (NsString.IsEmpty())
    NsString = "ns";

  {
    ShowItem_Bool ( IDX_COMPRESS_NT_SYM_LINKS,    cd->SymLinks.Supported);
    ShowItem_Bool ( IDX_COMPRESS_NT_HARD_LINKS,   cd->HardLinks.Supported);
    ShowItem_Bool ( IDX_COMPRESS_NT_ALT_STREAMS,  cd->AltStreams.Supported);
    ShowItem_Bool ( IDX_COMPRESS_NT_SECUR,        cd->NtSecurity.Supported);

    ShowItem_Bool ( IDG_COMPRESS_NTFS,
           cd->SymLinks.Supported
        || cd->HardLinks.Supported
        || cd->AltStreams.Supported
        || cd->NtSecurity.Supported);
  }

   /* we read property from two sources:
       1) command line  : (Info)
       2) registry      : (RegistryInfo)
     (Info) has priority, if both are no defined */

  CheckButton_Bool1 ( IDX_COMPRESS_NT_SYM_LINKS,   cd->SymLinks);
  CheckButton_Bool1 ( IDX_COMPRESS_NT_HARD_LINKS,  cd->HardLinks);
  CheckButton_Bool1 ( IDX_COMPRESS_NT_ALT_STREAMS, cd->AltStreams);
  CheckButton_Bool1 ( IDX_COMPRESS_NT_SECUR,       cd->NtSecurity);

  CheckButton_Bool1 (IDX_COMPRESS_PRESERVE_ATIME, cd->PreserveATime);

  m_Prec.Attach (GetItem(IDC_COMPRESS_TIME_PREC));

  MTime.SetIDs ( IDX_COMPRESS_MTIME, IDX_COMPRESS_MTIME_SET);
  CTime.SetIDs ( IDX_COMPRESS_CTIME, IDX_COMPRESS_CTIME_SET);
  ATime.SetIDs ( IDX_COMPRESS_ATIME, IDX_COMPRESS_ATIME_SET);
  ZTime.SetIDs ( IDX_COMPRESS_ZTIME, IDX_COMPRESS_ZTIME_SET);

  {
    const NCompression::CFormatOptions &fo = cd->Get_FormatOptions();
    TimePrec = fo.TimePrec;
    MTime.BoolPair = fo.MTime;
    CTime.BoolPair = fo.CTime;
    ATime.BoolPair = fo.ATime;
    ZTime.BoolPair = fo.SetArcMTime;
  }

  SetPrec();

  NormalizePosition();

  return CModalDialog::OnInit();
}


bool COptionsDialog::OnCommand(unsigned code, unsigned itemID, LPARAM lParam)
{
  if (code == CBN_SELCHANGE)
  {
    switch (itemID)
    {
      case IDC_COMPRESS_TIME_PREC:
      {
        Store_TimeBoxes();
        SetTimeMAC(); // for zip/tar
        return true;
      }
    }
  }
  return CModalDialog::OnCommand(code, itemID, lParam);
}


bool COptionsDialog::OnButtonClicked(unsigned buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDX_COMPRESS_PREC_SET:  { On_CheckBoxSet_Prec_Clicked(); return true; }
    case IDX_COMPRESS_MTIME_SET: { On_CheckBoxSet_Clicked (MTime); return true; }
    case IDX_COMPRESS_CTIME_SET: { On_CheckBoxSet_Clicked (CTime); return true; }
    case IDX_COMPRESS_ATIME_SET: { On_CheckBoxSet_Clicked (ATime); return true; }
    case IDX_COMPRESS_ZTIME_SET: { On_CheckBoxSet_Clicked (ZTime); return true; }
  }
  return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
}


void COptionsDialog::OnOK()
{
  GetButton_Bool1 (IDX_COMPRESS_NT_SYM_LINKS,   cd->SymLinks);
  GetButton_Bool1 (IDX_COMPRESS_NT_HARD_LINKS,  cd->HardLinks);
  GetButton_Bool1 (IDX_COMPRESS_NT_ALT_STREAMS, cd->AltStreams);
  GetButton_Bool1 (IDX_COMPRESS_NT_SECUR,       cd->NtSecurity);
  GetButton_Bool1 (IDX_COMPRESS_PRESERVE_ATIME, cd->PreserveATime);

  Store_TimeBoxes();
  {
    NCompression::CFormatOptions &fo = cd->Get_FormatOptions();
    fo.TimePrec = TimePrec;
    fo.MTime = MTime.BoolPair;
    fo.CTime = CTime.BoolPair;
    fo.ATime = ATime.BoolPair;
    fo.SetArcMTime = ZTime.BoolPair;
  }

  CModalDialog::OnOK();
}
