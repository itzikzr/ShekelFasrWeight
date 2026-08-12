"""
Scale Sampler — תחנת שקילה Swan (RS-232), חוצה Windows/Linux.

מסוע עובד כל הזמן; כשמתגלה משקל מעל הסף האפליקציה ממצעת את הקריאות הנכנסות
ובסוף רושמת את המשקל שעבר, כולל סיווג רמזור (ירוק/אדום/צהוב) לפי מוצר נבחר.

כל הלוגיקה נמצאת בחבילת scale_app/ — קובץ זה הוא רק נקודת הכניסה.
"""

import tkinter as tk

from scale_app import theme
from scale_app.main_window import MainWindow

if __name__ == "__main__":
    root = tk.Tk()
    theme.maximize(root)
    app = MainWindow(root)
    root.mainloop()
