// ExtractSettingsPage.cpp

#include "StdAfx.h"

#include "ExtractSettingsPage.h"

#include "../../../Windows/Control/Dialog.h"

#include "LangUtils.h"
#include "RegistryUtils.h"
#include "SettingsPageRes.h"

using namespace NWindows;

// **************** SSS Modification Start ****************
static const UInt32 kLangIDs[] =
{
  IDX_SETTINGS_DELETE_AFTER_EXTRACT,
  IDX_SETTINGS_DELETE_PERMANENTLY
};

bool CExtractSettingsPage::OnInit()
{
  _wasChanged = false;

  LangSetDlgItems(*this, kLangIDs, ARRAY_SIZE(kLangIDs));

  CFmSettings st;
  st.Load();

  CheckButton(IDX_SETTINGS_DELETE_AFTER_EXTRACT, st.DeleteAfterExtract);
  CheckButton(IDX_SETTINGS_DELETE_PERMANENTLY, st.DeletePermanently);
  return true;
}

LONG CExtractSettingsPage::OnApply()
{
  if (_wasChanged)
  {
    CFmSettings st;
    st.Load();
    st.DeleteAfterExtract = IsButtonCheckedBool(IDX_SETTINGS_DELETE_AFTER_EXTRACT);
    st.DeletePermanently = IsButtonCheckedBool(IDX_SETTINGS_DELETE_PERMANENTLY);
    st.Save();
    _wasChanged = false;
  }
  return PSNRET_NOERROR;
}

bool CExtractSettingsPage::OnButtonClicked(int buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDX_SETTINGS_DELETE_AFTER_EXTRACT:
    case IDX_SETTINGS_DELETE_PERMANENTLY:
      _wasChanged = true;
      break;
    default:
      return CPropertyPage::OnButtonClicked(buttonID, buttonHWND);
  }
  return true;
}
// **************** SSS Modification End ****************
