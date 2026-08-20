"""
דשבורד רשת מקומית — עמודי HTML קריאים-בלבד שמציגים סטטוס/היסטוריה/דוחות,
נגישים מכל מכשיר באותה רשת מקומית (טלפון וכו') דרך
http://<כתובת-ה-IP-של-ה-NUC>:<פורט>/. לא מערכת ריבוי-תחנות מלאה (יש כרגע
תחנה אחת בפועל) — רק גישה נוחה בלי לעמוד ליד המחשב.

http.server.ThreadingHTTPServer (stdlib בלבד, כמו שאר האפליקציה) רץ בתהליכון
daemon נפרד. אין שום endpoint לכתיבה בכוונה — זה LAN פתוח בלי אימות, אז שום
פעולה (מוצרים/הגדרות/שליטה בממסרים) לא נגישה ממנו, רק תצוגה.

שני מקורות נתונים נפרדים בכוונה, לא אחד:
  - get_snapshot(): dict בזיכרון (MainWindow._dashboard_snapshot) לחיבור/משקל
    חי — אלה לא בכלל ב-DB, ואי אפשר "לשאול" אותם מ-thread אחר בלי ה-callback
    הזה. קריאה בטוחה חוצת-thread כי ההחלפה תמיד היא הצבה שלמה של dict חדש
    (ראו MainWindow._update_dashboard_snapshot), לא מוטציה במקום — בדיוק כמו
    EngineSettings/דגלי הממסרים בשאר האפליקציה.
  - get_db_path(): callable שמחזיר את נתיב ה-DB *בכל קריאה* (לא נתיב שנשמר
    פעם אחת ב-__init__) כדי לא לחזור על באג-הבדיקות המתועד ב-db.py
    (db.connect(db_path=None) קורא את DB_FILE בתוך גוף הפונקציה, לא כברירת
    מחדל שנקבעת בזמן import, בדיוק מהסיבה הזו). כל עמוד שמציג שקילות —
    כולל /api/status עצמו, לא רק /history ו-/reports — פותח חיבור sqlite
    *נפרד* משלו לכל בקשה (uri=True, mode=ro — readonly אמיתי, לא רק מוסכם)
    במקום להשתמש ב-db.py/db._c() בכלל: זה החיבור המשותף שהאפליקציה מקפידה
    שרק ה-thread הראשי יגע בו (ראו db.py) — thread של שרת ה-HTTP הזה חייב
    חיבור עצמאי, לא את אותו אחד.

עמוד הבית מתעדכן חי (fetch ל-/api/status כל 4 שניות, בונה מחדש את שתי
הטבלאות + האריחים מ-JS) — לא רק המשקל החי כמו בגרסה הקודמת. /api/status
מחזיר גם רשימת "שקילות אחרונות"/סיכום-מוצרים טריים בכל קריאה, לא רק מצב
הזיכרון — כך ששקילה חדשה מופיעה בלי לרענן את הדף. limit (20/50/100/300,
ראו RECENT_LIMIT_OPTIONS) עובר גם בטעינת הדף הראשונית (querystring, GET
form) וגם בכל בקשת polling עוקבת (embedd-ed כקבוע ב-JS), כך שהם תמיד
מסונכרנים.
"""

import http.server
import json
import socket
import sqlite3
import threading
from datetime import datetime, timedelta
from urllib.parse import parse_qs, urlparse

VERDICT_LABELS = {"green": "ירוק — בטווח", "red": "אדום — מעל", "yellow": "צהוב — מתחת",
                  "none": "ללא מוצר"}
# תווית קצרה ל"שקילות אחרונות" בעמוד הבית בלבד — /history משתמש ב-VERDICT_LABELS
# המלא, זה טבלת יומן מפורטת; "שקילות אחרונות" היא תקציר מהיר, לא צריך את
# צבע-המילה החזרתי (ירוק/אדום/צהוב) כשגם צבע התא עצמו כבר מסמן את זה.
SHORT_VERDICT_LABELS = {"green": "בטווח", "red": "מעל", "yellow": "מתחת", "none": "—"}
VERDICT_COLORS = {"green": "#2e7d32", "red": "#c62828", "yellow": "#f9a825", "none": "#9aa5b1"}
HISTORY_ROW_LIMIT = 300
RECENT_LIMIT_OPTIONS = [20, 50, 100, 300]
DEFAULT_RECENT_LIMIT = 20

