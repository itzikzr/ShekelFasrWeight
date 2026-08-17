"""
ווידג'טים גרפיים משותפים — מלבן סטטוס השקילה ופס מצב חיבור.
"""

import tkinter as tk

from . import theme


class ColorBar(tk.Canvas):
    """
    מלבן רחב שרק צבעו משתנה — תצוגת הסטטוס בעמוד הראשי.
    set_state("none"/"red"/"yellow"/"green") צובע את כל השטח בצבע הסמנטי המתאים.
    הצבע נקרא מ-theme בכל קריאה (לא נשמר כקבוע) כדי שיתעדכן נכון גם אם מצב
    התצוגה (בהיר/כהה) מתחלף בזמן שהמלבן כבר צבוע.
    """

    def __init__(self, master, width=340, height=40, **kwargs):
        super().__init__(master, width=width, height=height,
                         background=theme.CARD_BG, highlightthickness=0, **kwargs)
        self._rect = self.create_rectangle(0, 0, width, height, outline="")
        self._state = "none"
        self.set_state("none")

    def _color_for(self, state: str) -> str:
        return {"red": theme.RED, "yellow": theme.AMBER,
                "green": theme.GREEN}.get(state, theme.IDLE_GRAY)

    def set_state(self, state: str):
        self._state = state
        self.itemconfig(self._rect, fill=self._color_for(state))

    def refresh_theme(self):
        """ מעדכן את צבע הרקע ומרנדר מחדש את המלבן בצבע העדכני של הפלטה. """
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
