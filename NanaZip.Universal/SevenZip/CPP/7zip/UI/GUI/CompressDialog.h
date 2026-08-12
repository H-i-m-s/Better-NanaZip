// CompressDialog.h

#ifndef ZIP7_INC_COMPRESS_DIALOG_H
#define ZIP7_INC_COMPRESS_DIALOG_H

#include "CompressDialogCore.h"

#include "../../../Windows/Control/ComboBox.h"
#include "../../../Windows/Control/Edit.h"

#include "../FileManager/DialogSize.h"

#include "CompressDialogRes.h"

class CCompressDialog: public NWindows::NControl::CModalDialog
{
public:
  // 规则层：全部数据与规则都在 Core，壳只负责控件显示与交互。
  CCompressDialogCore Core;

  // UpdateGUI 等外部代码使用的公开接口（绑定到 Core 的数据）
  NCompressDialog::CInfo &Info;
  const CObjectVector<CArcInfoEx> *&ArcFormats;
  CUIntVector &ArcIndices; // can not be empty, must contain Info.FormatIndex, if Info.FormatIndex >= 0
  AStringVector &ExternalMethods;
  UString &OriginalFileName; // for bzip2, gzip2

  void SetMethods(const CObjectVector<CCodecInfoUser> &userCodecs)
  {
    Core.SetMethods(userCodecs);
  }

  INT_PTR Create(HWND wndParent = NULL)
  {
    BIG_DIALOG_SIZE(400, 320);
    return CModalDialog::Create(SIZED_DIALOG(IDD_COMPRESS), wndParent);
  }

  CCompressDialog():
      Info(Core.Info),
      ArcFormats(Core.ArcFormats),
      ArcIndices(Core.ArcIndices),
      ExternalMethods(Core.ExternalMethods),
      OriginalFileName(Core.OriginalFileName)
      {}

private:
  NWindows::NControl::CComboBox m_ArchivePath;
  NWindows::NControl::CComboBox m_Format;
  NWindows::NControl::CComboBox m_Level;
  NWindows::NControl::CComboBox m_Method;
  NWindows::NControl::CComboBox m_Dictionary;
  // NWindows::NControl::CComboBox m_Dictionary_Chain;
  NWindows::NControl::CComboBox m_Order;
  NWindows::NControl::CComboBox m_Solid;
  NWindows::NControl::CComboBox m_NumThreads;
  NWindows::NControl::CComboBox m_MemUse;
  NWindows::NControl::CComboBox m_Volume;

  int _dictionaryCombo_left;

  NWindows::NControl::CDialogChildControl m_Params;

  NWindows::NControl::CComboBox m_UpdateMode;
  NWindows::NControl::CComboBox m_PathMode;
  
  NWindows::NControl::CEdit _password1Control;
  NWindows::NControl::CEdit _password2Control;
  NWindows::NControl::CComboBox _encryptionMethod;

  // ---- Sync：Core 状态/列表 → 控件 ----

  void SyncFormat();
  void SyncLevel();
  void SyncMethod();
  void SyncDictionary();
  void SyncOrder();
  void SyncSolid();
  void SyncThreads();
  void SyncMemUse();
  void SyncEncryptionMethod();
  void SyncMemoryUsage();
  void SyncOptionsSummary();
  void SyncParams();
  void SyncArcPathFields();
  void SyncHardwareThreads();
  void SyncBoolChecks();

  void SetCurSelByValue(NWindows::NControl::CComboBox &combo,
      const CObjectVector<CCompressDialogCore::COptionItem> &items, UInt64 value);

  // ---- 控件操作与事件 ----

  void UpdatePasswordControl();
  void EnableMultiCombo(unsigned id);
  void CheckSFXControlsEnable();
  void CheckSFXNameChange();
  void FormatChanged(bool isChanged);
  void FormatChangedControls();
  void CollectTexts();
  void SetArchiveName(const UString &name);
  void SetArchiveName2(bool prevWasSFX);
  void ArcPath_WasChanged(const UString &path);
  void OnButtonSetArchive();
  bool SetArcPathFields(const UString &path, UString &name, bool always);
  bool SetArcPathFields(const UString &path);
  bool GetFinalPath_Smart(UString &resPath) const;
  void ShowOptionsString();
  void SetParams();
  void SaveOptionsInMem();
  bool IsSFX() { return Core.IsSfx(); }