_CSS = """
  body { font-family: Arial, sans-serif; background: #f4f6f9; color: #1a1a1a;
         margin: 0; padding: 16px; }
  a { color: #1a6fb5; text-decoration: none; }
  .nav { display: flex; gap: 16px; margin-bottom: 14px; font-size: 15px; }
  .nav a.active { font-weight: bold; color: #1a1a1a; }
  .card { background: #fff; border-radius: 10px; padding: 16px 20px; margin-bottom: 14px;
          box-shadow: 0 1px 3px rgba(0,0,0,.1); }
  .status-row { display: flex; align-items: center; gap: 10px; font-size: 20px; }
  .dot { width: 14px; height: 14px; border-radius: 50%; }
  .weight { font-size: 48px; font-weight: bold; color: #1a6fb5; text-align: center; margin: 8px 0; }
  /* direction: ltr במפורש (לא מסתמכים על היפוך flex-row אוטומטי של dir="rtl"
     בעמוד — נצפה שלא כל דפדפן מתייחס לזה אותו דבר). מסמן מוצג פיזית שמאל-
     לימין לפי סדר ה-DOM עצמו, בלי תלות בפרשנות RTL של הדפדפן — אדום, ירוק,
     צהוב במקור = אדום משמאל, צהוב מימין, בעקביות בכל דפדפן.
     flex-wrap: nowrap בכוונה — שלושת המלבנים תמיד בשורה אחת, גם על מסך טלפון
     צר; min-width קבוע (90px) על .tile הוא מה שגרם לשבירה לשתי שורות בפועל
     על מסך צר מדי בשבילו (נצפה על מכשיר אמיתי) — הוסר, המלבנים מתכווצים
     לפי הצורך במקום לעבור לשורה חדשה. */
  .tiles { display: flex; direction: ltr; gap: 8px; flex-wrap: nowrap; margin-bottom: 14px; }
  .tile { flex: 1; min-width: 0; background: #fff; border-radius: 10px; padding: 10px 6px;
          text-align: center; box-shadow: 0 1px 3px rgba(0,0,0,.1); }
  .tile .n { font-size: 22px; font-weight: bold; }
  .tile .l { font-size: 12px; color: #666; margin-top: 4px; }
  .muted { color: #666; font-size: 13px; }
  .row-header { display: flex; align-items: center; justify-content: space-between;
                gap: 12px; margin-bottom: 10px; flex-wrap: wrap; }
  .row-header h3 { margin: 0; }
  .row-header form { margin: 0; }
  table { width: 100%; border-collapse: collapse; background: #fff; border-radius: 10px;
          overflow: hidden; box-shadow: 0 1px 3px rgba(0,0,0,.1); }
  th, td { padding: 8px 10px; text-align: right; border-bottom: 1px solid #eee; font-size: 14px; }
  th { background: #f0f2f5; font-weight: bold; }
  select, button { font-size: 14px; padding: 6px 10px; margin-left: 6px; }
  form.filters { margin-bottom: 14px; }
"""


def local_lan_ip():
    """
    כתובת ה-IP שהמחשב הזה פונה איתה לרשת המקומית — לא 127.0.0.1/127.0.1.1
    שגם socket.gethostbyname(socket.gethostname()) עלול להחזיר בלינוקס. לא
    שולח בפועל שום דבר ל-8.8.8.8 (UDP connect לא פותח תעבורה) — טריק סטנדרטי
    לגלות את הממשק היוצא בלי תלות בהגדרת /etc/hosts.
    """
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        s.close()


def _connect_readonly(db_path):
    conn = sqlite3.connect(f"file:{db_path}?mode=ro", uri=True, timeout=5)
    conn.row_factory = sqlite3.Row
    return conn


def _fetch_products(db_path):
    conn = _connect_readonly(db_path)
    try:
        return [r["name"] for r in conn.execute("SELECT name FROM products ORDER BY name")]
    finally:
        conn.close()


def _fetch_weighings(db_path, limit=None, since=None, product=None, verdict=None):
    conn = _connect_readonly(db_path)
    try:
        query = "SELECT * FROM weighings"
        clauses, params = [], []
        if since:
            clauses.append("timestamp >= ?")
            params.append(since)
        if product:
            clauses.append("product_name = ?")
            params.append(product)
        if verdict:
            clauses.append("verdict = ?")
            params.append(verdict)
        if clauses:
            query += " WHERE " + " AND ".join(clauses)
        total = conn.execute(f"SELECT COUNT(*) {query[len('SELECT *'):]}", params).fetchone()[0]
        query += " ORDER BY id DESC"
        if limit:
            query += f" LIMIT {int(limit)}"
        rows = conn.execute(query, params).fetchall()
        return rows, total
    finally:
        conn.close()


