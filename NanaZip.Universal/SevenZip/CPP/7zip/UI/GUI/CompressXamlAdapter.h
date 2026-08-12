// CompressXamlAdapter.h

#ifndef ZIP7_INC_COMPRESS_XAML_ADAPTER_H
#define ZIP7_INC_COMPRESS_XAML_ADAPTER_H

#include "CompressDialogCore.h"

enum ECompressXamlResult
{
  kXamlNotAvailable,
  kXamlCancelled,
  kXamlOk
};

// Show the XAML compression dialog for one dialog session.
// On kXamlOk the caller reads core.Info for the committed result.
ECompressXamlResult K7ShowCompressDialogXaml(HWND hwndParent, CCompressDialogCore &core);

#endif
