@echo off
rem Register NanaZip ShellExtension (Win11 context menu) via MSIX shell package
rem Usage: RegisterShellExt.cmd  (must run from NanaZip install dir, msix in same dir)
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "try { Add-AppxPackage -Path '%~dp0NanaZipShellExt.x64.msix' -ExternalLocation '%~dp0' -ErrorAction Stop; exit 0 } catch { exit 1 }"
endlocal
