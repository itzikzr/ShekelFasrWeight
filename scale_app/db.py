"""
שכבת התמדה — SQLite, stdlib בלבד.

כלל: קריאה/כתיבה לבסיס הנתונים מתבצעת רק מהתהליכון הראשי (דרך ה-queue pump ב-main_window.py) —
כמו הכלל הקיים שתהליכוני איסוף הנתונים לא נוגעים ב-Tk. כך אין צורך ב-check_same_thread=False
ואין סיכוני concurrency.
"""

import sqlite3
import datetime
from pathlib import Path

DB_FILE = Path(__file__).parent.parent / "scale_data.db"

_SCHEMA = """
CREATE TABLE IF NOT EXISTS products (
    id              INTEGER PRIMARY KEY,
    name            TEXT NOT NULL UNIQUE,
    target_weight   REAL NOT NULL,
    tolerance_upper REAL NOT NULL,
    tolerance_lower REAL NOT NULL,
    created_at      TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS weighings (
    id              INTEGER PRIMARY KEY,
    timestamp       TEXT NOT NULL,
    product_id      INTEGER REFERENCES products(id),
    product_name    TEXT,
    target_weight   REAL,
    tolerance_upper REAL,
    tolerance_lower REAL,
    decided_weight  REAL NOT NULL,
    verdict         TEXT NOT NULL,
    reading_count   INTEGER,
    elapsed_seconds REAL,
    min_weight      REAL,
    max_weight      REAL,
    basis           TEXT
);

CREATE TABLE IF NOT EXISTS weighing_readings (
    id          INTEGER PRIMARY KEY,
    weighing_id INTEGER NOT NULL REFERENCES weighings(id),
    t_offset    REAL NOT NULL,
    weight      REAL NOT NULL,
    in_window   INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_weighing_readings_weighing_id ON weighing_readings(weighing_id);
"""

# מספר השקילות (האחרונות) שעבורן שומרים את הקריאות הגולמיות המפורטות — ראו
# _prune_weighing_readings. שורת ה-weighings עצמה (הסיכום) נשארת לתמיד בכל מקרה.
DETAIL_RETENTION_COUNT = 100

_conn = None


def connect(db_path=None):
    global _conn
    _conn = sqlite3.connect(db_path if db_path is not None else DB_FILE)
    _conn.row_factory = sqlite3.Row
    _conn.executescript(_SCHEMA)
    _conn.commit()
    return _conn


def _c():
    if _conn is None:
        connect()
    return _conn


# ──────────────────────────────────────────────
# Products
# ──────────────────────────────────────────────

def list_products():
    return _c().execute("SELECT * FROM products ORDER BY name").fetchall()


def get_product(product_id):
    return _c().execute("SELECT * FROM products WHERE id = ?", (product_id,)).fetchone()


def add_product(name, target_weight, tolerance_upper, tolerance_lower):
    cur = _c().execute(
        "INSERT INTO products (name, target_weight, tolerance_upper, tolerance_lower, created_at) "
        "VALUES (?, ?, ?, ?, ?)",
        (name, target_weight, tolerance_upper, tolerance_lower,
         datetime.datetime.now().isoformat(timespec="seconds")))
    _c().commit()
    return cur.lastrowid


def update_product(product_id, name, target_weight, tolerance_upper, tolerance_lower):
    _c().execute(
        "UPDATE products SET name=?, target_weight=?, tolerance_upper=?, tolerance_lower=? "
        "WHERE id=?",
        (name, target_weight, tolerance_upper, tolerance_lower, product_id))
    _c().commit()


def delete_product(product_id):
    _c().execute("DELETE FROM products WHERE id = ?", (product_id,))
    _c().commit()


# ──────────────────────────────────────────────
# Weighings
# ──────────────────────────────────────────────