def _summarize(rows):
    counts = {"green": 0, "red": 0, "yellow": 0, "none": 0, "total": 0}
    weights = []
    for r in rows:
        counts[r["verdict"]] = counts.get(r["verdict"], 0) + 1
        counts["total"] += 1
        weights.append(r["decided_weight"])
    avg = sum(weights) / len(weights) if weights else None
    return counts, avg, (min(weights) if weights else None), (max(weights) if weights else None)


def _summarize_by_product(rows):
    products = {}
    for r in rows:
        name = r["product_name"] or "— ללא מוצר —"
        c = products.setdefault(name, {"green": 0, "red": 0, "yellow": 0, "total": 0})
        c["total"] += 1
        if r["verdict"] in c:
            c[r["verdict"]] += 1
    return products


def _parse_recent_limit(params):
    try:
        limit = int(params.get("limit", [DEFAULT_RECENT_LIMIT])[0])
    except (TypeError, ValueError):
        return DEFAULT_RECENT_LIMIT
    return limit if limit in RECENT_LIMIT_OPTIONS else DEFAULT_RECENT_LIMIT


def _row_to_dict(r):
    """ שורת שקילה כ-dict בטוח ל-JSON, לרינדור מחדש ב-JS (ראו _render_home) —
    התווית/הצבע מחושבים כאן, מקור אחד, כדי שה-JS לא יצטרך עותק משלו של
    SHORT_VERDICT_LABELS/VERDICT_COLORS. """
    return {
        "id": r["id"], "timestamp": r["timestamp"],
        "product_name": r["product_name"] or "— ללא מוצר —",
        "decided_weight": r["decided_weight"],
        "target_weight": r["target_weight"],
        "verdict_label": SHORT_VERDICT_LABELS.get(r["verdict"], r["verdict"]),
        "color": VERDICT_COLORS.get(r["verdict"], "#000"),
    }


def _weighing_row_html(r):
    color = VERDICT_COLORS.get(r["verdict"], "#000")
    target_str = f"{r['target_weight']:.3f}" if r["target_weight"] is not None else "—"
    elapsed_str = f"{r['elapsed_seconds']:.2f}s" if r["elapsed_seconds"] is not None else "—"
    return (f"<tr><td>{r['id']}</td><td>{r['timestamp']}</td>"
           f"<td>{r['product_name'] or '— ללא מוצר —'}</td>"
           f"<td>{r['decided_weight']:.3f}</td><td>{target_str}</td>"
           f"<td style='color:{color}'>{VERDICT_LABELS.get(r['verdict'], r['verdict'])}</td>"
           f"<td>{r['reading_count'] or 0}</td><td>{elapsed_str}</td></tr>")


def _recent_row_html(r):
    """ שורה ל"שקילות אחרונות" בעמוד הבית — תקציר, לא יומן מפורט: בלי
    קריאות/משך, ותוצאה כמילה קצרה אחת (מעל/מתחת/בטווח) בלבד. """
    color = VERDICT_COLORS.get(r["verdict"], "#000")
    target_str = f"{r['target_weight']:.3f}" if r["target_weight"] is not None else "—"
    return (f"<tr><td>{r['id']}</td><td>{r['timestamp']}</td>"
           f"<td>{r['product_name'] or '— ללא מוצר —'}</td>"
           f"<td>{r['decided_weight']:.3f}</td><td>{target_str}</td>"
           f"<td style='color:{color}'>{SHORT_VERDICT_LABELS.get(r['verdict'], r['verdict'])}</td></tr>")


def _page_shell(title, active, body_html, extra_head=""):
    def nav_link(href, label, key):
        cls = ' class="active"' if key == active else ""
        return f'<a href="{href}"{cls}>{label}</a>'

    nav = (nav_link("/", "🏠 בית", "home") + nav_link("/history", "🕘 היסטוריה", "history")
          + nav_link("/reports", "📊 דוחות", "reports"))
    return f"""<!doctype html>
<html dir="rtl" lang="he">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title}</title>
<style>{_CSS}</style>
{extra_head}
</head>
<body>
  <div class="nav">{nav}</div>
  {body_html}
</body>
</html>"""


