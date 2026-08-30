@echo off
rem Register NanaZip ShellExtension (Win11 context menu) via MSIX shell package
rem Usage: RegisterShellExt.cmd  (must run from NanaZip install dir, msix in same dir)
rem Logging + diagnostics go to ShellExt注册日志.txt (written by RegisterShellExt.ps1)
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0RegisterShellExt.ps1"
endlocal & exit /b %ERRORLEVEL%
