@echo off
rem Creates a shortcut in the current user's Startup folder so the app
rem launches automatically at Windows login. Safe to run more than once
rem (just re-creates the same shortcut). Run uninstall-autostart-windows.bat
rem to remove it again.
setlocal
set "SCRIPT_DIR=%~dp0"
set "STARTUP_DIR=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup"
set "SHORTCUT=%STARTUP_DIR%\Scale Sampler.lnk"

powershell -NoProfile -Command ^
  "$ws = New-Object -ComObject WScript.Shell;" ^
  "$sc = $ws.CreateShortcut('%SHORTCUT%');" ^
  "$sc.TargetPath = '%SCRIPT_DIR%run.bat';" ^
  "$sc.WorkingDirectory = '%SCRIPT_DIR%';" ^
  "$sc.IconLocation = '%SCRIPT_DIR%scale_app\assets\icon.ico';" ^
  "$sc.Save()"

if exist "%SHORTCUT%" (
    echo Done. The app will now launch automatically at Windows login.
    echo Shortcut created at: %SHORTCUT%
    echo To undo this, run uninstall-autostart-windows.bat.
) else (
    echo Failed to create the shortcut — check the errors above.
)
pause
