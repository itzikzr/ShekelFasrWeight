"""
מסך הבית — מסוע עובד כל הזמן; כשמתגלה משקל מעל הסף האפליקציה ממצעת ורושמת תוצאה.

זהו המקום היחיד שנוגע ב-Tk וב-DB בעקבות אירועי engine — התהליכונים ב-engine.py
דוחפים (kind, payload) ל-ui_queue, וכאן (בתהליכון הראשי, דרך _pump_queue) אנחנו
קוראים את מזהה המוצר הנבחר-כרגע, מסווגים רמזור, וכותבים ל-DB. כך בחירת מוצר יכולה
להשתנות בין שקילה לשקילה בלי שתהליכון הרקע ידע על Tk בכלל.
"""

import json
import queue
import threading
import tkinter as tk
from datetime import datetime, timedelta
from tkinter import ttk, messagebox
from pathlib import Path

from . import backup as backup_mod
from . import db, rtl, theme
from . import lan_dashboard, update_check
from .engine import WeighingEngine, decide_weight
from .relay_engine import RelayEngine
from .formatting import fmt_weight
from .widgets import ColorBar, StatusPill
from .settings_window import SettingsWindow
from .products_window import ProductsWindow
from .history_window import HistoryWindow
from .weighing_detail_window import WeighingDetailWindow
from .reports_window import ReportsWindow
from .version import __version__

CONFIG_FILE = Path(__file__).resolve().parent.parent / "scale_sampler_config.json"
DEFAULT_BACKUP_DIR = Path(__file__).resolve().parent.parent / "backups"

# בדיקת "יש גיבוי לבצע?" מרוצת גם תוך כדי ריצה (לא רק בעלייה) — קיוסק יכול
# לרוץ ברצף כמה ימים בלי להיסגר, ואז רק בדיקה חוזרת תופסת את חצות היום החדש.
BACKUP_CHECK_INTERVAL_MS = 60 * 60 * 1000

# בדיוק אותו טעם כמו BACKUP_CHECK_INTERVAL_MS — קיוסק שרץ ברצף כמה ימים.
UPDATE_CHECK_INTERVAL_MS = 60 * 60 * 1000
DEFAULT_LAN_DASHBOARD_PORT = 8090

NO_PRODUCT_LABEL = "— ללא מוצר —"

VERDICT_TEXT = {
    "green":  "✓ בטווח",
    "red":    "⚠ מעל הטווח",
    "yellow": "⚠ מתחת לטווח",
    "none":   "—",
}

# סינון "שקילות אחרונות" לפי טווח זמן — (מפתח פנימי, תווית תצוגה).
RECENT_FILTERS = [
    ("today", "היום"),
    ("week", "השבוע"),
    ("session", "מאז ההפעלה"),
]
RECENT_FILTER_LABEL_TO_KEY = {label: key for key, label in RECENT_FILTERS}


