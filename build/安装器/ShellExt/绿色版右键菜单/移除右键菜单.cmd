@echo off
rem NanaZip green build: remove context menu integration and restore clean portable state
rem Chinese output lives in context_menu_remove.ps1 (UTF-8 BOM). This launcher is pure ASCII on purpose.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0context_menu_remove.ps1"
