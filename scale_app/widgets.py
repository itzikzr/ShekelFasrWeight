"""
ווידג'טים גרפיים משותפים — מלבן סטטוס השקילה, פס מצב חיבור, גרף מגמת משקל
ובורר תאריך.
"""

import calendar
import tkinter as tk
from datetime import date
from tkinter import ttk

from . import theme
from .formatting import fmt_weight

_HEBREW_MONTHS = ["ינואר", "פברואר", "מרץ", "אפריל", "מאי", "יוני",
                  "יולי", "אוגוסט", "ספטמבר", "אוקטובר", "נובמבר", "דצמבר"]


class ColorBar(tk.Canvas):
    """
    מלבן רחב שצבעו וגם תוצאת השקילה (טקסט) שבתוכו משתנים — תצוגת הסטטוס
    בעמוד הראשי. set_state("none"/"red"/"yellow"/"green", text=...) צובע את
    כל השטח בצבע הסמנטי המתאים וכותב את text במרכזו.
    הצבע נקרא מ-theme בכל קריאה (לא נשמר כקבוע) כדי שיתעדכן נכון גם אם מצב
    התצוגה (בהיר/כהה) מתחלף בזמן שהמלבן כבר צבוע. צבע הטקסט עצמו קבוע לפי
    מצב, לא לפי theme — צהוב (AMBER) הוא הרקע הבהיר ביותר משתי הפלטות (בהיר
    וכהה), טקסט לבן עליו כמעט בלתי-קריא; טקסט שחור נבחר רק למצב הזה, לבן
    לשלושת האחרים (ירוק/אדום/אפור-אין-מוצר — כולם כהים מספיק בשתי הפלטות).
    """

    def __init__(self, master, width=340, height=40, **kwargs):
        super().__init__(master, width=width, height=height,
                         background=theme.CARD_BG, highlightthickness=0, **kwargs)
        self._rect = self.create_rectangle(0, 0, width, height, outline="")
        self._text_id = self.create_text(width / 2, height / 2, text="",
                                         font=("TkDefaultFont", 13, "bold"))
        self._state = "none"
        self._text = ""
        self.set_state("none")

    def _color_for(self, state: str) -> str:
        return {"red": theme.RED, "yellow": theme.AMBER,
                "green": theme.GREEN}.get(state, theme.IDLE_GRAY)

    def _text_color_for(self, state: str) -> str:
        return "#000000" if state == "yellow" else "#ffffff"

    def set_state(self, state: str, text: str = ""):
        self._state = state
        self._text = text
        self.itemconfig(self._rect, fill=self._color_for(state))
        self.itemconfig(self._text_id, text=text, fill=self._text_color_for(state))

    def refresh_theme(self):
        """ מעדכן את צבע הרקע ומרנדר מחדש את המלבן בצבע העדכני של הפלטה — עם
        אותו טקסט שהיה מוצג, לא מאפס אותו. """
        self.configure(background=theme.CARD_BG)
        self.set_state(self._state, self._text)


class StatusPill(tk.Frame):
    """ נקודה צבעונית + טקסט — לתצוגת מצב חיבור. """

    def __init__(self, master, **kwargs):
        super().__init__(master, background=theme.CARD_BG, **kwargs)
        self._dot = tk.Canvas(self, width=12, height=12, background=theme.CARD_BG,
                              highlightthickness=0)
        self._dot.pack(side="left", padx=(0, 6))
        self._dot_id = self._dot.create_oval(1, 1, 11, 11, fill=theme.IDLE_GRAY, outline="")
        self._label = tk.Label(self, background=theme.CARD_BG, foreground=theme.TEXT,
                               font=("TkDefaultFont", 10))
        self._label.pack(side="left")

    def set(self, text: str, color: str):
        self._label.config(text=text)
        self._dot.itemconfig(self._dot_id, fill=color)

    def refresh_theme(self):
        """ מעדכן רק את צבעי המסגרת/רקע — הצבע הסמנטי (מחובר/מנותק) נשאר באחריות הקורא. """
        self.configure(background=theme.CARD_BG)
        self._dot.configure(background=theme.CARD_BG)
        self._label.configure(background=theme.CARD_BG, foreground=theme.TEXT)


