"""
בדיקת עדכון גרסה מרוחקת — git fetch + השוואת HEAD מקומי מול origin, לא pull
אוטומטי. "מתי לבדוק" (תזמון יומי) הוא אחריות MainWindow (_maybe_check_for_update),
בדיוק כמו backup.py (מנגנון) מול _maybe_run_backup (מדיניות) — אותה חלוקה.

כל קריאת git כאן היא subprocess חוסם עם timeout, ותמיד נקראת מתהליכון נפרד
(MainWindow._check_for_update_worker) — לעולם לא מהתהליכון הראשי, כי רשת
יכולה להיות אטית או לא זמינה בכלל (תחנות רבות מותקנות במתכוון בלי גישה
קבועה לאינטרנט), ותקיעה כאן הייתה מקפיאה את כל ה-UI.
"""

import subprocess
from pathlib import Path

REPO_DIR = Path(__file__).resolve().parent.parent


def _run_git(args, timeout):
    return subprocess.run(["git"] + args, cwd=REPO_DIR, capture_output=True,
                          text=True, timeout=timeout)


def check_for_update():
    """
    מחזיר dict: {"ok", "update_available", "commits_behind", "latest_summary", "error"}.
    לא נוגע בעץ העבודה מעבר ל-fetch (מעדכן רק refs מרוחקים מקומיים) — pull
    בפועל הוא apply_update(), שנקרא רק בלחיצה מפורשת של המשתמש.
    """
    try:
        fetch = _run_git(["fetch", "--quiet"], timeout=20)
        if fetch.returncode != 0:
            return {"ok": False, "error": fetch.stderr.strip() or "git fetch נכשל"}
        behind = _run_git(["rev-list", "--count", "HEAD..@{u}"], timeout=10)
        if behind.returncode != 0:
            return {"ok": False, "error": behind.stderr.strip() or "אין upstream מוגדר"}
        count = int(behind.stdout.strip() or "0")
        latest_summary = ""
        if count > 0:
            log = _run_git(["log", "@{u}", "-1", "--pretty=%h %s"], timeout=10)
            latest_summary = log.stdout.strip()
        return {"ok": True, "update_available": count > 0, "commits_behind": count,
               "latest_summary": latest_summary, "error": None}
    except FileNotFoundError:
        return {"ok": False, "error": "git לא מותקן, או לא נמצא ב-PATH"}
    except subprocess.TimeoutExpired:
        return {"ok": False, "error": "הבדיקה נכשלה (timeout) — כנראה אין חיבור לאינטרנט"}
    except Exception as e:
        return {"ok": False, "error": str(e)}


def apply_update():
    """
    git pull --ff-only בפועל — נקרא רק מלחיצה מפורשת על "עדכן", לעולם לא
    אוטומטית. --ff-only (לא merge) כדי שלא ליצור commit מיזוג על עץ עבודה
    שאף פעם לא נועד לפיתוח מקומי; אם יש שינויים מקומיים שמתנגשים, ה-pull
    נכשל בבירור במקום ליצור מיזוג לא צפוי.
    """
    try:
        result = _run_git(["pull", "--ff-only"], timeout=30)
        if result.returncode != 0:
            return {"ok": False, "error": result.stderr.strip() or result.stdout.strip()}
        return {"ok": True, "output": result.stdout.strip()}
    except subprocess.TimeoutExpired:
        return {"ok": False, "error": "העדכון נכשל (timeout)"}
    except Exception as e:
        return {"ok": False, "error": str(e)}