def classify(weight, target, tolerance_upper, tolerance_lower):
    """ ירוק בתחום [target-lower, target+upper], אדום מעל, צהוב מתחת. """
    if weight > target + tolerance_upper:
        return "red"
    if weight < target - tolerance_lower:
        return "yellow"
    return "green"


def insert_weighing(decided_weight, reading_count, elapsed_seconds, min_weight, max_weight,
                     basis, product=None):
    """
    product: שורת sqlite3.Row של מוצר (name/target_weight/tolerance_upper/tolerance_lower/id),
    או None אם לא נבחר מוצר. מחזיר (verdict, row_id).
    """
    if product is not None:
        verdict = classify(decided_weight, product["target_weight"],
                            product["tolerance_upper"], product["tolerance_lower"])
        product_id       = product["id"]
        product_name     = product["name"]
        target_weight    = product["target_weight"]
        tolerance_upper  = product["tolerance_upper"]
        tolerance_lower  = product["tolerance_lower"]
    else:
        verdict = "none"
        product_id = product_name = target_weight = tolerance_upper = tolerance_lower = None

    cur = _c().execute(
        "INSERT INTO weighings (timestamp, product_id, product_name, target_weight, "
        "tolerance_upper, tolerance_lower, decided_weight, verdict, reading_count, "
        "elapsed_seconds, min_weight, max_weight, basis) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
        (datetime.datetime.now().isoformat(timespec="seconds"), product_id, product_name,
         target_weight, tolerance_upper, tolerance_lower, decided_weight, verdict,
         reading_count, elapsed_seconds, min_weight, max_weight, basis))
    _c().commit()
    return verdict, cur.lastrowid


def list_weighings(product_id=None, verdict=None, limit=None, since=None):
    query = "SELECT * FROM weighings"
    clauses, params = [], []
    if product_id is not None:
        clauses.append("product_id = ?")
        params.append(product_id)
    if verdict is not None:
        clauses.append("verdict = ?")
        params.append(verdict)
    if since is not None:
        # timestamp is stored via datetime.isoformat(timespec="seconds"); since
        # must be formatted the same way for the string comparison to sort correctly.
        clauses.append("timestamp >= ?")
        params.append(since)
    if clauses:
        query += " WHERE " + " AND ".join(clauses)
    query += " ORDER BY id DESC"
    if limit:
        query += f" LIMIT {int(limit)}"
    return _c().execute(query, params).fetchall()


def get_weighing(weighing_id):
    return _c().execute("SELECT * FROM weighings WHERE id = ?", (weighing_id,)).fetchone()


# ──────────────────────────────────────────────
# Weighing readings (פירוט קריאות — נשמר רק ל-DETAIL_RETENTION_COUNT השקילות האחרונות)
# ──────────────────────────────────────────────

def insert_weighing_readings(weighing_id, readings, window_start, window_end):
    """
    readings: רשימת (t_offset, weight, status) כפי שנאספה ב-session. window_start/window_end
    (כפי שהוחזרו מ-decide_weight) מסמנים אילו קריאות היו בפועל בבסיס החישוב (in_window=1).
    שומר ומגביל אוטומטית לשקילות הפירוט האחרונות בלבד.
    """
    conn = _c()
    rows = [(weighing_id, t_offset, weight, 1 if window_start <= i < window_end else 0)
            for i, (t_offset, weight, _status) in enumerate(readings)]
    conn.executemany(
        "INSERT INTO weighing_readings (weighing_id, t_offset, weight, in_window) "
        "VALUES (?, ?, ?, ?)", rows)
    conn.execute(
        "DELETE FROM weighing_readings WHERE weighing_id NOT IN ("
        "  SELECT DISTINCT weighing_id FROM weighing_readings ORDER BY weighing_id DESC LIMIT ?)",
        (DETAIL_RETENTION_COUNT,))
    conn.commit()


def get_weighing_readings(weighing_id):
    return _c().execute(
        "SELECT * FROM weighing_readings WHERE weighing_id = ? ORDER BY id",
        (weighing_id,)).fetchall()