  bool IsShowPasswordChecked() const { return IsButtonCheckedBool(IDX_PASSWORD_SHOW); }

  virtual bool OnInit() Z7_override;
  virtual bool OnMessage(UINT message, WPARAM wParam, LPARAM lParam) Z7_override;
  virtual bool OnCommand(unsigned code, unsigned itemID, LPARAM lParam) Z7_override;
  virtual bool OnButtonClicked(unsigned buttonID, HWND buttonHWND) Z7_override;
  virtual void OnOK() Z7_override;
  // **************** NanaZip Modification Start ****************
  //virtual void OnHelp() Z7_override;
  // **************** NanaZip Modification End ****************

  void MessageBoxError(LPCWSTR message)
  {
    // **************** NanaZip Modification Start ****************
    //MessageBoxW(*this, message, L"7-Zip", MB_ICONERROR);
    MessageBoxW(*this, message, L"NanaZip", MB_ICONERROR);
    // **************** NanaZip Modification End ****************
  }
};




class COptionsDialog: public NWindows::NControl::CModalDialog
{
  struct CBoolBox
  {
    bool IsSupported;
    bool DefaultVal;
    CBoolPair BoolPair;
    
    unsigned Id;
    unsigned Set_Id;

    void SetIDs(unsigned id, unsigned set_Id)
    {
      Id = id;
      Set_Id = set_Id;
    }

    CBoolBox():
        IsSupported(false),
        DefaultVal(false)
        {}
  };

  CCompressDialogCore *cd;

  NWindows::NControl::CComboBox m_Prec;

  UInt32 _auto_Prec;
  UInt32 TimePrec;

  void Reset_TimePrec() { TimePrec = (UInt32)(Int32)-1; }
  bool IsSet_TimePrec() const { return TimePrec != (UInt32)(Int32)-1; }

  CBoolBox MTime;
  CBoolBox CTime;
  CBoolBox ATime;
  CBoolBox ZTime;

  UString SecString;
  UString NsString;


  void CheckButton_Bool1(UINT id, const CBool1 &b1);
  void GetButton_Bool1(UINT id, CBool1 &b1);
  void CheckButton_BoolBox(bool supported, const CBoolPair &b2, CBoolBox &bb);
  void GetButton_BoolBox(CBoolBox &bb);

  void Store_TimeBoxes();

  UInt32 GetComboValue(NWindows::NControl::CComboBox &c, int defMax = 0);
  UInt32 GetPrecSpec()
  {
    UInt32 prec = GetComboValue(m_Prec, 1);
    if (prec == _auto_Prec)
      prec = (UInt32)(Int32)-1;
    return prec;
  }
  UInt32 GetPrec() { return GetComboValue(m_Prec, 0); }

  int AddPrec(unsigned prec, bool isDefault);
  void SetPrec();
  void SetTimeMAC();

  void On_CheckBoxSet_Prec_Clicked();
  void On_CheckBoxSet_Clicked(const CBoolBox &bb);

  virtual bool OnInit() Z7_override;
  virtual bool OnCommand(unsigned code, unsigned itemID, LPARAM lParam) Z7_override;
  virtual bool OnButtonClicked(unsigned buttonID, HWND buttonHWND) Z7_override;
  virtual void OnOK() Z7_override;
  // **************** NanaZip Modification Start ****************
  //virtual void OnHelp() Z7_override;
  // **************** NanaZip Modification End ****************

public:

  INT_PTR Create(HWND wndParent = NULL)
  {
    BIG_DIALOG_SIZE(240, 232);
    return CModalDialog::Create(SIZED_DIALOG(IDD_COMPRESS_OPTIONS), wndParent);
  }

  COptionsDialog(CCompressDialogCore *cdLoc):
      cd(cdLoc)
      // , TimePrec(0)
      {
        Reset_TimePrec();
      }
};

#endif
