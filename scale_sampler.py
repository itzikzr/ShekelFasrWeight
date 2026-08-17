"""
Scale Sampler — תחנת שקילה Swan (RS-232), חוצה Windows/Linux.

מסוע עובד כל הזמן; כשמתגלה משקל מעל הסף האפליקציה ממצעת את הקריאות הנכנסות
ובסוף רושמת את המשקל שעבר, כולל סיווג רמזור (ירוק/אדום/צהוב) לפי מוצר נבחר.

כל הלוגיקה נמצאת בחבילת scale_app/ — קובץ זה הוא רק נקודת הכניסה.
"""

import sys
import tkinter as tk

from scale_app import rtl, theme
from scale_app.main_window import MainWindow

if __name__ == "__main__":
    if sys.platform == "win32":
        # בלי זה, שורת המשימות של Windows מציגה את אייקון python.exe במקום
        # אייקון האפליקציה (icon.ico) — היא מקבצת/מזהה חלונות לפי ה-exe
        # שהריץ אותם כברירת מחדל כשמריצים סקריפט python.exe רגיל (לא exe
        # קומפילציה עצמאי), אלא אם מוגדר AppUserModelID מפורש. חייב לרוץ
        # *לפני* יצירת tk.Tk() הראשון.
        import ctypes
        try:
            ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID(
                "ShekelOnline.ScaleSampler.WeighingStation")
        except Exception:
            pass
    rtl.patch()
    root = tk.Tk()
    theme.maximize(root)
    app = MainWindow(root)
    root.mainloop()
