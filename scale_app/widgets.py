"""
ווידג'טים גרפיים משותפים — רמזור השקילה ופס מצב חיבור.
"""

import tkinter as tk

from . import theme


class TrafficLight(tk.Canvas):
    """
    רמזור אנכי אדום/צהוב/ירוק. set_state("none"/"red"/"yellow"/"green") מדליק מנורה אחת.
    צבעי הנורה הדולקת נקראים מ-theme בכל קריאה (לא נשמרים כקבוע) כדי שיתעדכנו נכון
    גם אם מצב התצוגה (בהיר/כהה) מתחלף בזמן שהרמזור כבר דולק בצבע מסוים.
    """

    _DIM = {"red": "#ffffff", "yellow": "#ffffff", "green": "#ffffff"}

    def __init__(self, master, size=140, **kwargs):
        width = size
        height = int(size * 2.6)
        super().__init__(master, width=width, height=height,
                          background=theme.CARD_BG, highlightthickness=0, **kwargs)
        pad = size * 0.12
        self.create_rectangle(pad, pad, width - pad, height - pad,
                              fill="#22262b", outline="")
        r = (width - 2 * pad) * 0.32
        d = 2 * r
        inner_h = height - 2 * pad
        gap = (inner_h - 3 * d) / 4   # מרווח שווה: לפני העיגול הראשון, בין העיגולים, ואחרי האחרון
        cx = width / 2
        self._lamp_ids = {}
        for i, name in enumerate(("red", "green", "yellow")):
            cy = pad + gap * (i + 1) + d * i + r
            lamp = self.create_oval(cx - r, cy - r, cx + r, cy + r,
                                    fill=self._DIM[name], outline="")
            self._lamp_ids[name] = lamp
        self._state = "none"
        self.set_state("none")

    def _lit_color(self, name: str) -> str:
        return {"red": theme.RED, "yellow": theme.AMBER, "green": theme.GREEN}[name]

    def set_state(self, state: str):
        self._state = state
        for name, lamp_id in self._lamp_ids.items():
            fill = self._lit_color(name) if state == name else self._DIM[name]
            self.itemconfig(lamp_id, fill=fill)

    def refresh_theme(self):
        """ מעדכן את צבע הרקע ומרנדר מחדש את הנורה הדולקת בצבע העדכני של הפלטה. """
        self.configure(background=theme.CARD_BG)
        self.set_state(self._state)


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
