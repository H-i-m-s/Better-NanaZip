@echo off
rem NanaZip green build: one-click context menu install (Win11 top-level + legacy fallback)
rem Chinese output lives in context_menu_install.ps1 (UTF-8 BOM). This launcher is pure ASCII on purpose.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0context_menu_install.ps1"
