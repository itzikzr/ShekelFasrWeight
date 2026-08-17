"""
הפעלה אוטומטית עם עליית מערכת ההפעלה.

Windows: קיצור ל-run.bat בתיקיית Startup של המשתמש הנוכחי (נוצר דרך
WScript.Shell ב-PowerShell, כי אין דרך פשוטה אחרת ליצור .lnk).
Linux: קובץ .desktop תקני תחת ~/.config/autostart/ (XDG autostart).

זהה בכוונה לקבצים העצמאיים install-autostart-windows.bat /
install-autostart-linux.sh (repo root) — אותם נתיבים/שמות קובץ בדיוק, כדי
ששני המנגנונים (הצ'קבוקס כאן וההרצה הידנית החד-פעמית) לא ייצרו רשומות
כפולות/סותרות.
"""
import subprocess
import sys
from pathlib import Path

APP_DIR = Path(__file__).resolve().parent.parent
_LINUX_AUTOSTART_FILE = Path.home() / ".config" / "autostart" / "scale-sampler.desktop"


def _windows_startup_shortcut():
    import os
    appdata = os.environ.get("APPDATA")
    if not appdata:
        return None
    return (Path(appdata) / "Microsoft" / "Windows" / "Start Menu" / "Programs"
            / "Startup" / "Scale Sampler.lnk")


def is_enabled():
    if sys.platform.startswith("win"):
        shortcut = _windows_startup_shortcut()
        return shortcut is not None and shortcut.exists()
    if sys.platform.startswith("linux"):
        return _LINUX_AUTOSTART_FILE.exists()
    return False


def is_supported():
    return sys.platform.startswith("win") or sys.platform.startswith("linux")


def enable():
    if sys.platform.startswith("win"):
        _enable_windows()
    elif sys.platform.startswith("linux"):
        _enable_linux()
    else:
        raise NotImplementedError("הפעלה אוטומטית נתמכת רק ב-Windows וב-Linux")


def disable():
    if sys.platform.startswith("win"):
        shortcut = _windows_startup_shortcut()
        if shortcut and shortcut.exists():
            shortcut.unlink()
    elif sys.platform.startswith("linux"):
        if _LINUX_AUTOSTART_FILE.exists():
            _LINUX_AUTOSTART_FILE.unlink()


def _enable_windows():
    shortcut = _windows_startup_shortcut()
    if shortcut is None:
        raise RuntimeError("לא נמצאה תיקיית APPDATA")
    shortcut.parent.mkdir(parents=True, exist_ok=True)
    run_bat = APP_DIR / "run.bat"
    icon = APP_DIR / "scale_app" / "assets" / "icon.ico"
    ps_script = (
        "$ws = New-Object -ComObject WScript.Shell; "
        f"$sc = $ws.CreateShortcut('{shortcut}'); "
        f"$sc.TargetPath = '{run_bat}'; "
        f"$sc.WorkingDirectory = '{APP_DIR}'; "
        f"$sc.IconLocation = '{icon}'; "
        "$sc.Save()"
    )
    subprocess.run(
        ["powershell", "-NoProfile", "-Command", ps_script],
        check=True, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))


def _enable_linux():
    _LINUX_AUTOSTART_FILE.parent.mkdir(parents=True, exist_ok=True)
    run_sh = APP_DIR / "run.sh"
    icon = APP_DIR / "scale_app" / "assets" / "icon.png"
    content = (
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=תחנת שקילה\n"
        "Comment=Scale Sampler — Swan weighing station\n"
        f"Exec={run_sh}\n"
        f"Icon={icon}\n"
        "Terminal=false\n"
        "X-GNOME-Autostart-enabled=true\n"
    )
    _LINUX_AUTOSTART_FILE.write_text(content, encoding="utf-8")
    _LINUX_AUTOSTART_FILE.chmod(0o755)