class MainWindow:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title(f"Swan  v{__version__}")
        theme.set_app_icon(root)

        try:
            _early_cfg = json.loads(CONFIG_FILE.read_text(encoding="utf-8"))
        except Exception:
            _early_cfg = {}
        self.dark_mode = bool(_early_cfg.get("dark_mode", False))
        self.fonts = theme.set_mode(root, "dark" if self.dark_mode else "light")
        self.root.minsize(980, 640)

        db.connect()

        self.ui_queue: "queue.Queue" = queue.Queue()
        self.engine = WeighingEngine(self.ui_queue)
        self.relay_engine = RelayEngine(self.ui_queue)
        # כניסה1/כניסה2 בכרטיס הממסרים יכולות לקבוע התחלה/סיום שקילה, לפי
        # start_trigger/stop_trigger שנבחרו בהגדרות — ראו _sync_relay_sensor_mode()
        # (נקרא מכל שינוי בהגדרות הממסרים ומכל connect/disconnect שלהם) ו-
        # engine.EngineSettings.start_trigger/stop_trigger. RelayEngine מדווח
        # את כל ארבעת המעברים הגולמיים; WeighingEngine מסנן לפי ההגדרה הנוכחית.
        self.relay_engine.on_input1_close = self.engine.on_sensor1_close
        self.relay_engine.on_input1_open = self.engine.on_sensor1_open
        self.relay_engine.on_input2_close = self.engine.on_sensor2_close
        self.relay_engine.on_input2_open = self.engine.on_sensor2_open
        self._relay_motor_on = False

        self.current_product_id = None
        self._product_name_to_id = {}
        self._display_to_name = {}

        self._app_start_time = datetime.now()
        self._recent_filter = "today"
        self._recent_cleared_at = None

        self.settings_window = None
        self.products_window = None
        self.history_window = None
        self.reports_window = None

        # דשבורד רשת מקומית — ראו lan_dashboard.py. הסנאפשוט מתעדכן בהצבה
        # שלמה של dict חדש (לא מוטציה במקום) כך שקריאה מתהליכון ה-HTTP
        # (אחר) בטוחה בלי lock, בדיוק כמו EngineSettings/דגלי הממסרים — שמור
        # רק ל"חיבור"/"משקל חי" (מצב זיכרון, לא ב-DB בכלל). היסטוריה/דוחות/
        # שקילות אחרונות בדשבורד נקראים ישירות מה-DB (חיבור sqlite נפרד,
        # readonly, ראו lan_dashboard._connect_readonly) — לא מהסנאפשוט הזה.
        self._dashboard_snapshot = {"connected": False, "live_weight": "---", "generated_at": ""}
        self.lan_dashboard = lan_dashboard.LanDashboardServer(
            lambda: self._dashboard_snapshot, lambda: db.DB_FILE)

        # בדיקת עדכון גרסה מרוחקת — ראו update_check.py. לא מבוצע pull
        # אוטומטית לעולם, רק בדיקה + הצגת תזכורת (ראו _sync_update_indicator).
        self._update_status = {"ok": None}

        self._build_ui()
        self._load_settings()
        self._sync_relay_ui()
        if self.lan_dashboard_enabled:
            self.start_lan_dashboard()
        self.refresh_products()
        self._refresh_recent_list()
        self._pump_queue()
        self._backup_tick()
        self._update_check_tick()

        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    # ──────────────────────────────────────────────
    # UI
    # ──────────────────────────────────────────────

    def _build_ui(self):
        f = self.fonts
        outer = ttk.Frame(self.root)
        outer.pack(fill="both", expand=True, padx=14, pady=14)

        # ── כותרת עליונה ──
        header = ttk.Frame(outer, style="Card.TFrame")
        header.pack(fill="x", pady=(0, 12))
        header.configure(padding=12)

        # במסך מלא (theme.maximize) אין שורת כותרת עם כפתור סגירה של Windows —
        # לכן חייבים כפתור סגירה משלנו, בפינה שבה משתמשים מצפים למצוא אותו.
        ttk.Button(header, text="✕ סגור", width=8,
                  command=self._on_close).pack(side="right", padx=(8, 0))

        brand_frame = ttk.Frame(header, style="Card.TFrame")
        brand_frame.pack(side="right", padx=8)

        self._logo_image = tk.PhotoImage(file=str(theme.ASSETS_DIR / "logo.png"))
        self.logo_label = tk.Label(brand_frame, image=self._logo_image, background=theme.CARD_BG)
        self.logo_label.pack(side="right")

        # סטטוס חיבור + גרסה זה מתחת לזה (לא ליד הלוגו) — מפנה מקום ברוחב
        # הכותרת, בין השאר כדי ש"▶ התחל" (כשממסרים מסומנים) יהיה לו מקום.
        status_frame = ttk.Frame(header, style="Card.TFrame")
        status_frame.pack(side="right", padx=(0, 16))

        self.status_pill = StatusPill(status_frame)
        self.status_pill.pack(anchor="e")
        self.status_pill.set("מנותק", theme.IDLE_GRAY)

        self.version_label = tk.Label(status_frame, text=f"גרסה {__version__}",
                                      font=(f["sans"], 8), background=theme.CARD_BG,
                                      foreground=theme.TEXT_MUTED)
        self.version_label.pack(anchor="e")

        # לא נארז (pack) כאן בכוונה — מוצג רק כשמתגלה עדכון, ראו _sync_update_indicator.
        self.update_indicator = tk.Label(status_frame, text="🔔 עדכון זמין", cursor="hand2",
                                         font=(f["sans"], 8, "bold"), background=theme.CARD_BG,
                                         foreground=theme.ACCENT)
        self.update_indicator.bind("<Button-1>", lambda e: self.open_settings())

        self.btn_connect_toggle = ttk.Button(header, text="התחבר", style="Accent.TButton",
                                             command=self._on_connect_click)
        self.btn_connect_toggle.pack(side="right", padx=(0, 16))

        # מצב כהה/בהיר עבר להגדרות (טאב "חיבור") — ראו SettingsWindow, כדי
        # לפנות מקום בכותרת. _toggle_theme() נשאר כאן, רק לא כפתור בכותרת.

        btns = ttk.Frame(header, style="Card.TFrame")
        btns.pack(side="left", padx=4)
        ttk.Button(btns, text="↺ איפוס", command=self._on_zero_click).pack(side="left", padx=3)
        ttk.Button(btns, text="📦 מוצרים", command=self.open_products).pack(side="left", padx=3)
        ttk.Button(btns, text="📊 דוחות", command=self.open_reports).pack(side="left", padx=3)
        ttk.Button(btns, text="⚙ הגדרות", command=self.open_settings).pack(side="left", padx=3)
        # לא נארז (pack) כאן בכוונה — מוצג רק כש"השתמש בממסרים" מסומן,
        # ראו _sync_relay_ui() (נקרא אחרי _load_settings ובכל שינוי הגדרה).
        self.btn_relay_motor = ttk.Button(btns, text="▶ התחל", command=self._on_relay_motor_toggle)

        # ── תוכן: שקילות אחרונות משמאל (מלא לגובה) | משקל+רמזור מימין (מלא לגובה) ──
        content = ttk.Frame(outer)
        content.pack(fill="both", expand=True)
        content.rowconfigure(0, weight=1)
        content.columnconfigure(0, weight=2)   # שמאל — שקילות אחרונות
        content.columnconfigure(1, weight=3)   # ימין — משקל + רמזור

        # ── ימין: משקל + רמזור, ממורכזים ומלאים לגובה ──
        RIGHT_PANEL_WIDTH = 420   # קבוע בכוונה, ראו ההערה למטה

        right_panel = ttk.Frame(content, style="Card.TFrame", width=RIGHT_PANEL_WIDTH)
        right_panel.grid(row=0, column=1, sticky="nsew", padx=(8, 0))
        # מבטל את התפשטות הגודל מהתוכן: בלי זה, הודעת סטטוס ארוכה (למשל "מתחת
        # למינימום השמירה — לא נשמר") הייתה מגדילה את הרוחב הטבעי של right_panel,
        # וה-grid החוצה היה "גונב" רוחב מהעמודה השנייה (שקילות אחרונות) — כך
        # שגודל הטבלה היה משתנה בכל שינוי טקסט. הרוחב נשאר קבוע; sticky="nsew"
        # עדיין מותח אותו לגובה המלא.
        right_panel.pack_propagate(False)

        # pack() ולא place() בכוונה: place() לא מדווח את הגודל הטבעי שלו להורה,
        # כך שה-grid החוצה היה מקצה ל-right_panel שטח מזערי ותוכן הפנים (המשקל
        # בגופן גדול) היה נחתך מעבר לגבול החלון. pack מדווח נכון את הגודל הנדרש
        # כלפי מעלה. לא expand=True יותר — מרותק לראש הפאנל כדי שטבלת סיכום
        # המוצרים למטה (ראו summary_frame) תקבל את שאר השטח הפנוי.
        right_inner = ttk.Frame(right_panel, style="Card.TFrame")
        right_inner.pack(pady=(12, 8))

        # אין יותר כיתוב "משקל נוכחי" מעל המשקל — הוסר כדי שהמשקל יתפוס את
        # החלק העליון של הפאנל (ראו גם ה-pady המוקטן של right_inner לעיל).
        self.weight_var = tk.StringVar(value="---")
        self.weight_label = tk.Label(right_inner, textvariable=self.weight_var,
                                     font=(f["mono"], 56, "bold"),
                                     background=theme.CARD_BG, foreground=theme.ACCENT)
        self.weight_label.pack()

        # מלבן צבעוני רחב (בערך ברוחב תצוגת המשקל, מוגדל קצת מהגרסה הקודמת)
        # שמחליף את הרמזור התלת-נורתי — הצבע *וגם* תוצאת השקילה (VERDICT_TEXT)
        # מוצגים בתוכו. status_line_var נשאר קיים ומתעדכן (משמש מקומות אחרים
        # בעתיד/דיבוג) אבל לא מוצג יותר.
        self.traffic = ColorBar(right_inner, width=RIGHT_PANEL_WIDTH - 40, height=50)
        self.traffic.pack(pady=18)

        self.status_line_var = tk.StringVar(value="מנותק — פתח הגדרות להתחברות")

        # ── סיכום מוצרים: אותו טווח/מסנן זמן כמו "שקילות אחרונות" משמאל ──
        summary_frame = ttk.Frame(right_panel, style="Card.TFrame")
        summary_frame.pack(fill="both", expand=True, padx=16, pady=(0, 16))
        summary_frame.columnconfigure(0, weight=1)
        summary_frame.rowconfigure(1, weight=1)

        ttk.Label(summary_frame, text="סיכום מוצרים", style="Card.TLabel",
                  font=(f["sans"], 11, "bold")).grid(row=0, column=0, sticky="w", pady=(0, 6))

        summary_cols = ("#", "מוצר", "תקין", "מעל", "מתחת", 'סה"כ')
        self.product_summary_tree = ttk.Treeview(summary_frame, columns=summary_cols,
                                                 show="headings", height=5)
        summary_widths = {"#": 32, "מוצר": 170}
        for c in summary_cols:
            self.product_summary_tree.heading(c, text=c)
            self.product_summary_tree.column(c, width=summary_widths.get(c, 55),
                                             anchor="center", stretch=True)
        self.product_summary_tree.grid(row=1, column=0, sticky="nsew")

        summary_sb = ttk.Scrollbar(summary_frame, orient="vertical",
                                   command=self.product_summary_tree.yview)
        self.product_summary_tree.configure(yscrollcommand=summary_sb.set)
        summary_sb.grid(row=1, column=1, sticky="ns")

        # ── שמאל: שקילות אחרונות, מלא לגובה ──
        recent = ttk.LabelFrame(content, text="שקילות אחרונות")
        recent.grid(row=0, column=0, sticky="nsew", padx=(0, 8))
        recent.columnconfigure(0, weight=1)
        recent.rowconfigure(1, weight=1)

        toolbar = ttk.Frame(recent)
        toolbar.grid(row=0, column=0, columnspan=2, sticky="ew", padx=8, pady=(8, 0))
        ttk.Button(toolbar, text="נקה תצוגה", command=self._on_clear_recent_display).pack(side="left")

        self.product_var = tk.StringVar(value=NO_PRODUCT_LABEL)
        self.product_combo = ttk.Combobox(toolbar, textvariable=self.product_var,
                                          state="readonly", width=18)
        self.product_combo.pack(side="left", padx=(10, 6))
        self.product_combo.bind("<<ComboboxSelected>>", self._on_product_selected)
        ttk.Label(toolbar, text="מוצר:").pack(side="left")

        # Same display/logical split as product_combo (see rtl.py): the combobox
        # shows bidi-reordered labels, but the filter key lookup must use the
        # original label — translate the read-back selection through the map.
        display_filter_values = [rtl.visual(label) for _, label in RECENT_FILTERS]
        self._recent_display_to_key = {
            rtl.visual(label): key for key, label in RECENT_FILTERS}
        self.recent_filter_var = tk.StringVar(
            value=rtl.visual(dict(RECENT_FILTERS).get(self._recent_filter, RECENT_FILTERS[0][1])))
        self.recent_filter_combo = ttk.Combobox(
            toolbar, textvariable=self.recent_filter_var, state="readonly", width=14,
            values=display_filter_values)
        self.recent_filter_combo.pack(side="right")
        self.recent_filter_combo.bind("<<ComboboxSelected>>", self._on_recent_filter_changed)
        ttk.Label(toolbar, text="הצג:").pack(side="right", padx=(0, 4))

        cols = ("#", "שעה", "מוצר", "משקל (kg)", "תוצאה", "קריאות", "משך")
        self.recent_tree = ttk.Treeview(recent, columns=cols, show="headings")
        for c, w, anchor in (("#", 45, "center"), ("שעה", 90, "center"), ("מוצר", 160, "e"),
                             ("משקל (kg)", 100, "e"), ("תוצאה", 130, "center"),
                             ("קריאות", 70, "center"), ("משך", 70, "center")):
            self.recent_tree.heading(c, text=c)
            self.recent_tree.column(c, width=w, anchor=anchor, stretch=(c == "מוצר"))
        self.recent_tree.grid(row=1, column=0, sticky="nsew", padx=(8, 0), pady=8)
        self.recent_tree.bind("<Double-1>", self._on_recent_double_click)

        sb = ttk.Scrollbar(recent, orient="vertical", command=self.recent_tree.yview)
        self.recent_tree.configure(yscrollcommand=sb.set)
        sb.grid(row=1, column=1, sticky="ns", pady=8)

        for verdict, (fg, bg) in theme.VERDICT_COLORS.items():
            self.recent_tree.tag_configure(verdict, background=bg, foreground=fg)

        footer = ttk.Frame(recent)
        footer.grid(row=2, column=0, columnspan=2, sticky="e", padx=8, pady=(0, 8))
        ttk.Button(footer, text="הצג את כל ההיסטוריה →", command=self.open_history).pack()

    # ──────────────────────────────────────────────
    # Theme (בהיר/כהה)
    # ──────────────────────────────────────────────

    def _toggle_theme(self):
        """ נקרא מתיבת הסימון "מצב כהה" בהגדרות (טאב "חיבור"), לא מכפתור בכותרת. """
        self.dark_mode = not self.dark_mode
        self.fonts = theme.set_mode(self.root, "dark" if self.dark_mode else "light")
        self.save_settings()
        self._refresh_theme_widgets()

    def _refresh_theme_widgets(self):
        """ מעדכן ווידג'טי tk גולמיים שלא מתעדכנים אוטומטית ע"י ה-ttk style. """
        for label in (self.logo_label, self.version_label, self.update_indicator,
                      self.weight_label):
            label.configure(background=theme.CARD_BG)
        self.version_label.configure(foreground=theme.TEXT_MUTED)
        self.update_indicator.configure(foreground=theme.ACCENT)
        self.weight_label.configure(foreground=theme.ACCENT)

        self.status_pill.refresh_theme()
        self.status_pill.set(*(("מחובר", theme.GREEN) if self.engine.connected
                               else ("מנותק", theme.IDLE_GRAY)))
        self.traffic.refresh_theme()

        for verdict, (fg, bg) in theme.VERDICT_COLORS.items():
            self.recent_tree.tag_configure(verdict, background=bg, foreground=fg)

        for win in (self.settings_window, self.products_window, self.history_window,
                   self.reports_window):
            if win is not None and win.winfo_exists():
                win.refresh_theme()

    # ──────────────────────────────────────────────
    # Settings persistence
    # ──────────────────────────────────────────────

    def _load_settings(self):
        try:
            cfg = json.loads(CONFIG_FILE.read_text(encoding="utf-8"))
        except Exception:
            cfg = {}
        self.port = cfg.get("port", "")
        # "or" ולא .get(key, default) בכוונה: אם הקובץ מכיל "baud": "" (ריק, לא
        # חסר) — למשל אחרי כתיבה חלקית/פגומה — .get היה מחזיר את הריק כי המפתח
        # קיים, וה-Combobox היה נראה ריק ו-int(baud) ב-connect() היה קורס
        # (ValueError: invalid literal for int() with base 10: ''). נצפה בפועל.
        self.baud = cfg.get("baud") or "9600"
        self.engine.settings.listen = cfg.get("listen", True)
        self.engine.settings.threshold = cfg.get("threshold", 0.5)
        self.engine.settings.debounce_s = cfg.get("debounce_s", 0.4)
        self.engine.settings.show_each = cfg.get("show_each", True)
        self.engine.settings.min_save_weight = cfg.get("min_save_weight", 0.0)
        self.sample_duration = cfg.get("sample_duration", 1.0)
        self.live_window = cfg.get("live_window", 1.0)
        self.current_product_id = cfg.get("product_id")
        self.dark_mode = bool(cfg.get("dark_mode", self.dark_mode))
        self.relay_enabled = bool(cfg.get("relay_enabled", False))
        self.relay_port = cfg.get("relay_port", "")
        self.relay_baud = cfg.get("relay_baud") or "19200"
        # migration מה-checkbox הבינארי הישן (relay_use_inputs) לשני צירים
        # עצמאיים (start_trigger/stop_trigger) — אם המפתח הישן קיים וערכו
        # False, זה "לא ממסרים לצורך התחלה/סיום, רק סף משקל עם armed-gate"
        # (require_armed הישן); בכל מקרה אחר (כולל קונפיג שנשמר לפני שהצ'קבוקס
        # הזה נוסף בכלל) — מצב החיישנים המלא שהיה קיים מאז ומתמיד, כמו קודם.
        old_use_inputs = cfg.get("relay_use_inputs")  # None אם לא הוגדר אף פעם
        default_start, default_stop = ("sensor1_open", "sensor2_close")
        if old_use_inputs is False:
            default_start, default_stop = ("threshold", "threshold")
        self.start_trigger = cfg.get("start_trigger", default_start)
        self.stop_trigger = cfg.get("stop_trigger", default_stop)
        self.relay_engine.settings.sort_pulse_seconds = cfg.get("relay_sort_pulse_seconds", 2.0)
        self.backup_enabled = bool(cfg.get("backup_enabled", False))
        self.backup_dir = cfg.get("backup_dir") or str(DEFAULT_BACKUP_DIR)
        self.backup_last_date = cfg.get("backup_last_date")   # "YYYY-MM-DD" או None
        self.backup_last_error = None   # לא מתמיד — רק לתצוגה בטאב "גיבוי" בזמן ריצה
        self.lan_dashboard_enabled = bool(cfg.get("lan_dashboard_enabled", False))
        self.lan_dashboard_port = int(cfg.get("lan_dashboard_port") or DEFAULT_LAN_DASHBOARD_PORT)
        self.update_check_enabled = bool(cfg.get("update_check_enabled", False))
        self.update_last_check_date = cfg.get("update_last_check_date")   # "YYYY-MM-DD" או None

    def save_settings(self):
        cfg = {
            "port": self.port,
            "baud": self.baud,
            "listen": self.engine.settings.listen,
            "threshold": self.engine.settings.threshold,
            "debounce_s": self.engine.settings.debounce_s,
            "show_each": self.engine.settings.show_each,
            "min_save_weight": self.engine.settings.min_save_weight,
            "sample_duration": self.sample_duration,
            "live_window": self.live_window,
            "product_id": self.current_product_id,
            "dark_mode": self.dark_mode,
            "relay_enabled": self.relay_enabled,
            "relay_port": self.relay_port,
            "relay_baud": self.relay_baud,
            "start_trigger": self.start_trigger,
            "stop_trigger": self.stop_trigger,
            "relay_sort_pulse_seconds": self.relay_engine.settings.sort_pulse_seconds,
            "backup_enabled": self.backup_enabled,
            "backup_dir": self.backup_dir,
            "backup_last_date": self.backup_last_date,
            "lan_dashboard_enabled": self.lan_dashboard_enabled,
            "lan_dashboard_port": self.lan_dashboard_port,
            "update_check_enabled": self.update_check_enabled,
            "update_last_check_date": self.update_last_check_date,
        }
        try:
            CONFIG_FILE.write_text(json.dumps(cfg, indent=2, ensure_ascii=False), encoding="utf-8")
        except Exception:
            pass

    # ──────────────────────────────────────────────
    # Backup אוטומטי יומי — ראו backup.py למנגנון עצמו (sqlite backup API + גיזום)
    # ──────────────────────────────────────────────

    def _backup_tick(self):
        """ נקרא בעלייה ואז מחדש כל BACKUP_CHECK_INTERVAL_MS — ראו הערה ליד
        הקבוע: קיוסק יכול לרוץ ברצף כמה ימים בלי להיסגר, אז רק בדיקה חוזרת
        תוך כדי ריצה (לא רק __init__) תופסת את חצות היום החדש. """
        self._maybe_run_backup()
        self.root.after(BACKUP_CHECK_INTERVAL_MS, self._backup_tick)

    def _maybe_run_backup(self):
        """ מריץ גיבוי לכל היותר פעם ביום קלנדרי (לפי backup_last_date השמור),
        רק אם backup_enabled. """
        if not self.backup_enabled:
            return
        if self.backup_last_date == datetime.now().date().isoformat():
            return
        self.run_backup_now()

    def run_backup_now(self):
        """ מריץ גיבוי מיידית (גם מכפתור "גבה עכשיו" בהגדרות, גם מ-_maybe_run_backup)
        ומעדכן backup_last_date/backup_last_error — כך שגיבוי ידני היום מונע גם
        את הריצה האוטומטית הכפולה של אותו יום. מחזיר (ok: bool, message: str).
        כשל לא מציג הודעה בעצמו ולא מעדכן backup_last_date (כדי שהבדיקה הבאה
        תנסה שוב במקום לדלג יום שלם) — רק שומר ל-backup_last_error לתצוגה
        בטאב "גיבוי" בהגדרות; ההודעה המוחזרת מוצגת ע"י הקורא (הכפתור הידני). """
        try:
            dest_path = backup_mod.run_backup(Path(self.backup_dir))
        except Exception as e:
            self.backup_last_error = str(e)
            return False, f"גיבוי נכשל:\n{e}"
        self.backup_last_error = None
        self.backup_last_date = datetime.now().date().isoformat()
        self.save_settings()
        return True, f"נשמר אל:\n{dest_path}"

    # ──────────────────────────────────────────────
    # דשבורד רשת מקומית — ראו lan_dashboard.py
    # ──────────────────────────────────────────────

    def start_lan_dashboard(self):
        self.lan_dashboard.start(self.lan_dashboard_port)

    def stop_lan_dashboard(self):
        self.lan_dashboard.stop()

    def _update_dashboard_snapshot(self, **overrides):
        """
        מחליף את self._dashboard_snapshot ב-dict *חדש* לגמרי (לא מוטציה של
        הקיים) — כך שתהליכון ה-HTTP (lan_dashboard.py, קורא בלי lock) לעולם
        לא רואה מצב חצי-מעודכן: הוא תמיד קורא או את כל ה-dict הישן, או את כל
        החדש, בלי מצב אמצעי אפשרי בפייתון עם GIL.
        """
        snap = dict(self._dashboard_snapshot)
        snap.update(overrides)
        snap["generated_at"] = datetime.now().strftime("%H:%M:%S")
        self._dashboard_snapshot = snap

    # ──────────────────────────────────────────────
    # בדיקת עדכון גרסה מרוחקת — ראו update_check.py למנגנון (git fetch/pull)
    # ──────────────────────────────────────────────

    def _update_check_tick(self):
        self._maybe_check_for_update()
        self.root.after(UPDATE_CHECK_INTERVAL_MS, self._update_check_tick)

    def _maybe_check_for_update(self):
        if not self.update_check_enabled:
            return
        if self.update_last_check_date == datetime.now().date().isoformat():
            return
        self.check_for_update_now()

    def check_for_update_now(self):
        """ רץ בתהליכון נפרד — git fetch הוא רשת, ותקיעה כאן הייתה מקפיאה
        את כל ה-UI (בדיוק כמו שכל I/O טורי/רשת באפליקציה הזו רץ מחוץ
        לתהליכון הראשי). התוצאה חוזרת דרך ui_queue, כרגיל. """
        threading.Thread(target=self._check_for_update_worker, daemon=True).start()

    def _check_for_update_worker(self):
        result = update_check.check_for_update()
        self.ui_queue.put(("update_check_result", result))

    def _on_update_check_result(self, result):
        self._update_status = result
        if result.get("ok"):
            # מסמנים "נבדק היום" רק בהצלחה — כשל (למשל אין אינטרנט) מנסה
            # שוב בבדיקה הבאה במקום לדלג יום שלם, בדיוק כמו _maybe_run_backup.
            self.update_last_check_date = datetime.now().date().isoformat()
            self.save_settings()
        self._sync_update_indicator()
        if self.settings_window is not None and self.settings_window.winfo_exists():
            self.settings_window.refresh_update_status()

    def _sync_update_indicator(self):
        show = bool(self._update_status.get("ok") and self._update_status.get("update_available"))
        if show:
            if not self.update_indicator.winfo_ismapped():
                self.update_indicator.pack(anchor="e")
        else:
            if self.update_indicator.winfo_ismapped():
                self.update_indicator.pack_forget()

    def apply_update_now(self):
        """ נקרא רק מלחיצה מפורשת על "עדכן" בהגדרות — לעולם לא אוטומטית.
        רץ בתהליכון נפרד (git pull הוא גם רשת), תוצאה חוזרת דרך ui_queue. """
        threading.Thread(target=self._apply_update_worker, daemon=True).start()

    def _apply_update_worker(self):
        result = update_check.apply_update()
        self.ui_queue.put(("update_apply_result", result))

    def _on_update_apply_result(self, result):
        if self.settings_window is not None and self.settings_window.winfo_exists():
            self.settings_window.handle_update_apply_result(result)
        if result.get("ok"):
            # git pull הצליח — מרעננים את הבדיקה כדי שהתזכורת בכותרת תיעלם.
            self.check_for_update_now()

    # ──────────────────────────────────────────────
    # Connection (מהכפתור במסך הראשי או ממסך ההגדרות)
    # ──────────────────────────────────────────────

    def _on_connect_click(self):
        if self.engine.connected:
            self.disconnect()
            return
        if not self.port:
            messagebox.showinfo("התחברות", "יש לבחור פורט בהגדרות לפני ההתחברות.")
            self.open_settings()
            return
        try:
            self.connect(self.port, self.baud)
        except Exception as e:
            messagebox.showerror("שגיאת חיבור", str(e))

    def _sync_connect_button(self):
        self.btn_connect_toggle.config(text="התנתק" if self.engine.connected else "התחבר")

    def connect(self, port_name: str, baud: str):
        self.engine.connect(port_name, int(baud))
        self.port, self.baud = port_name, baud
        self.save_settings()
        self.status_pill.set("מחובר", theme.GREEN)
        self.status_line_var.set("מחכה למשקל...")
        self._sync_connect_button()
        self._update_dashboard_snapshot(connected=True)
        self.engine.start_monitor()

    def disconnect(self):
        self.engine.disconnect()
        self.status_pill.set("מנותק", theme.IDLE_GRAY)
        self.status_line_var.set("מנותק — פתח הגדרות להתחברות")
        self.weight_var.set("---")
        self.traffic.set_state("none")
        self._sync_connect_button()
        self._update_dashboard_snapshot(connected=False, live_weight="---")

    # ──────────────────────────────────────────────
    # Relays (כרטיס IA-3116-U2i — מנוע המסוע + חיישני התחלה/סיום שקילה +
    # מיון) — חיבור סיריאלי נפרד ועצמאי מהמשקל, ראו relay_engine.py.
    # ──────────────────────────────────────────────

    def connect_relays(self, port_name: str, baud: str):
        self.relay_engine.connect(port_name, int(baud))
        self.relay_port, self.relay_baud = port_name, baud
        self.save_settings()
        self._sync_relay_sensor_mode()
        self._sync_relay_ui()

    def disconnect_relays(self):
        self.relay_engine.disconnect()
        self._relay_motor_on = False
        self._sync_relay_sensor_mode()
        self._sync_relay_ui()

    def _sync_relay_sensor_mode(self):
        """
        active = "השתמש בממסרים" מסומן *וגם* הממסרים בפועל מחוברים — "עובד
        אז מחליף, לא עובד אז כמו קודם" כפי שהתבקש. כש-active, ה-start_trigger/
        stop_trigger שנבחרו בהגדרות (טאב "ממסרים") מועברים כמו שהם ל-engine
        (שלוש אפשרויות עצמאיות לכל אחד: "threshold"/"sensor1_close"/
        "sensor1_open" להתחלה, "threshold"/"sensor2_close"/"sensor2_open"
        לסיום — ראו engine.EngineSettings ו-engine._monitor_loop). כש-not
        active, שניהם נכפים חזרה ל-"threshold" בלי קשר לבחירה השמורה, בדיוק
        כמו לפני שהתכונה הזו נוספה. נקרא מכל שינוי בכל אחד מהתנאים האלה.
        """
        active = self.relay_enabled and self.relay_engine.connected
        self.engine.settings.relay_active = active
        self.engine.settings.start_trigger = self.start_trigger if active else "threshold"
        self.engine.settings.stop_trigger = self.stop_trigger if active else "threshold"
        self.engine.settings.armed = self._relay_motor_on

    def _sync_relay_ui(self):
        """ כפתור המסוע מוצג רק כש"השתמש בממסרים" מסומן — כשלא, המסך נראה
        בדיוק כמו לפני התכונה הזו בכלל. """
        if self.relay_enabled:
            if not self.btn_relay_motor.winfo_ismapped():
                self.btn_relay_motor.pack(side="left", padx=3)
        else:
            if self.btn_relay_motor.winfo_ismapped():
                self.btn_relay_motor.pack_forget()
        self._sync_relay_motor_button()

    def _sync_relay_motor_button(self):
        self.btn_relay_motor.config(text="⏹ עצור" if self._relay_motor_on else "▶ התחל")

    def _on_relay_motor_toggle(self):
        """
        לחיצה על "התחל" (לא על "עצור") מתחברת אוטומטית את שני ההתקנים אם
        עוד לא מחוברים — גם המשקל עצמו (self.connect, בדיוק כמו כפתור
        "התחבר" בכותרת) וגם כרטיס הממסרים (self.connect_relays) — במקום רק
        להציג הודעת שגיאה ולהפנות להגדרות, "לחיצה על התחל מחברת אותם" כפי
        שהתבקש. אם אין port שמור בכלל להתקן מסוים, או שהחיבור עצמו נכשל,
        עדיין מפנים להגדרות/מציגים שגיאה (אין מה להתחבר אליו) ולא מתקדמים
        להפעלת הממסר. לא נוגעים בחיבור הקיים כשמדובר בלחיצת "עצור".
        """
        turning_on = not self._relay_motor_on
        if turning_on:
            if not self.engine.connected:
                if not self.port:
                    messagebox.showinfo("התחברות", "יש לבחור פורט למשקל בהגדרות לפני ההפעלה.")
                    self.open_settings()
                    return
                try:
                    self.connect(self.port, self.baud)
                except Exception as e:
                    messagebox.showerror("שגיאת חיבור", f"החיבור למשקל נכשל:\n{e}")
                    return
            if not self.relay_engine.connected:
                if not self.relay_port:
                    messagebox.showinfo("ממסרים", "יש להגדיר פורט לכרטיס הממסרים בהגדרות לפני ההפעלה.")
                    self.open_settings()
                    return
                try:
                    self.connect_relays(self.relay_port, self.relay_baud)
                except Exception as e:
                    messagebox.showerror("ממסרים", f"החיבור לכרטיס הממסרים נכשל:\n{e}")
                    return
        self._relay_motor_on = not self._relay_motor_on
        self.relay_engine.set_relay(1, self._relay_motor_on)
        self._sync_relay_sensor_mode()
        self._sync_relay_motor_button()

    # ──────────────────────────────────────────────
    # Products
    # ──────────────────────────────────────────────

    def refresh_products(self):
        products = db.list_products()
        self._product_name_to_id = {p["name"]: p["id"] for p in products}
        values = [NO_PRODUCT_LABEL] + [p["name"] for p in products]
        # Combobox values are rendered by Tk itself (not routed through
        # rtl.patch()'s "text" hook), so they need bidi-reordering here —
        # but self.product_var / self._product_name_to_id must keep the
        # original (logical) names, since _on_product_selected looks the
        # selection up by exact string match. _display_to_name bridges the
        # two: a no-op dict on Windows, a real reverse-map on Linux.
        display_values = [rtl.visual(v) for v in values]
        self._display_to_name = dict(zip(display_values, values))
        self.product_combo.config(values=display_values)

        name_by_id = {p["id"]: p["name"] for p in products}
        if self.current_product_id in name_by_id:
            self.product_var.set(rtl.visual(name_by_id[self.current_product_id]))
        else:
            self.current_product_id = None
            self.product_var.set(rtl.visual(NO_PRODUCT_LABEL))

    def _on_product_selected(self, _event=None):
        display_name = self.product_var.get()
        name = self._display_to_name.get(display_name, display_name)
        self.current_product_id = self._product_name_to_id.get(name)
        self.save_settings()

    # ──────────────────────────────────────────────
    # Child windows
    # ──────────────────────────────────────────────

    def open_settings(self):
        if self.settings_window is None or not self.settings_window.winfo_exists():
            self.settings_window = SettingsWindow(self)
        else:
            self.settings_window.lift()
            self.settings_window.focus_set()

    def open_products(self):
        if self.products_window is None or not self.products_window.winfo_exists():
            self.products_window = ProductsWindow(self)
        else:
            self.products_window.lift()
            self.products_window.focus_set()

    def open_history(self):
        if self.history_window is None or not self.history_window.winfo_exists():
            self.history_window = HistoryWindow(self)
        else:
            self.history_window.lift()
            self.history_window.focus_set()

    def open_reports(self):
        if self.reports_window is None or not self.reports_window.winfo_exists():
            self.reports_window = ReportsWindow(self)
        else:
            self.reports_window.refresh()
            self.reports_window.lift()
            self.reports_window.focus_set()

    def _settings_device_tab_active(self):
        """ True אם הגדרות פתוחות וכרגע על טאב "תצורת משקל"/"איפוס יצרן" — שכבר
        עצר את המוניטור בעצמו (pause_device_access), ראו SettingsWindow._on_tab_changed. """
        return (self.settings_window is not None and self.settings_window.winfo_exists()
                and self.settings_window._device_tab_active)

    def _on_zero_click(self):
        if not self.engine.connected:
            return
        # אם טאב תצורת המשקל בהגדרות כבר פעיל, הוא כבר עצר את המוניטור בעצמו
        # ויחזיר אותו כשיצא ממנו/יסגור — אין לגעת ב-pause/resume כאן, רק לשלוח
        # את הפקודה, אחרת נוציא אותה ממצב "מושתק" באמצע שהוא עדיין פעיל.
        if not self._settings_device_tab_active():
            self.engine.pause_device_access()
        self.engine.zero()

    def _on_zero_result(self, payload):
        if self._settings_device_tab_active():
            return
        self.engine.resume_after_device_access()
        if payload.get("ok"):
            messagebox.showinfo("איפוס", "האיפוס בוצע בהצלחה")
        else:
            messagebox.showerror("שגיאה", payload.get("error", "האיפוס נכשל"))

    def _on_close(self):
        try:
            self.engine.disconnect()
        except Exception:
            pass
        try:
            self.relay_engine.disconnect()
        except Exception:
            pass
        try:
            self.lan_dashboard.stop()
        except Exception:
            pass
        self.root.destroy()

    # ──────────────────────────────────────────────
    # Event pump — היחיד שנוגע ב-Tk וב-DB
    # ──────────────────────────────────────────────

    def _pump_queue(self):
        try:
            while True:
                kind, payload = self.ui_queue.get_nowait()
                if kind == "live":
                    # מציגים בדיוק כמו שההתקן שלח (payload[1] = מספר הספרות
                    # אחרי הנקודה בפריים הגולמי) — לא כופים פורמט קבוע (3 ספרות).
                    w, decimals = payload
                    weight_str = fmt_weight(w, decimals=decimals if decimals is not None else 3)
                    self.weight_var.set(weight_str)
                    self._update_dashboard_snapshot(live_weight=weight_str)
                elif kind == "update_check_result":
                    self._on_update_check_result(payload)
                elif kind == "update_apply_result":
                    self._on_update_apply_result(payload)
                elif kind == "session_start":
                    self.status_line_var.set("שוקל...")
                elif kind == "session_done":
                    self._on_session_done(*payload)
                elif kind == "monitor_stopped":
                    pass
                elif kind == "relay_connection_lost":
                    # הפורט נפל תוך כדי polling (לא ניתוק מכוון) — מפילים חזרה
                    # למצב סף-משקל הרגיל אוטומטית, "לא עובד אז כמו קודם".
                    # מצב "התחל"/armed מתאפס — לא ידוע אם ממסר 1 עוד דלוק
                    # בפועל אחרי שהפורט נפל, ואין דרך לשלוח לו כיבוי.
                    self._relay_motor_on = False
                    self._sync_relay_sensor_mode()
                    self._sync_relay_ui()
                    if self.settings_window is not None and self.settings_window.winfo_exists():
                        self.settings_window.handle_engine_event(kind, payload)
                else:
                    # device_zero יכול לבוא גם מכפתור "איפוס" בכותרת (מטופל כאן)
                    # וגם מטאב "תצורת משקל" בהגדרות (מטופל שם) — שני הצרכנים
                    # מקבלים את האירוע, ה-guard ב-_on_zero_result מונע resume כפול.
                    if kind == "device_zero":
                        self._on_zero_result(payload)
                    # אירועי דיאגנוסטיקה/כיול/תצורת משקל/ממסרים — רלוונטיים רק אם מסך ההגדרות פתוח
                    if self.settings_window is not None and self.settings_window.winfo_exists():
                        self.settings_window.handle_engine_event(kind, payload)
                    if kind in ("calib_disconnect", "device_reset_disconnect"):
                        self.disconnect()
        except queue.Empty:
            pass
        self.root.after(40, self._pump_queue)

    def _on_session_done(self, readings, elapsed, wall_start, wall_end):
        decided, basis, window_start, window_end = decide_weight(readings)
        weights = [w for _, w, _ in readings]
        count = len(weights)
        self.weight_var.set(fmt_weight(decided))

        if decided < self.engine.settings.min_save_weight:
            self.status_line_var.set(
                f"משקל {fmt_weight(decided)} kg מתחת למינימום השמירה — לא נשמר")
            return

        product = db.get_product(self.current_product_id) if self.current_product_id else None
        verdict, row_id = db.insert_weighing(decided, count, elapsed, min(weights), max(weights),
                                             basis, product)
        db.insert_weighing_readings(row_id, readings, window_start, window_end)

        self.traffic.set_state(verdict, f"{fmt_weight(decided, force_sign=True)} kg   "
                                        f"{VERDICT_TEXT.get(verdict, '')}")
        self.status_line_var.set(
            f"הושלם: {fmt_weight(decided)} kg   {VERDICT_TEXT[verdict]}   "
            f"({count} קריאות, {elapsed:.1f}s)")

        if self.relay_enabled and self.relay_engine.connected:
            self.relay_engine.activate_sort_relay(verdict)

        product_name = product["name"] if product else NO_PRODUCT_LABEL
        self._add_recent_row(row_id, wall_end, product_name, decided, verdict, count, elapsed)
        self._refresh_product_summary()

        if self.history_window is not None and self.history_window.winfo_exists():
            self.history_window.refresh()

    def _add_recent_row(self, row_id, wall_end, product_name, decided, verdict, count, elapsed):
        time_str = wall_end.strftime("%H:%M:%S")
        self.recent_tree.insert("", 0, iid=str(row_id), tags=(verdict,),
                                values=(row_id, time_str, product_name,
                                        fmt_weight(decided, force_sign=True),
                                        VERDICT_TEXT[verdict], count, f"{elapsed:.2f}s"))

    def _recent_since_bound(self):
        """ תחילת הטווח הנוכחי (today/week/session), משולב עם "נקה תצוגה" אם נעשה שימוש בה. """
        now = datetime.now()
        if self._recent_filter == "today":
            bound = now.replace(hour=0, minute=0, second=0, microsecond=0)
        elif self._recent_filter == "week":
            bound = now - timedelta(days=7)
        else:  # "session"
            bound = self._app_start_time

        if self._recent_cleared_at is not None and self._recent_cleared_at > bound:
            bound = self._recent_cleared_at
        return bound

    def _on_recent_filter_changed(self, _event=None):
        display = self.recent_filter_var.get()
        self._recent_filter = self._recent_display_to_key.get(display, self._recent_filter)
        # בחירת מסנן מפורשת מבטלת "נקה תצוגה" קודם — אחרת שינוי לטווח רחב יותר
        # (למשל "שבוע") לא היה מציג שוב שקילות ישנות שהוסתרו ע"י ניקוי.
        self._recent_cleared_at = None
        self._refresh_recent_list()

    def _on_clear_recent_display(self):
        """ מנקה את תצוגת "שקילות אחרונות" בלבד — השקילות עצמן נשארות ב-DB ונגישות
        דרך "הצג את כל ההיסטוריה". שקילות חדשות ימשיכו להופיע כרגיל. """
        self._recent_cleared_at = datetime.now()
        self._refresh_recent_list()

    def _refresh_recent_list(self):
        self.recent_tree.delete(*self.recent_tree.get_children())
        since = self._recent_since_bound().isoformat(timespec="seconds")
        for r in db.list_weighings(since=since):
            time_str = r["timestamp"][11:19] if r["timestamp"] and len(r["timestamp"]) >= 19 \
                else (r["timestamp"] or "")
            product_name = r["product_name"] or NO_PRODUCT_LABEL
            elapsed_str = f"{r['elapsed_seconds']:.2f}s" if r["elapsed_seconds"] is not None else "—"
            self.recent_tree.insert("", "end", iid=str(r["id"]), tags=(r["verdict"],),
                                    values=(r["id"], time_str, product_name,
                                            fmt_weight(r["decided_weight"], force_sign=True),
                                            VERDICT_TEXT.get(r["verdict"], r["verdict"]),
                                            r["reading_count"] or 0, elapsed_str))
        self._refresh_product_summary()

    def _refresh_product_summary(self):
        """ טבלת סיכום מוצרים (תקין/מעל/מתחת/סה"כ) על אותה קבוצת שקילות המוצגת
        כרגע ב"שקילות אחרונות" — אותו since (מסנן זמן + "נקה תצוגה"). """
        self.product_summary_tree.delete(*self.product_summary_tree.get_children())
        since = self._recent_since_bound().isoformat(timespec="seconds")
        counts = {}
        for r in db.list_weighings(since=since):
            name = r["product_name"] or NO_PRODUCT_LABEL
            c = counts.setdefault(name, {"green": 0, "red": 0, "yellow": 0, "total": 0})
            c["total"] += 1
            if r["verdict"] in c:
                c[r["verdict"]] += 1
        for i, name in enumerate(sorted(counts), start=1):
            c = counts[name]
            self.product_summary_tree.insert("", "end", values=(
                i, name, c["green"], c["red"], c["yellow"], c["total"]))

    def _on_recent_double_click(self, _event=None):
        sel = self.recent_tree.selection()
        if not sel:
            return
        try:
            weighing_id = int(sel[0])
        except ValueError:
            return
        WeighingDetailWindow(self, weighing_id)
