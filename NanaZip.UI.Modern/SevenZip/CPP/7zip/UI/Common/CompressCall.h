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
    bool email, bool showDialog, bool waitFinish);

// **************** NanaZip Modification Start ****************
// void ExtractArchives(const UStringVector &arcPaths, const UString &outFolder, bool showDialog, bool elimDup, UInt32 writeZone);
void ExtractArchives(const UStringVector &arcPaths, const UString &outFolder, bool showDialog, bool elimDup, UInt32 writeZone, bool smartExtract = false, bool openFolder = false, UInt32 overwriteMode = (UInt32)(Int32)-1, bool waitFinish = false, bool suppressDelete = false, bool useDlgState = false, const UString &releaseBeforeDeleteMarker = UString(), const UString &passwordSessionId = UString(), bool forceDeleteAfter = false);
// **************** NanaZip Modification End ****************
void TestArchives(const UStringVector &arcPaths, bool hashMode = false);

void CalcChecksum(const UStringVector &paths,
    const UString &methodName,
    const UString &arcPathPrefix,
    const UString &arcFileName);

void Benchmark(bool totalMode);

// **************** SSS Modification Start ****************
// Closes every 7zG process this File Manager started (polite WM_CLOSE,
// then a hard kill after a short grace period). Called from the File
// Manager exit paths so a dialog left open does not survive the main
// window (see CompressCall.cpp).
void SssShutdownChildProcesses();
// **************** SSS Modification End ****************

#endif
