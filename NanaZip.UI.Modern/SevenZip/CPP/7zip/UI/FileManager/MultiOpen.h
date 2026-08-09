// MultiOpen.h
// Single-instance merge for batch archive opening.
//   - right-click multi-select:   NanaZip.exe "a.zip" "b.zip" -multiopen
//   - double-click multi-select:  NanaZip.exe "a.zip" -open   (x N, almost
//     simultaneously, each in its own process)
// The first process becomes the primary instance and waits a short
// batching window to collect paths forwarded by later processes, then
// opens one batch-extract window instead of N file-manager windows.

#ifndef __MULTI_OPEN_H
#define __MULTI_OPEN_H

#include "../../../Common/MyString.h"

// Returns true if the request was fully handled here (the process
// should exit); false if the caller should continue the normal
// single-file flow (primary instance, nothing else arrived).
bool SssHandleBatchOpen(const UStringVector &paths, bool isOpen, bool isMultiOpen);

// **************** SSS Modification Start ****************
// Merged batch paths, handed to the main-window flow (FM.cpp) which
// mounts them as the batch view once the file-manager window exists.
extern UStringVector g_SssBatchPaths;
// **************** SSS Modification End ****************

#endif
