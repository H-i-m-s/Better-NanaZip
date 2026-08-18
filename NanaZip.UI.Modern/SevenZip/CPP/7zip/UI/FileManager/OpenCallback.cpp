// OpenCallback.cpp

#include "StdAfx.h"

#include <string>
#include <vector>

#include "../../../Common/ComTry.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariant.h"

#include "../../Common/FileStreams.h"

#include "../Common/ZipRegistry.h"

#include "OpenCallback.h"
#include "PasswordDialog.h"
#include "NanaZip.Modern.h"
#include <NanaZip.Password.h>

namespace
{
  static BOOLEAN WINAPI QueryPasswordForDialog(
      LPCWSTR archivePath,
      UINT32 source,
      LPVOID context,
      LPWSTR password,
      UINT32 passwordCapacity)
  {
    if (!password || passwordCapacity == 0)
      return FALSE;
    const std::wstring *fullArchivePath =
        static_cast<const std::wstring *>(context);
    const std::wstring path = fullArchivePath ? *fullArchivePath
        : (archivePath ? archivePath : L"");
    if (path.empty())
      return FALSE;
    std::wstring value;
    if (source == 1)
    {
      if (!NanaZipPassword::QueryCloudPassword(path, value))
        return FALSE;
    }
    else
    {
      std::vector<NanaZipPassword::Candidate> candidates;
      if (!NanaZipPassword::LoadLocalCandidates(candidates) || candidates.empty())
        return FALSE;
      value = candidates.front().Value;
    }
    wcsncpy_s(password, passwordCapacity, value.c_str(), _TRUNCATE);
    return TRUE;
  }
}

using namespace NWindows;

STDMETHODIMP COpenArchiveCallback::SetTotal(const UInt64 *numFiles, const UInt64 *numBytes)
{
  RINOK(ProgressDialog.Sync.CheckStop());
  {
    // NSynchronization::CCriticalSectionLock lock(_criticalSection);
    ProgressDialog.Sync.Set_NumFilesTotal(numFiles ? *numFiles : (UInt64)(Int64)-1);
    // if (numFiles)
    {
      ProgressDialog.Sync.Set_BytesProgressMode(numFiles == NULL);
    }
    if (numBytes)
      ProgressDialog.Sync.Set_NumBytesTotal(*numBytes);
  }
  return S_OK;
}

STDMETHODIMP COpenArchiveCallback::SetCompleted(const UInt64 *numFiles, const UInt64 *numBytes)
{
  // NSynchronization::CCriticalSectionLock lock(_criticalSection);
  if (numFiles)
    ProgressDialog.Sync.Set_NumFilesCur(*numFiles);
  if (numBytes)
    ProgressDialog.Sync.Set_NumBytesCur(*numBytes);
  return ProgressDialog.Sync.CheckStop();
}

STDMETHODIMP COpenArchiveCallback::SetTotal(const UInt64 total)
{
  RINOK(ProgressDialog.Sync.CheckStop());
  ProgressDialog.Sync.Set_NumBytesTotal(total);
  return S_OK;
}

STDMETHODIMP COpenArchiveCallback::SetCompleted(const UInt64 *completed)
{
  return ProgressDialog.Sync.Set_NumBytesCur(completed);
}

STDMETHODIMP COpenArchiveCallback::GetProperty(PROPID propID, PROPVARIANT *value)
{
  NCOM::CPropVariant prop;
  if (_subArchiveMode)
  {
    switch (propID)
    {
      case kpidName: prop = _subArchiveName; break;
    }
  }
  else
  {
    switch (propID)
    {
      case kpidName:  prop = fs2us(_fileInfo.Name); break;
      case kpidIsDir:  prop = _fileInfo.IsDir(); break;
      case kpidSize:  prop = _fileInfo.Size; break;
      case kpidAttrib:  prop = (UInt32)_fileInfo.Attrib; break;
      case kpidCTime:  prop = _fileInfo.CTime; break;
      case kpidATime:  prop = _fileInfo.ATime; break;
      case kpidMTime:  prop = _fileInfo.MTime; break;
    }
  }
  prop.Detach(value);
  return S_OK;
}

STDMETHODIMP COpenArchiveCallback::GetStream(const wchar_t *name, IInStream **inStream)
{
  COM_TRY_BEGIN
  *inStream = NULL;
  if (_subArchiveMode)
    return S_FALSE;

  FString fullPath;
  if (!NFile::NName::GetFullPath(_folderPrefix, us2fs(name), fullPath))
    return S_FALSE;
  if (!_fileInfo.Find_FollowLink(fullPath))
    return S_FALSE;
  if (_fileInfo.IsDir())
    return S_FALSE;
  CInFileStream *inFile = new CInFileStream;
  CMyComPtr<IInStream> inStreamTemp = inFile;
  if (!inFile->Open(fullPath))
    return ::GetLastError();
  *inStream = inStreamTemp.Detach();
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP COpenArchiveCallback::CryptoGetTextPassword(BSTR *password)
{
  COM_TRY_BEGIN
  PasswordWasAsked = true;
  if (!PasswordIsDefined)
  {
#ifdef NANAZIP_MODERN
    K7_PASSWORD_DIALOG_CONTEXT Context = {};
    std::wstring queryArchivePath(
        fs2us(_folderPrefix + _fileInfo.Name).Ptr());
    {
      wcsncpy_s(Context.ArchivePath, queryArchivePath.c_str(), _TRUNCATE);
      Context.QueryCallback = QueryPasswordForDialog;
      Context.QueryContext = &queryArchivePath;
      Context.PasswordSource = 0;
    }
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
      DWORD autoShare = 0;
      HKEY key = nullptr;
      if (::RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\NanaZip\\FM", 0,
          KEY_READ, &key) == ERROR_SUCCESS)
      {
        DWORD size = sizeof(autoShare);
        ::RegQueryValueExW(key, L"AutoSharePassword", nullptr, nullptr,
            reinterpret_cast<LPBYTE>(&autoShare), &size);
        ::RegCloseKey(key);
      }
      Context.SharePassword = autoShare != 0 ? TRUE : FALSE;
    }
    {
      // Fixed-size ABI buffer: truncate before writing so an over-long
      // password can never trigger wcscpy_s failure.
      UString pwd = Password;
      if (pwd.Len() >= K7_PASSWORD_MAX_PASSWORD_LENGTH)
        pwd.DeleteFrom(K7_PASSWORD_MAX_PASSWORD_LENGTH - 1);
      wcscpy_s(Context.Password, pwd.Ptr());
    }
    ProgressDialog.WaitCreating();
    if (K7ModernShowPasswordDialog(ProgressDialog, &Context) < 0 ||
        !Context.OK)
      return E_ABORT;
    Password = Context.Password;
    PasswordIsDefined = true;
    PasswordSource = Context.PasswordSource == 1 ? NanaZipPassword::PasswordSource::Cloud
        : (Context.PasswordSource == 2 ? NanaZipPassword::PasswordSource::Local
        : NanaZipPassword::PasswordSource::Manual);
    SharePasswordAuthorized = Context.SharePassword != FALSE;
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
  COM_TRY_END
}