def _render_home(snapshot, db_path, params):
    connected = snapshot.get("connected", False)
    status_dot = "#2e7d32" if connected else "#9aa5b1"
    status_text = "מחובר" if connected else "מנותק"
    live_weight = snapshot.get("live_weight", "---")
    limit = _parse_recent_limit(params)

    rows, total = _fetch_weighings(db_path, limit=limit)
    counts, _avg, _lo, _hi = _summarize(rows)
    products = _summarize_by_product(rows)

    recent_rows_html = "".join(_recent_row_html(r) for r in rows) or (
        "<tr><td colspan='6' class='muted'>אין שקילות עדיין</td></tr>")
    summary_rows_html = "".join(
        f"<tr><td>{name}</td><td>{c['green']}</td><td>{c['red']}</td>"
        f"<td>{c['yellow']}</td><td>{c['total']}</td></tr>"
        for name, c in sorted(products.items())) or (
        "<tr><td colspan='5' class='muted'>אין נתונים</td></tr>")

    limit_options = "".join(
        f'<option value="{n}"{" selected" if n == limit else ""}>{n}</option>'
        for n in RECENT_LIMIT_OPTIONS)
    note = (f"<div class='muted'>מוצגות {limit} מתוך {total} שקילות בסה\"כ.</div>"
           if total > limit else "")

    body = f"""
  <div class="card status-row"><span class="dot" id="dot" style="background:{status_dot}"></span>
    <span id="status_text">{status_text}</span></div>
  <div class="card">
    <div class="muted">משקל נוכחי</div>
    <div class="weight" id="live_weight">{live_weight}</div>
  </div>
  <div class="tiles">
    <div class="tile"><div class="n" style="color:#c62828" id="t_red">{counts['red']}</div><div class="l">אדום</div></div>
    <div class="tile"><div class="n" style="color:#2e7d32" id="t_green">{counts['green']}</div><div class="l">ירוק</div></div>
    <div class="tile"><div class="n" style="color:#f9a825" id="t_yellow">{counts['yellow']}</div><div class="l">צהוב</div></div>
  </div>
  <div class="card">
    <div class="row-header">
      <h3>שקילות אחרונות</h3>
      <form class="filters" method="get">
        <select name="limit" onchange="this.form.submit()">{limit_options}</select>
        <button type="submit">הצג</button>
      </form>
    </div>
    <div id="recent_note">{note}</div>
    <table><thead><tr><th>#</th><th>זמן</th><th>מוצר</th><th>משקל</th><th>מטרה</th>
      <th>תוצאה</th></tr></thead>
      <tbody id="recent_tbody">{recent_rows_html}</tbody></table>
  </div>
  <div class="card">
    <h3 id="summary_title">סיכום שקילות ({limit} אחרונות)</h3>
    <table><thead><tr><th>מוצר</th><th>ירוק</th><th>אדום</th><th>צהוב</th><th>סה"כ</th></tr></thead>
      <tbody id="summary_tbody">{summary_rows_html}</tbody></table>
  </div>
  <div class="card muted">עודכן: <span id="generated_at">{snapshot.get('generated_at', '')}</span></div>
<script>
const RECENT_LIMIT = {limit};

function cell(text, color) {{
  const td = document.createElement("td");
  td.textContent = text;
  if (color) td.style.color = color;
  return td;
}}

function buildRecentRow(r) {{
  const tr = document.createElement("tr");
  tr.appendChild(cell(r.id));
  tr.appendChild(cell(r.timestamp));
  tr.appendChild(cell(r.product_name));
  tr.appendChild(cell(r.decided_weight.toFixed(3)));
  tr.appendChild(cell(r.target_weight !== null ? r.target_weight.toFixed(3) : "—"));
  tr.appendChild(cell(r.verdict_label, r.color));
  return tr;
}}

function buildSummaryRow(s) {{
  const tr = document.createElement("tr");
  tr.appendChild(cell(s.product_name));
  tr.appendChild(cell(s.green));
  tr.appendChild(cell(s.red));
  tr.appendChild(cell(s.yellow));
  tr.appendChild(cell(s.total));
  return tr;
}}

async function refresh() {{
  try {{
    const r = await fetch("/api/status?limit=" + RECENT_LIMIT);
    const s = await r.json();
    document.getElementById("dot").style.background = s.connected ? "#2e7d32" : "#9aa5b1";
    document.getElementById("status_text").textContent = s.connected ? "מחובר" : "מנותק";
    document.getElementById("live_weight").textContent = s.live_weight;
    document.getElementById("generated_at").textContent = s.generated_at;

    document.getElementById("t_green").textContent = s.totals.green;
    document.getElementById("t_red").textContent = s.totals.red;
    document.getElementById("t_yellow").textContent = s.totals.yellow;

    const recentBody = document.getElementById("recent_tbody");
    recentBody.innerHTML = "";
    if (s.recent.length === 0) {{
      const tr = document.createElement("tr");
      const td = cell("אין שקילות עדיין");
      td.colSpan = 6; td.className = "muted";
      tr.appendChild(td);
      recentBody.appendChild(tr);
    }} else {{
      s.recent.forEach(r => recentBody.appendChild(buildRecentRow(r)));
    }}

    const summaryBody = document.getElementById("summary_tbody");
    summaryBody.innerHTML = "";
    if (s.summary.length === 0) {{
      const tr = document.createElement("tr");
      const td = cell("אין נתונים");
      td.colSpan = 5; td.className = "muted";
      tr.appendChild(td);
      summaryBody.appendChild(tr);
    }} else {{
      s.summary.forEach(row => summaryBody.appendChild(buildSummaryRow(row)));
    }}

    document.getElementById("recent_note").textContent =
      s.recent_total > RECENT_LIMIT ? `מוצגות ${{RECENT_LIMIT}} מתוך ${{s.recent_total}} שקילות בסה"כ.` : "";
  }} catch (e) {{ /* הרשת רגעית לא זמינה — ננסה שוב בטיק הבא, לא מציגים שגיאה */ }}
}}
setInterval(refresh, 4000);
</script>"""
    return _page_shell("תחנת שקילה — בית", "home", body)


