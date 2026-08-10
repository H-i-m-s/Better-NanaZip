// ExtractSettingsPage.cpp

#include "StdAfx.h"

#include <commdlg.h>

#include "ExtractSettingsPage.h"

#include "../../../Windows/Control/Dialog.h"
#include "../../../Windows/Control/Edit.h"
#include "../../../Windows/Control/ComboBox.h"

#include "LangUtils.h"
#include "RegistryUtils.h"
#include "SssPasswordFile.h"
#include "SettingsPageRes.h"

using namespace NWindows;
using namespace NWindows::NControl;

// **************** SSS Modification Start ****************
static const UInt32 kLangIDs[] =
{
  IDX_SETTINGS_DELETE_AFTER_EXTRACT,
  IDX_SETTINGS_DELETE_PERMANENTLY,
  // --- 自动匹配组 ---
  IDX_SETTINGS_AUTO_QUERY_CLOUD,
  IDT_SETTINGS_API_URL,
  IDT_SETTINGS_API_APP_ID,
  IDT_SETTINGS_API_AES_KEY,
  IDT_SETTINGS_API_SIGNING_KEY,
  IDT_SETTINGS_API_PACKAGE_NAME,
  IDT_SETTINGS_API_FINGERPRINT,
  IDX_SETTINGS_AUTO_MATCH_LOCAL,
  IDT_SETTINGS_MATCH_PRIORITY,
  IDX_SETTINGS_AUTO_SHOW_PASSWORD,
  // --- 密码本组 ---
  IDT_SETTINGS_GROUP_BOOK,
  IDX_SETTINGS_IMPORT_BOOK
};

// 密码本 Edit 子类化：空行回车 = 点击「确定」（关闭并应用设置）
static WNDPROC g_BookEditProc = NULL;

static LRESULT CALLBACK BookEditWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (msg == WM_KEYDOWN && wParam == VK_RETURN)
  {
    DWORD sel = (DWORD)SendMessage(hwnd, EM_GETSEL, 0, 0);
    int caret = (int)LOWORD(sel);
    int line = (int)SendMessage(hwnd, EM_LINEFROMCHAR, caret, 0);
    int lineStart = (int)SendMessage(hwnd, EM_LINEINDEX, line, 0);
    int lineLen = (int)SendMessage(hwnd, EM_LINELENGTH, lineStart, 0);
    if (lineLen == 0)
    {
      // 空行回车 → 确定（属性表收到 PSM_PRESSBUTTON 后关闭并应用）
      HWND sheet = GetParent(GetParent(hwnd));
      SendMessage(sheet, PSM_PRESSBUTTON, PSBTN_OK, 0);
      return 0;
    }
  }
  return CallWindowProc(g_BookEditProc, hwnd, msg, wParam, lParam);
}

bool CExtractSettingsPage::OnInit()
{
  _wasChanged = false;
  _apiChanged = false;
  _bookLoading = true;
  _apiLoading = true;

  LangSetDlgItems(*this, kLangIDs, ARRAY_SIZE(kLangIDs));

  CFmSettings st;
  st.Load();

  CheckButton(IDX_SETTINGS_DELETE_AFTER_EXTRACT, st.DeleteAfterExtract);
  CheckButton(IDX_SETTINGS_DELETE_PERMANENTLY, st.DeletePermanently);
  CheckButton(IDX_SETTINGS_AUTO_QUERY_CLOUD, st.AutoQueryCloud);
  CheckButton(IDX_SETTINGS_AUTO_MATCH_LOCAL, st.AutoMatchLocal);
  CheckButton(IDX_SETTINGS_AUTO_SHOW_PASSWORD, st.AutoShowPassword);

  CComboBox priority;
  priority.Attach(GetItem(IDX_SETTINGS_MATCH_PRIORITY));
  priority.ResetContent();
  priority.AddString(LangString(IDX_SETTINGS_PRIORITY_LOCAL_CLOUD));
  priority.AddString(LangString(IDX_SETTINGS_PRIORITY_CLOUD_LOCAL));
  priority.SetCurSel(st.MatchPriority != 0 ? 1 : 0);

  LoadBookToEdit();
  LoadApiToEdits();

  HWND bookEdit = GetItem(IDX_SETTINGS_PASSWORD_BOOK);
  if (bookEdit)
    g_BookEditProc = (WNDPROC)SetWindowLongPtr(bookEdit, GWLP_WNDPROC, (LONG_PTR)BookEditWindowProc);

  _bookLoading = false;
  _apiLoading = false;
  return true;
}

void CExtractSettingsPage::LoadBookToEdit()
{
  SssPasswordBook book;
  UString text;
  if (SssLoadPasswordBook(book))
  {
    for (unsigned i = 0; i < book.lines.Size(); i++)
    {
      if (i != 0)
        text += L"\r\n";
      text += book.lines[i];
    }
  }
  SetItemText(IDX_SETTINGS_PASSWORD_BOOK, text);
}

void CExtractSettingsPage::SaveBookFromEdit()
{
  UString text;
  GetItemText(IDX_SETTINGS_PASSWORD_BOOK, text);
  UStringVector lines;
  SssSplitTextToLines(text, lines);
  // 过滤空行（保留注释行与密码行）
  UStringVector clean;
  for (unsigned i = 0; i < lines.Size(); i++)
    if (!lines[i].IsEmpty())
      clean.Add(lines[i]);
  SssSavePasswordBook(clean);
}

