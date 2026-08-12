// CompressDialog.h

#ifndef ZIP7_INC_COMPRESS_DIALOG_H
#define ZIP7_INC_COMPRESS_DIALOG_H

#include "CompressDialogCore.h"

#include "../../../Windows/Control/ComboBox.h"
#include "../../../Windows/Control/Edit.h"

#include "../FileManager/DialogSize.h"

#include "CompressDialogRes.h"

// CCompressDialog (the Win32 compression dialog shell) was removed: the XAML
// CompressPage is the only dialog path now, and the shell was never
// triggered. COptionsDialog below is still used by the XAML path
// (the Options button opens this Win32 modal dialog).

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

public:

  INT_PTR Create(HWND wndParent = NULL)
  {
    BIG_DIALOG_SIZE(240, 232);
    return CModalDialog::Create(SIZED_DIALOG(IDD_COMPRESS_OPTIONS), wndParent);
  }

  COptionsDialog(CCompressDialogCore *cdLoc):
      cd(cdLoc)
      {
        Reset_TimePrec();
      }
};

#endif