def _render_history(db_path, params):
    product = (params.get("product", [""])[0]) or None
    verdict = (params.get("verdict", [""])[0]) or None
    rows, total = _fetch_weighings(db_path, limit=HISTORY_ROW_LIMIT, product=product, verdict=verdict)
    products = _fetch_products(db_path)

    def option(value, label, current):
        sel = " selected" if value == (current or "") else ""
        return f'<option value="{value}"{sel}>{label}</option>'

    product_options = option("", "כל המוצרים", product) + "".join(
        option(p, p, product) for p in products)
    verdict_options = option("", "הכל", verdict) + "".join(
        option(k, v, verdict) for k, v in VERDICT_LABELS.items() if k != "none")

    note = (f"<div class='muted'>מוצגות {HISTORY_ROW_LIMIT} השקילות האחרונות "
           f"מתוך {total} בסינון הנוכחי.</div>" if total > HISTORY_ROW_LIMIT else "")
    rows_html = "".join(_weighing_row_html(r) for r in rows) or (
        "<tr><td colspan='8' class='muted'>אין שקילות בסינון הנוכחי</td></tr>")

    body = f"""
  <div class="card">
    <form class="filters" method="get">
      מוצר: <select name="product">{product_options}</select>
      תוצאה: <select name="verdict">{verdict_options}</select>
      <button type="submit">סנן</button>
    </form>
    {note}
    <table><thead><tr><th>#</th><th>זמן</th><th>מוצר</th><th>משקל</th><th>מטרה</th>
      <th>תוצאה</th><th>קריאות</th><th>משך</th></tr></thead>
      <tbody>{rows_html}</tbody></table>
  </div>"""
    return _page_shell("תחנת שקילה — היסטוריה", "history", body)


