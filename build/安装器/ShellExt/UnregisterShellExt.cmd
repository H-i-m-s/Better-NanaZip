@echo off
rem Unregister NanaZip ShellExtension (MSIX shell package)
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "try { Get-AppxPackage -Name 'SSS.NanaZip.ShellExtension' -ErrorAction SilentlyContinue | Remove-AppxPackage -ErrorAction Stop; exit 0 } catch { exit 1 }"
endlocal
