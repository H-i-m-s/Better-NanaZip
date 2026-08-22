// OpenCallback.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariant.h"

#include "../../Common/FileStreams.h"

#include "../Common/ZipRegistry.h"

#include <NanaZip.Password.h>

#include "OpenCallback.h"
#include "PasswordDialog.h"
#include "NanaZip.Modern.h"

using namespace NWindows;

// Adds the current password to the local password book (see
// NanaZip.Password). Filled into the password dialog context as
// AddPasswordCallback; the XAML page calls it from the "+" button next to
// the password box.
static BOOLEAN WINAPI SssAddPasswordToBookCallback(LPCWSTR password)
{
  if (!password)
    return FALSE;
  return NanaZipPassword::AddPasswordToBook(password) ? TRUE : FALSE;
}

HRESULT COpenArchiveCallback::Open_SetTotal(const UInt64 *numFiles, const UInt64 *numBytes)
// Z7_COM7F_IMF(COpenArchiveCallback::SetTotal(const UInt64 *numFiles, const UInt64 *numBytes))
{
  // COM_TRY_BEGIN
  RINOK(ProgressDialog.Sync.CheckStop())
  {
    // NSynchronization::CCriticalSectionLock lock(_criticalSection);
    ProgressDialog.Sync.Set_NumFilesTotal(numFiles ? *numFiles : (UInt64)(Int64)-1);
    // if (numFiles)
    {
      ProgressDialog.Sync.Set_FilesProgressMode(numFiles != NULL);
    }
    if (numBytes)
      ProgressDialog.Sync.Set_NumBytesTotal(*numBytes);
  }
  return S_OK;
  // COM_TRY_END
}

HRESULT COpenArchiveCallback::Open_SetCompleted(const UInt64 *numFiles, const UInt64 *numBytes)
// Z7_COM7F_IMF(COpenArchiveCallback::SetCompleted(const UInt64 *numFiles, const UInt64 *numBytes))
{
  // COM_TRY_BEGIN
  // NSynchronization::CCriticalSectionLock lock(_criticalSection);
  if (numFiles)
    ProgressDialog.Sync.Set_NumFilesCur(*numFiles);
  if (numBytes)
    ProgressDialog.Sync.Set_NumBytesCur(*numBytes);
  return ProgressDialog.Sync.CheckStop();
  // COM_TRY_END
}

HRESULT COpenArchiveCallback::Open_CheckBreak()
{
  return ProgressDialog.Sync.CheckStop();
}

HRESULT COpenArchiveCallback::Open_Finished()
{
  return ProgressDialog.Sync.CheckStop();
}

#ifndef Z7_NO_CRYPTO
HRESULT COpenArchiveCallback::Open_CryptoGetTextPassword(BSTR *password)
// Z7_COM7F_IMF(COpenArchiveCallback::CryptoGetTextPassword(BSTR *password))
{
  // COM_TRY_BEGIN
  PasswordWasAsked = true;
  if (!PasswordIsDefined)
  {
#ifdef NANAZIP_MODERN
    K7_PASSWORD_DIALOG_CONTEXT Context = {};
    {
      // Dialog font size from the registry (mirrors the ExtractDialog font).
      DWORD pt = 0;
      HKEY key = nullptr;
      if (::RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\NanaZip\\Options", 0,
          KEY_READ, &key) == ERROR_SUCCESS)
      {
        DWORD size = sizeof(pt);
        ::RegQueryValueExW(key, L"FontSizeDialog", nullptr, nullptr,
            reinterpret_cast<LPBYTE>(&pt), &size);
        ::RegCloseKey(key);
      }
      Context.FontSizeDialog = pt;
    }
    // The "auto show password" setting (HKCU\\Software\\NanaZip\\FM\\AutoShowPassword)
    // is the parent of this box: when it is on, the box is checked by
    // default; otherwise the remembered value is used. The dialog never
    // writes back to the setting.
    bool showPassword = NExtract::Read_ShowPassword();
    {
      DWORD autoShow = 0;
      HKEY key = nullptr;
      if (::RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\NanaZip\\FM", 0,
          KEY_READ, &key) == ERROR_SUCCESS)
      {
        DWORD size = sizeof(autoShow);
        ::RegQueryValueExW(key, L"AutoShowPassword", nullptr, nullptr,
            reinterpret_cast<LPBYTE>(&autoShow), &size);
        ::RegCloseKey(key);
      }
      Context.ShowPassword = (autoShow != 0 || showPassword) ? TRUE : FALSE;
    }
    {
      // Fixed-size ABI buffer: truncate before writing so an over-long
      // password can never trigger wcscpy_s failure.
      UString pwd = Password;
      if (pwd.Len() >= K7_PASSWORD_MAX_PASSWORD_LENGTH)
        pwd.DeleteFrom(K7_PASSWORD_MAX_PASSWORD_LENGTH - 1);
      wcscpy_s(Context.Password, pwd.Ptr());
    }
    Context.AddPasswordCallback = SssAddPasswordToBookCallback;
    ProgressDialog.WaitCreating();
    if (K7ModernShowPasswordDialog(ProgressDialog, &Context) < 0 ||
        !Context.OK)
      return E_ABORT;
    Password = Context.Password;
    PasswordIsDefined = true;
    if ((Context.ShowPassword != FALSE) != showPassword)
      NExtract::Save_ShowPassword(Context.ShowPassword);
#else
    CPasswordDialog dialog;
    bool showPassword = NExtract::Read_ShowPassword();
    dialog.ShowPassword = showPassword;
   
    ProgressDialog.WaitCreating();
    if (dialog.Create(ProgressDialog) != IDOK)
      return E_ABORT;

    Password = dialog.Password;
    PasswordIsDefined = true;
    if (dialog.ShowPassword != showPassword)
      NExtract::Save_ShowPassword(dialog.ShowPassword);
#endif
  }
  return StringToBstr(Password, password);
  // COM_TRY_END
}
#endif
