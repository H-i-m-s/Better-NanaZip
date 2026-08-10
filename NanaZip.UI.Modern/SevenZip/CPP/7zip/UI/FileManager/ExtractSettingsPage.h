// ExtractSettingsPage.h

#ifndef __EXTRACT_SETTINGS_PAGE_H
#define __EXTRACT_SETTINGS_PAGE_H

#include "../../../Windows/Control/PropertyPage.h"

// **************** SSS Modification Start ****************
// "Extraction Settings" property page: delete-archive-after-extraction
// switches, password auto-match switches, cloud API config and the local
// password book, moved out of the main settings page into their own tab.
// **************** SSS Modification End ****************

class CExtractSettingsPage: public NWindows::NControl::CPropertyPage
{
  bool _wasChanged;
  bool _apiChanged;   // API 配置六行被用户改过（懒创建：没动过且无文件则不写）
  bool _bookLoading;  // OnInit 填充密码本时抑制 EN_CHANGE 写盘
  bool _apiLoading;   // OnInit 填充 API 六行时抑制 EN_CHANGE 置位

  bool OnButtonClicked(int buttonID, HWND buttonHWND);
  bool OnCommand(int code, int itemID, LPARAM param);

  void LoadBookToEdit();
  void SaveBookFromEdit();   // 读取 Edit → 过滤空行 → 写盘（输入即保存）
  void DoImportBook();

  void LoadApiToEdits();
  void ApplyApiFromEdits();  // OnApply 时写 api_config.txt（懒创建）

  virtual bool OnInit();
  virtual LONG OnApply();
public:
};

#endif
