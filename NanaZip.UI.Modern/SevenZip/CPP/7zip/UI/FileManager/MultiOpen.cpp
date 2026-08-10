// MultiOpen.cpp

#include "StdAfx.h"

#include "MultiOpen.h"

// **************** SSS Modification Start ****************
// The batch paths are consumed by FM.cpp once the main window exists.
UStringVector g_SssBatchPaths;
// **************** SSS Modification End ****************

static const wchar_t kWindowClass[] = L"SSS_NanaZip_BatchOpen_Window";
static const DWORD kCopyDataId = 0x53535353; // 'SSSS'
static const wchar_t kMutexName[] = L"Local\\SSS.NanaZip.SingleInstance";

static UStringVector g_Queue;
static UInt32 g_Flags; // bit0: some path came with -open, bit1: with -multiopen
// True while the primary is still inside its batching loop; WM_COPYDATA
// then merges into g_Queue. Once false (batch view is being mounted) a
// late batch is appended to g_SssBatchPaths and the main window is
// notified so it can add the paths to the live batch view.
static bool g_SssBatching = false;

extern HWND g_HWND;

static void SssAddPathsUniqueTo(UStringVector &dst, const UStringVector &v)
{
  FOR_VECTOR(i, v)
  {
    bool dup = false;
    FOR_VECTOR(j, dst)
      if (dst[j].IsEqualTo_NoCase(v[i]))
      {
        dup = true;
        break;
      }
    if (!dup)
      dst.Add(v[i]);
  }
}

static void SssAddPathsUnique(const UStringVector &v)
{
  SssAddPathsUniqueTo(g_Queue, v);
}

static void SssSplitLines(const UString &s, UStringVector &v)
{
  unsigned pos = 0;
  const unsigned len = s.Len();
  for (;;)
  {
    const int np = s.Find(L'\n', pos);
    if (np < 0)
    {
      if (pos < len)
      {
        UString part;
        part.SetFrom(s.Ptr(pos), len - pos);
        v.Add(part);
      }
      return;
    }
    if ((unsigned)np > pos)
    {
      UString part;
      part.SetFrom(s.Ptr(pos), (unsigned)np - pos);
      v.Add(part);
    }
    pos = (unsigned)np + 1;
  }
}

static void SssSendBatch(HWND wnd, const UStringVector &paths, bool isOpen, bool isMultiOpen)
{
  UString payload;
  payload += (isMultiOpen ? L'M' : isOpen ? L'O' : L'P');
  FOR_VECTOR(i, paths)
  {
    payload += L'\n';
    payload += paths[i];
  }
  COPYDATASTRUCT cds;
  cds.dwData = kCopyDataId;
  cds.cbData = (DWORD)((payload.Len() + 1) * sizeof(wchar_t));
  cds.lpData = (void *)payload.Ptr_non_const();
  ::SendMessageW(wnd, WM_COPYDATA, 0, (LPARAM)&cds);
}

static LRESULT CALLBACK SssBatchWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (msg == WM_COPYDATA)
  {
    const COPYDATASTRUCT *cds = (const COPYDATASTRUCT *)lParam;
    if (cds && cds->dwData == kCopyDataId && cds->lpData && cds->cbData >= (int)sizeof(wchar_t))
    {
      const wchar_t *data = (const wchar_t *)cds->lpData;
      if (data[0] == L'M')
        g_Flags |= 2;
      else if (data[0] == L'O')
        g_Flags |= 1;
      UStringVector v;
      SssSplitLines(UString(data + 1), v);
      if (g_SssBatching)
      {
        // Still inside the batching loop: merge into the primary queue.
        SssAddPathsUnique(v);
      }
      else
      {
        // The batch view is already up (or about to be): append the late
        // paths and tell the main window to refresh the live view.
        SssAddPathsUniqueTo(g_SssBatchPaths, v);
        if (g_HWND)
          ::PostMessageW(g_HWND, WM_SSS_BATCH_APPEND, 0, 0);
      }
    }
    return TRUE;
  }
  return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool SssHandleBatchOpen(const UStringVector &paths, bool isOpen, bool isMultiOpen)
{
  HANDLE mutex = ::CreateMutexW(NULL, TRUE, kMutexName);
  if (::GetLastError() == ERROR_ALREADY_EXISTS)
  {
    // We are a later process: forward our paths to the primary and exit.
    HWND wnd = NULL;
    for (int i = 0; i < 40 && !wnd; i++)
    {
      wnd = ::FindWindowW(kWindowClass, NULL);
      if (!wnd)
        ::Sleep(50);
    }
    if (wnd)
    {
      SssSendBatch(wnd, paths, isOpen, isMultiOpen);
      ::CloseHandle(mutex);
      return true;
    }
    // Extremely rare: primary exists but its window is not up yet.
    // Fall through and behave as the primary.
    ::CloseHandle(mutex);
    mutex = NULL;
  }

  // Primary instance: create the batching window.
  HINSTANCE hInst = (HINSTANCE)::GetModuleHandleW(NULL);
  WNDCLASSW wc = {0};
  wc.lpfnWndProc = SssBatchWndProc;
  wc.hInstance = hInst;
  wc.lpszClassName = kWindowClass;
  ::RegisterClassW(&wc);
  HWND wnd = ::CreateWindowExW(0, kWindowClass, L"", 0, 0, 0, 0, 0,
      NULL, NULL, hInst, NULL);

  g_Queue.Clear();
  g_Flags = (isOpen ? 1 : 0) | (isMultiOpen ? 2 : 0);
  SssAddPathsUnique(paths);

  // Batching window: 300 ms normally, 100 ms after the first forwarded
  // batch arrives (explorer starts the processes almost simultaneously).
  g_SssBatching = true;
  MSG msg;
  const DWORD start = ::GetTickCount();
  bool gotAny = false;
  for (;;)
  {
    const DWORD limit = gotAny ? 100 : 300;
    if (::GetTickCount() - start >= limit)
      break;
    if (::PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
    {
      if (msg.message == WM_QUIT)
        break;
      ::TranslateMessage(&msg);
      ::DispatchMessageW(&msg);
      if (msg.message == WM_COPYDATA)
        gotAny = true;
      continue;
    }
    ::WaitMessage();
  }
  g_SssBatching = false;

  // A single non-multi path: nothing else arrived, behave as before.
  if (g_Queue.Size() <= 1 && !(g_Flags & 2))
  {
    // Plain single-file open: release the coordinator so a later process
    // can start its own window (double-clicking two archives must open
    // two windows).
    if (wnd)
      ::DestroyWindow(wnd);
    if (mutex)
      ::CloseHandle(mutex);
    return false;
  }

  // **************** SSS Modification Start ****************
  // Batch scenario: keep the coordinator window and the mutex alive for
  // the whole lifetime of this process (they are reclaimed by the OS
  // when the process exits). Explorer launches the command handler in
  // chunks, so late processes keep forwarding their paths here and the
  // main window appends them to the live batch view via
  // WM_SSS_BATCH_APPEND.
  // Hand the merged list to the main-window flow: FM.cpp mounts it as
  // the batch view right after the file-manager window is created.
  g_SssBatchPaths = g_Queue;
  return false;
  // **************** SSS Modification End ****************
}
