@echo off
rem Removes the Startup-folder shortcut created by install-autostart-windows.bat.
setlocal
set "SHORTCUT=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\Scale Sampler.lnk"

if exist "%SHORTCUT%" (
    del "%SHORTCUT%"
    echo Removed: %SHORTCUT%
    echo The app will no longer launch automatically at Windows login.
) else (
    echo No auto-start shortcut found — nothing to remove.
)
pause
