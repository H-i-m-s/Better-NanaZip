// MyLoadMenu.h

#ifndef __MY_LOAD_MENU_H
#define __MY_LOAD_MENU_H

void OnMenuActivating(HWND hWnd, HMENU hMenu, int position);
// void OnMenuUnActivating(HWND hWnd, HMENU hMenu, int id);
// void OnMenuUnActivating(HWND hWnd);

bool OnMenuCommand(HWND hWnd, unsigned id);
void MyLoadMenu();

// These flags identify only FileManager's built-in file-list context-menu
// roots. Shell extension items and the Windows system submenu are excluded.
enum EFileContextMenuItem
{
  kFileContextMenuItemOpen,
  kFileContextMenuItemOpenInside,
  kFileContextMenuItemOpenInsideOne,
  kFileContextMenuItemOpenInsideParser,
  kFileContextMenuItemOpenOutside,
  kFileContextMenuItemView,
  kFileContextMenuItemEdit,
  kFileContextMenuItemRename,
  kFileContextMenuItemCopyTo,
  kFileContextMenuItemMoveTo,
  kFileContextMenuItemDelete,
  kFileContextMenuItemSplit,
  kFileContextMenuItemCombine,
  kFileContextMenuItemProperties,
  kFileContextMenuItemComment,
  kFileContextMenuItemCrc,
  kFileContextMenuItemDiff,
  kFileContextMenuItemCreateFolder,
  kFileContextMenuItemCreateFile,
  kFileContextMenuItemLink,
  kFileContextMenuItemAlternateStreams,
  kFileContextMenuItemExtractOneByOne,
  kFileContextMenuItemExtractAll,
  kFileContextMenuItemExtractAllDialog,
  kFileContextMenuItemVerEdit,
  kFileContextMenuItemVerCommit,
  kFileContextMenuItemVerRevert,
  kFileContextMenuItemVerDiff,
  kFileContextMenuItemCount
};

static const UInt32 kFileContextMenuAllFlags =
    ((UInt32)1 << kFileContextMenuItemCount) - 1;

enum EFileContextMenuResource
{
  kFileContextMenuResourceTitle = 2560,
  kFileContextMenuResourceExtractOneByOne,
  kFileContextMenuResourceExtractAll,
  kFileContextMenuResourceExtractAllDialog,
  kFileContextMenuResourceVerEdit,
  kFileContextMenuResourceVerCommit,
  kFileContextMenuResourceVerRevert,
  kFileContextMenuResourceVerDiff
};

bool IsFileContextMenuItemVisible(UInt32 flags, unsigned itemIndex);
void GetFileContextMenuItemText(unsigned itemIndex, UString &text);
void GetFileContextMenuItemName(unsigned itemIndex, UString &name);

struct CFileMenu
{
  bool programMenu;
  bool readOnly;
  bool isHashFolder;
  bool isFsFolder;
  bool allAreFiles;
  bool isAltStreamsSupported;
  int numItems;
  
  FString FilePath;

  CFileMenu():
      programMenu(false),
      readOnly(false),
      isHashFolder(false),
      isFsFolder(false),
      allAreFiles(false),
      isAltStreamsSupported(true),
      numItems(0)
    {}

  void Load(HMENU hMenu, unsigned startPos);
};

bool ExecuteFileCommand(unsigned id);

#endif
