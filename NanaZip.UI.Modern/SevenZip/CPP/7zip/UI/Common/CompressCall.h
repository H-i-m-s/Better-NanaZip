// CompressCall.h

#ifndef __COMPRESS_CALL_H
#define __COMPRESS_CALL_H

#include "../../../Common/MyString.h"

UString GetQuotedString(const UString &s);

HRESULT CompressFiles(
    const UString &arcPathPrefix,
    const UString &arcName,
    const UString &arcType,
    bool addExtension,
    const UStringVector &names,
    bool email, bool showDialog, bool waitFinish,
    bool shellHostMode = false);

// **************** NanaZip Modification Start ****************
// void ExtractArchives(const UStringVector &arcPaths, const UString &outFolder, bool showDialog, bool elimDup, UInt32 writeZone);
// SSS Modification: shellHostMode marks a call made from an in-process
// shell extension (IExplorerCommand::Invoke runs on the explorer.exe UI
// thread). Such a call must not hold the shell: no batch password session
// (7zG handles the automatic password itself), no wait for the 7zG process
// and no -sspid (that pid would be explorer.exe). The File Manager keeps
// the default false.
void ExtractArchives(const UStringVector &arcPaths, const UString &outFolder, bool showDialog, bool elimDup, UInt32 writeZone, bool smartExtract = false, bool openFolder = false, UInt32 overwriteMode = (UInt32)(Int32)-1, bool waitFinish = false, bool suppressDelete = false, bool useDlgState = false, const UString &releaseBeforeDeleteMarker = UString(), const UString &passwordSessionId = UString(), bool forceDeleteAfter = false, bool shellHostMode = false);
// **************** NanaZip Modification End ****************
// SSS Modification: the three entries below take the same shellHostMode as
// ExtractArchives - true for callers hosted in explorer.exe, whose UI
// thread must never wait for 7zG. It is forwarded as waitProcessAfterEvent
// and as Call7zGui's own shellHostMode (no -sspid).
void TestArchives(const UStringVector &arcPaths, bool hashMode = false, bool shellHostMode = false);

void CalcChecksum(const UStringVector &paths,
    const UString &methodName,
    const UString &arcPathPrefix,
    const UString &arcFileName,
    bool shellHostMode = false);

void Benchmark(bool totalMode);

// **************** SSS Modification Start ****************
// Closes every 7zG process this File Manager started (polite WM_CLOSE,
// then a hard kill after a short grace period). Called from the File
// Manager exit paths so a dialog left open does not survive the main
// window (see CompressCall.cpp).
void SssShutdownChildProcesses();
// **************** SSS Modification End ****************

#endif
