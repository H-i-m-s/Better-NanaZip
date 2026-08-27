// ContextMenuFlags.h

#ifndef __CONTEXT_MENU_FLAGS_H
#define __CONTEXT_MENU_FLAGS_H

namespace NContextMenuFlags
{
  const UInt32 kExtract = 1 << 0;
  const UInt32 kExtractHere = 1 << 1;
  const UInt32 kExtractTo = 1 << 2;
// **************** NanaZip Modification Start ****************
  const UInt32 kExtractHereSmart = 1 << 3;
// **************** NanaZip Modification End ****************

  const UInt32 kTest = 1 << 4;
  const UInt32 kOpen = 1 << 5;
  const UInt32 kOpenAs = 1 << 6;

  const UInt32 kCompress = 1 << 8;
  const UInt32 kCompressTo7z = 1 << 9;
  const UInt32 kCompressEmail = 1 << 10;
  const UInt32 kCompressTo7zEmail = 1 << 11;
  const UInt32 kCompressToZip = 1 << 12;
  const UInt32 kCompressToZipEmail = 1 << 13;

// **************** SSS Modification Start ****************
  // Smart extract to the current folder, then delete the archive after a
  // successful extraction regardless of the global DeleteAfterExtract
  // setting (for users who keep the default delete switch off but
  // sometimes want the archive gone in one action).
  const UInt32 kExtractHereSmartDelete = 1 << 14;
// **************** SSS Modification End ****************

  const UInt32 kCRC_Cascaded = (UInt32)1 << 30;
  const UInt32 kCRC = (UInt32)1 << 31;
}

#endif