void CExtractSettingsPage::DoImportBook()
{
  wchar_t buf[MAX_PATH * 4] = { 0 };
  OPENFILENAMEW ofn = { 0 };
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = (HWND)*this;
  ofn.lpstrFilter = L"Text files (*.txt)\0*.txt\0All files (*.*)\0*.*\0";
  ofn.lpstrFile = buf;
  ofn.nMaxFile = ARRAY_SIZE(buf);
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
  if (!GetOpenFileNameW(&ofn))
    return;

  UString text;
  if (!SssReadFileUtf8((const wchar_t *)ofn.lpstrFile, text))
  {
    MessageBoxW((HWND)*this, L"Failed to read the selected file.", L"NanaZip", MB_OK | MB_ICONWARNING);
    return;
  }
  // 替换当前密码本内容；EN_CHANGE 会自动触发 SaveBookFromEdit 落盘
  SetItemText(IDX_SETTINGS_PASSWORD_BOOK, text);
}

void CExtractSettingsPage::LoadApiToEdits()
{
  SssApiConfig cfg;
  SssLoadApiConfig(cfg);
  SetItemText(IDX_SETTINGS_API_URL, cfg.Url);
  SetItemText(IDX_SETTINGS_API_APP_ID, cfg.AppId);
  SetItemText(IDX_SETTINGS_API_AES_KEY, cfg.AesKey);
  SetItemText(IDX_SETTINGS_API_SIGNING_KEY, cfg.SigningKey);
  SetItemText(IDX_SETTINGS_API_PACKAGE_NAME, cfg.PackageName);
  SetItemText(IDX_SETTINGS_API_FINGERPRINT, cfg.Fingerprint);
}

void CExtractSettingsPage::ApplyApiFromEdits()
{
  if (!_apiChanged)
    return; // 用户没动过 API 配置 → 不创建文件（懒创建）
  SssApiConfig cfg;
  GetItemText(IDX_SETTINGS_API_URL, cfg.Url);
  GetItemText(IDX_SETTINGS_API_APP_ID, cfg.AppId);
  GetItemText(IDX_SETTINGS_API_AES_KEY, cfg.AesKey);
  GetItemText(IDX_SETTINGS_API_SIGNING_KEY, cfg.SigningKey);
  GetItemText(IDX_SETTINGS_API_PACKAGE_NAME, cfg.PackageName);
  GetItemText(IDX_SETTINGS_API_FINGERPRINT, cfg.Fingerprint);
  SssSaveApiConfig(cfg);
}

LONG CExtractSettingsPage::OnApply()
{
  if (_wasChanged)
  {
    CFmSettings st;
    st.Load();
    st.DeleteAfterExtract = IsButtonCheckedBool(IDX_SETTINGS_DELETE_AFTER_EXTRACT);
    st.DeletePermanently = IsButtonCheckedBool(IDX_SETTINGS_DELETE_PERMANENTLY);
    st.AutoQueryCloud = IsButtonCheckedBool(IDX_SETTINGS_AUTO_QUERY_CLOUD);
    st.AutoMatchLocal = IsButtonCheckedBool(IDX_SETTINGS_AUTO_MATCH_LOCAL);
    {
      CComboBox priority;
      priority.Attach(GetItem(IDX_SETTINGS_MATCH_PRIORITY));
      st.MatchPriority = (priority.GetCurSel() == 1) ? 1 : 0;
    }
    st.AutoShowPassword = IsButtonCheckedBool(IDX_SETTINGS_AUTO_SHOW_PASSWORD);
    st.Save();
    _wasChanged = false;
  }
  ApplyApiFromEdits();
  return PSNRET_NOERROR;
}

bool CExtractSettingsPage::OnCommand(int code, int itemID, LPARAM param)
{
  switch (code)
  {
    case EN_CHANGE:
      if (itemID == IDX_SETTINGS_PASSWORD_BOOK)
      {
        if (!_bookLoading)
        {
          SaveBookFromEdit();
          Changed();
        }
        return true;
      }
      if (itemID >= IDX_SETTINGS_API_URL && itemID <= IDX_SETTINGS_API_FINGERPRINT)
      {
        if (!_apiLoading)
        {
          _apiChanged = true;
          Changed();
        }
        return true;
      }
      break;
    case CBN_SELCHANGE:
      if (itemID == IDX_SETTINGS_MATCH_PRIORITY)
      {
        _wasChanged = true;
        Changed();
        return true;
      }
      break;
  }
  return CPropertyPage::OnCommand(code, itemID, param);
}

bool CExtractSettingsPage::OnButtonClicked(int buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDX_SETTINGS_DELETE_AFTER_EXTRACT:
    case IDX_SETTINGS_DELETE_PERMANENTLY:
    case IDX_SETTINGS_AUTO_QUERY_CLOUD:
    case IDX_SETTINGS_AUTO_MATCH_LOCAL:
    case IDX_SETTINGS_AUTO_SHOW_PASSWORD:
      _wasChanged = true;
      Changed();
      break;
    case IDX_SETTINGS_IMPORT_BOOK:
      DoImportBook();
      break;
    default:
      return CPropertyPage::OnButtonClicked(buttonID, buttonHWND);
  }
  return true;
}
// **************** SSS Modification End ****************