def _render_reports(db_path, params):
    product = (params.get("product", [""])[0]) or None
    range_key = params.get("range", ["week"])[0]
    now = datetime.now()
    since_map = {
        "today": now.replace(hour=0, minute=0, second=0, microsecond=0).isoformat(timespec="seconds"),
        "week": (now - timedelta(days=7)).isoformat(timespec="seconds"),
        "month": (now - timedelta(days=30)).isoformat(timespec="seconds"),
        "all": None,
    }
    since = since_map.get(range_key, since_map["week"])

    rows, total = _fetch_weighings(db_path, since=since, product=product)
    counts, avg, lo, hi = _summarize(rows)
    products = _fetch_products(db_path)

    def option(value, label, current):
        sel = " selected" if value == (current or "") else ""
        return f'<option value="{value}"{sel}>{label}</option>'

    product_options = option("", "כל המוצרים", product) + "".join(
        option(p, p, product) for p in products)
    range_options = "".join(
        option(k, v, range_key) for k, v in
        [("today", "היום"), ("week", "7 ימים אחרונים"), ("month", "30 ימים אחרונים"), ("all", "כל הזמן")])

    def pct(n):
        return f" ({n / total * 100:.0f}%)" if total else ""

    avg_str = f"{avg:.3f} kg" if avg is not None else "—"
    range_str = f"{lo:.3f}–{hi:.3f} kg" if lo is not None else "—"

    body = f"""
  <div class="card">
    <form class="filters" method="get">
      מוצר: <select name="product">{product_options}</select>
      טווח: <select name="range">{range_options}</select>
      <button type="submit">סנן</button>
    </form>
  </div>
  <div class="tiles">
    <div class="tile"><div class="n">{total}</div><div class="l">סה"כ שקילות</div></div>
    <div class="tile"><div class="n" style="color:#2e7d32">{counts['green']}{pct(counts['green'])}</div><div class="l">ירוק</div></div>
    <div class="tile"><div class="n" style="color:#c62828">{counts['red']}{pct(counts['red'])}</div><div class="l">אדום</div></div>
    <div class="tile"><div class="n" style="color:#f9a825">{counts['yellow']}{pct(counts['yellow'])}</div><div class="l">צהוב</div></div>
  </div>
  <div class="card">
    <div>משקל ממוצע: <b>{avg_str}</b></div>
    <div>טווח מין–מקס: <b>{range_str}</b></div>
  </div>"""
    return _page_shell("תחנת שקילה — דוחות", "reports", body)


class _Handler(http.server.BaseHTTPRequestHandler):
    get_snapshot = None   # מוגדר ע"י _make_handler_class()
    get_db_path = None

    def log_message(self, format, *args):
        pass   # שקט בקונסול — קיוסק, לא שרת דיבאג

    def _send_html(self, html):
        body = html.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        parsed = urlparse(self.path)
        params = parse_qs(parsed.query)
        db_path = self.get_db_path()
        try:
            if parsed.path == "/api/status":
                limit = _parse_recent_limit(params)
                rows, total = _fetch_weighings(db_path, limit=limit)
                counts, _avg, _lo, _hi = _summarize(rows)
                products = _summarize_by_product(rows)
                payload = dict(self.get_snapshot())
                payload["totals"] = counts
                payload["recent"] = [_row_to_dict(r) for r in rows]
                payload["recent_total"] = total
                payload["summary"] = [{"product_name": name, **c}
                                      for name, c in sorted(products.items())]
                body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
                self.send_response(200)
                self.send_header("Content-Type", "application/json; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            elif parsed.path == "/history":
                self._send_html(_render_history(db_path, params))
            elif parsed.path == "/reports":
                self._send_html(_render_reports(db_path, params))
            else:
                self._send_html(_render_home(self.get_snapshot(), db_path, params))
        except sqlite3.Error as e:
            self.send_response(503)
            body = f"מסד הנתונים לא זמין כרגע: {e}".encode("utf-8")
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)


def _make_handler_class(get_snapshot, get_db_path):
    return type("Handler", (_Handler,),
               {"get_snapshot": staticmethod(get_snapshot),
                "get_db_path": staticmethod(get_db_path)})


class LanDashboardServer:
    """ start()/stop() — ThreadingHTTPServer בתהליכון daemon נפרד, לא חוסם את Tk. """

    def __init__(self, get_snapshot, get_db_path):
        self.get_snapshot = get_snapshot
        self.get_db_path = get_db_path
        self._httpd = None
        self._thread = None

    @property
    def running(self):
        return self._httpd is not None

    def start(self, port: int):
        if self.running:
            return
        handler_cls = _make_handler_class(self.get_snapshot, self.get_db_path)
        self._httpd = http.server.ThreadingHTTPServer(("0.0.0.0", port), handler_cls)
        self._thread = threading.Thread(target=self._httpd.serve_forever, daemon=True)
        self._thread.start()

    def stop(self):
        if not self.running:
            return
        self._httpd.shutdown()
        self._httpd.server_close()
        self._httpd = None
        self._thread = None
