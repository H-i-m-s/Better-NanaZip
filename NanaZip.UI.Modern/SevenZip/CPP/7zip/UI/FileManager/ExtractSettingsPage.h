// ExtractSettingsPage.h

#ifndef __EXTRACT_SETTINGS_PAGE_H
#define __EXTRACT_SETTINGS_PAGE_H

#include "../../../Windows/Control/PropertyPage.h"

// **************** SSS Modification Start ****************
// "Extraction Settings" property page: delete-archive-after-extraction
// switches, moved out of the main settings page into their own tab.
// **************** SSS Modification End ****************

class CExtractSettingsPage: public NWindows::NControl::CPropertyPage
{
  bool _wasChanged;
  bool OnButtonClicked(int buttonID, HWND buttonHWND);
  virtual bool OnInit();
  virtual LONG OnApply();
public:
};

#endif
