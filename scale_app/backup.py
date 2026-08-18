"""
מנגנון הגיבוי עצמו — sqlite3 backup API + גיזום גיבויים ישנים. "מתי להריץ"
(תזמון יומי, בעלייה ותוך כדי ריצה) הוא אחריות MainWindow (_maybe_run_backup),
לא כאן — זה נשאר פונקציה טהורה שרק מגבה ומגזימה, בלי מצב/הגדרות משלה.
"""

import datetime
import sqlite3
from pathlib import Path

from . import db

BACKUP_RETENTION_COUNT = 14  # שומרים רק את 14 הגיבויים האחרונים
_FILENAME_PREFIX = "scale_data_backup_"


def run_backup(dest_dir: "Path") -> Path:
    """
    מגבה את ה-DB הפעיל בעזרת sqlite3.Connection.backup() אל dest_dir — לא
    shutil.copy על קובץ scale_data.db הגולמי, כדי שהגיבוי יהיה עקבי גם אם יש
    טרנזקציה פתוחה (backup API מתעד רק מצב commit-ed, לא מעתיק בתים אמצע-כתיבה).
    יוצר את dest_dir אם עוד לא קיימת (יכולה להיות בתוך הפרויקט או בדיסק אחר).
    """
    dest_dir = Path(dest_dir)
    dest_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y-%m-%d_%H%M%S")
    dest_path = dest_dir / f"{_FILENAME_PREFIX}{stamp}.db"
    dest_conn = sqlite3.connect(str(dest_path))
    try:
        db._c().backup(dest_conn)
    finally:
        dest_conn.close()
    _prune_old_backups(dest_dir)
    return dest_path


def _prune_old_backups(dest_dir: Path):
    files = sorted(dest_dir.glob(f"{_FILENAME_PREFIX}*.db"))
    excess = len(files) - BACKUP_RETENTION_COUNT
    for f in files[:max(0, excess)]:
        try:
            f.unlink()
        except OSError:
            pass