class TrendChart(tk.Canvas):
    """
    גרף מגמת משקל לאורך זמן (ReportsWindow) — נקודה צבעונית לכל שקילה
    (ירוק/אדום/צהוב/אפור=ללא מוצר) על קו מחבר דק, ורצועת טולרנס מוצללת +
    קו מטרה מקווקו כשיש target/tolerance (כלומר מוצר ספציפי נבחר, לא "הכל").

    מצייר ב-Canvas גולמי בכוונה, לא עם ספריית גרפים חיצונית (matplotlib וכו') —
    התלות היחידה של האפליקציה מעבר ל-stdlib היא pyserial (חובה) ו-openpyxl
    (אופציונלי, לייצוא בלבד), וזה נשאר כך.
    """

    MARGIN_LEFT = 56
    MARGIN_RIGHT = 16
    MARGIN_TOP = 14
    MARGIN_BOTTOM = 26
    DOT_RADIUS = 3.5

    def __init__(self, master, height=260, **kwargs):
        super().__init__(master, height=height, background=theme.CARD_BG,
                         highlightthickness=0, **kwargs)
        self._points = []   # [(x_label, weight, verdict), ...] בסדר כרונולוגי
        self._target = None
        self._tol_upper = None
        self._tol_lower = None
        self._empty_message = "אין נתונים להצגה בטווח הנבחר"
        self.bind("<Configure>", lambda _e: self._redraw())

    def set_data(self, points, target=None, tolerance_upper=None, tolerance_lower=None,
                empty_message=None):
        self._points = points
        self._target = target
        self._tol_upper = tolerance_upper
        self._tol_lower = tolerance_lower
        if empty_message is not None:
            self._empty_message = empty_message
        self._redraw()

    def refresh_theme(self):
        self.configure(background=theme.CARD_BG)
        self._redraw()

    def _redraw(self):
        self.delete("all")
        w, h = self.winfo_width(), self.winfo_height()
        if w <= 1 or h <= 1:
            return
        if not self._points:
            self.create_text(w / 2, h / 2, text=self._empty_message,
                             fill=theme.TEXT_MUTED, font=("TkDefaultFont", 11))
            return

        x0, x1 = self.MARGIN_LEFT, w - self.MARGIN_RIGHT
        y0, y1 = self.MARGIN_TOP, h - self.MARGIN_BOTTOM
        if x1 <= x0 or y1 <= y0:
            return

        weights = [p[1] for p in self._points]
        lo, hi = min(weights), max(weights)
        if self._target is not None:
            lo = min(lo, self._target - (self._tol_lower or 0))
            hi = max(hi, self._target + (self._tol_upper or 0))
        span = hi - lo
        if span <= 0:
            span = (abs(hi) * 0.1) or 1.0
        pad = span * 0.15
        lo, hi = lo - pad, hi + pad
        span = hi - lo

        def yof(weight):
            return y1 - (weight - lo) / span * (y1 - y0)

        n = len(self._points)

        def xof(i):
            return (x0 + x1) / 2 if n == 1 else x0 + i / (n - 1) * (x1 - x0)

        if self._target is not None:
            top = yof(self._target + (self._tol_upper or 0))
            bottom = yof(self._target - (self._tol_lower or 0))
            self.create_rectangle(x0, top, x1, bottom, fill=theme.GREEN_BG, outline="")
            target_y = yof(self._target)
            self.create_line(x0, target_y, x1, target_y, fill=theme.GREEN, dash=(4, 3))

        self.create_line(x0, y0, x0, y1, fill=theme.BORDER)
        self.create_line(x0, y1, x1, y1, fill=theme.BORDER)
        for frac in (0.0, 0.5, 1.0):
            gy = y0 + frac * (y1 - y0)
            self.create_line(x0, gy, x1, gy, fill=theme.BORDER, dash=(2, 4))
            self.create_text(x0 - 6, gy, text=fmt_weight(hi - frac * span, decimals=2),
                             anchor="e", fill=theme.TEXT_MUTED, font=("TkDefaultFont", 8))

        if n >= 2:
            coords = [c for i, (_lbl, weight, _v) in enumerate(self._points)
                     for c in (xof(i), yof(weight))]
            self.create_line(*coords, fill=theme.TEXT_MUTED, width=1)

        colors = {"green": theme.GREEN, "red": theme.RED, "yellow": theme.AMBER,
                 "none": theme.IDLE_GRAY}
        for i, (_lbl, weight, verdict) in enumerate(self._points):
            cx, cy = xof(i), yof(weight)
            r = self.DOT_RADIUS
            self.create_oval(cx - r, cy - r, cx + r, cy + r,
                             fill=colors.get(verdict, theme.IDLE_GRAY), outline="")

        for i in sorted({0, n // 2, n - 1}):
            self.create_text(xof(i), y1 + 12, text=self._points[i][0],
                             fill=theme.TEXT_MUTED, font=("TkDefaultFont", 8))


class DatePicker(ttk.Frame):
    """
    שדה תאריך שנבחר מלוח שנה קופץ, לא מוקלד — Entry לקריאה בלבד (state=
    "readonly") + כפתור "📅" שפותח פופאפ. מיושם עצמאית ב-Tkinter טהור, לא עם
    tkcalendar חיצוני — כמו ב-TrendChart מול matplotlib: זה קיוסק עם עיצוב
    בהיר/כהה מותאם, וידג'ט חוץ עם עיצוב קשיח משלו לא היה משתלב בלי עבודה
    נוספת בכל מקרה, וזו הזדמנות טובה כמו כל אחרת לא להוסיף תלות.

    get() -> "YYYY-MM-DD" או "" אם לא נבחר כלום. set(value)/clear(). on_change
    (אופציונלי) נקרא בלי ארגומנטים בכל בחירת יום בפופאפ — כדי שהמסך הקורא
    יוכל לרפרש סינון באופן זהה ל-<<ComboboxSelected>>.
    """

    WEEKDAY_LABELS = ["א", "ב", "ג", "ד", "ה", "ו", "ש"]  # תחילת שבוע יום ראשון

    def __init__(self, master, on_change=None, **kwargs):
        super().__init__(master, **kwargs)
        self.on_change = on_change
        self._value = None   # datetime.date | None
        self._popup = None
        self._cal_year = None
        self._cal_month = None

        self.entry_var = tk.StringVar(value="")
        self.entry = ttk.Entry(self, textvariable=self.entry_var, width=11, state="readonly")
        self.entry.pack(side="right")
        ttk.Button(self, text="📅", width=3, command=self._toggle_popup).pack(
            side="right", padx=(4, 0))

    def get(self) -> str:
        return self._value.isoformat() if self._value else ""

    def set(self, value):
        """ value: מחרוזת "YYYY-MM-DD", אובייקט date, או ריק/None לניקוי. """
        if not value:
            self.clear()
            return
        d = value if isinstance(value, date) else date.fromisoformat(value)
        self._value = d
        self.entry_var.set(d.isoformat())

    def clear(self):
        self._value = None
        self.entry_var.set("")

    def refresh_theme(self):
        pass   # הפופאפ נבנה מאפס בכל פתיחה עם צבעי theme העדכניים; סגור, אין מה לעדכן.

    def _toggle_popup(self):
        if self._popup is not None and self._popup.winfo_exists():
            self._close_popup()
            return
        self._open_popup()

    def _open_popup(self):
        base = self._value or date.today()
        self._cal_year, self._cal_month = base.year, base.month

        self._popup = tk.Toplevel(self)
        self._popup.overrideredirect(True)
        self._popup.attributes("-topmost", True)
        x = self.entry.winfo_rootx()
        y = self.entry.winfo_rooty() + self.entry.winfo_height()
        self._popup.geometry(f"+{x}+{y}")
        self._build_calendar_body()
        self._popup.bind("<Escape>", lambda e: self._close_popup())
        self._popup.bind("<FocusOut>", self._on_popup_focus_out)
        self._popup.focus_force()

    def _on_popup_focus_out(self, _event):
        # ה-focus עובר בין ווידג'טים *בתוך* הפופאפ (כפתורי הימים/החודש) גם
        # הם — לא רק כשעוברים לחלון אחר לגמרי. סוגרים רק אם ה-focus החדש
        # באמת מחוץ לפופאפ, אחרת לחיצה על "▶"/"◀" הייתה סוגרת אותו מיידית.
        if self._popup is None:
            return
        new_focus = self._popup.focus_get()
        if new_focus is None or str(new_focus).startswith(str(self._popup)):
            return
        self._close_popup()

    def _close_popup(self):
        if self._popup is not None:
            try:
                self._popup.destroy()
            except tk.TclError:
                pass
            self._popup = None

    def _build_calendar_body(self):
        for child in self._popup.winfo_children():
            child.destroy()

        frame = tk.Frame(self._popup, background=theme.CARD_BG, padx=6, pady=6,
                         highlightbackground=theme.BORDER, highlightthickness=1)
        frame.pack()

        header = tk.Frame(frame, background=theme.CARD_BG)
        header.pack(fill="x", pady=(0, 4))
        tk.Button(header, text="▶", command=self._prev_month, relief="flat", bd=0,
                 background=theme.CARD_BG, foreground=theme.TEXT,
                 activebackground=theme.BORDER).pack(side="right")
        tk.Label(header, text=f"{_HEBREW_MONTHS[self._cal_month - 1]} {self._cal_year}",
                background=theme.CARD_BG, foreground=theme.TEXT,
                font=("TkDefaultFont", 10, "bold")).pack(side="right", expand=True)
        tk.Button(header, text="◀", command=self._next_month, relief="flat", bd=0,
                 background=theme.CARD_BG, foreground=theme.TEXT,
                 activebackground=theme.BORDER).pack(side="left")

        grid = tk.Frame(frame, background=theme.CARD_BG)
        grid.pack()
        for col, label in enumerate(self.WEEKDAY_LABELS):
            tk.Label(grid, text=label, width=3, background=theme.CARD_BG,
                    foreground=theme.TEXT_MUTED, font=("TkDefaultFont", 9)).grid(
                row=0, column=6 - col, pady=(0, 2))   # RTL: יום א' בעמודה הימנית ביותר

        cal = calendar.Calendar(firstweekday=6)   # 6 = יום ראשון
        today = date.today()
        for row, week in enumerate(cal.monthdayscalendar(self._cal_year, self._cal_month), start=1):
            for col, day in enumerate(week):
                if day == 0:
                    continue
                is_selected = (self._value is not None and self._value.year == self._cal_year
                             and self._value.month == self._cal_month
                             and self._value.day == day)
                is_today = (today.year == self._cal_year and today.month == self._cal_month
                           and today.day == day)
                bg = theme.ACCENT if is_selected else theme.CARD_BG
                fg = "white" if is_selected else (theme.ACCENT if is_today else theme.TEXT)
                tk.Button(grid, text=str(day), width=3, relief="flat", bd=0,
                         background=bg, foreground=fg, activebackground=theme.BORDER,
                         command=lambda d=day: self._pick_day(d)).grid(
                    row=row, column=6 - col, padx=1, pady=1)

    def _prev_month(self):
        self._cal_month -= 1
        if self._cal_month == 0:
            self._cal_month = 12
            self._cal_year -= 1
        self._build_calendar_body()

    def _next_month(self):
        self._cal_month += 1
        if self._cal_month == 13:
            self._cal_month = 1
            self._cal_year += 1
        self._build_calendar_body()

    def _pick_day(self, day):
        self._value = date(self._cal_year, self._cal_month, day)
        self.entry_var.set(self._value.isoformat())
        self._close_popup()
        if self.on_change:
            self.on_change()
