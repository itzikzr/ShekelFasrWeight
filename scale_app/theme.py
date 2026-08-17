"""
עיצוב אחיד וחוצה-פלטפורמות (Windows + Linux) — פלטת צבעים, גופנים וסטייל ttk אחד.

Tk הנטיבי משתנה מאוד בין Windows ל-Linux (עיצוב, גופנים קיימים) — כדי שהתוכנה תיראה
אחידה בשתי הפלטפורמות אנחנו לא נשענים על ה-theme הנטיבי, אלא בונים סטייל ttk אחד
("clam", קיים בכל build של Tk) עם הפלטה שלנו.

מצב בהיר/כהה: שני מילוני צבע קבועים (_LIGHT/_DARK). set_mode() מחליף את הערכים
הגלובליים במודול הזה ומריץ מחדש את apply_theme — זה מספיק כדי לעדכן חוצה-פלטפורמות
את כל ווידג'טי ה-ttk (הם נשענים על ה-style, שמתעדכן באופן חי). ווידג'טי tk גולמיים
(tk.Label/tk.Canvas שנוצרו עם background/foreground מפורש) לא מתעדכנים אוטומטית —
המסכים שמשתמשים בהם חושפים refresh_theme() שקוראים לו לאחר set_mode().
"""

import tkinter as tk
import tkinter.font as tkfont
from pathlib import Path
from tkinter import ttk

ASSETS_DIR = Path(__file__).parent / "assets"

_LIGHT = dict(
    BG="#f4f6f9", CARD_BG="#ffffff", BORDER="#dfe3e8", TEXT="#1a1a1a", TEXT_MUTED="#666666",
    ACCENT="#1a6fb5", ACCENT_DARK="#125088",
    GREEN="#2e7d32", GREEN_BG="#e6f4ea", RED="#c62828", RED_BG="#fdecea",
    AMBER="#f9a825", AMBER_BG="#fff6e0", IDLE_GRAY="#9aa5b1", IDLE_GRAY_BG="#eceff3",
)

_DARK = dict(
    BG="#1b1e23", CARD_BG="#252932", BORDER="#3a3f4a", TEXT="#e8e9eb", TEXT_MUTED="#9aa1ac",
    ACCENT="#5aa9e6", ACCENT_DARK="#8cc7f0",
    GREEN="#4caf50", GREEN_BG="#20301f", RED="#ef5350", RED_BG="#3a1f1f",
    AMBER="#ffca28", AMBER_BG="#3a2f10", IDLE_GRAY="#6b7280", IDLE_GRAY_BG="#2a2d33",
)

_PALETTES = {"light": _LIGHT, "dark": _DARK}
_current_mode = "light"


def _apply_palette(mode: str):
    global _current_mode
    global BG, CARD_BG, BORDER, TEXT, TEXT_MUTED, ACCENT, ACCENT_DARK
    global GREEN, GREEN_BG, RED, RED_BG, AMBER, AMBER_BG, IDLE_GRAY, IDLE_GRAY_BG
    global VERDICT_COLORS

    p = _PALETTES[mode]
    BG, CARD_BG, BORDER = p["BG"], p["CARD_BG"], p["BORDER"]
    TEXT, TEXT_MUTED = p["TEXT"], p["TEXT_MUTED"]
    ACCENT, ACCENT_DARK = p["ACCENT"], p["ACCENT_DARK"]
    GREEN, GREEN_BG = p["GREEN"], p["GREEN_BG"]
    RED, RED_BG = p["RED"], p["RED_BG"]
    AMBER, AMBER_BG = p["AMBER"], p["AMBER_BG"]
    IDLE_GRAY, IDLE_GRAY_BG = p["IDLE_GRAY"], p["IDLE_GRAY_BG"]

    VERDICT_COLORS = {
        "green":  (GREEN, GREEN_BG),
        "red":    (RED, RED_BG),
        "yellow": (AMBER, AMBER_BG),
        "none":   (IDLE_GRAY, IDLE_GRAY_BG),
    }
    _current_mode = mode


_apply_palette("light")

_font_cache = {}


def current_mode() -> str:
    return _current_mode


def pick_font(root, preferred: list, fallback="TkDefaultFont"):
    """ בוחר את הגופן הראשון מהרשימה שקיים בפועל על המערכת, אחרת fallback. """
    key = tuple(preferred)
    if key in _font_cache:
        return _font_cache[key]
    try:
        available = set(tkfont.families(root))
    except Exception:
        available = set()
    for name in preferred:
        if name in available:
            _font_cache[key] = name
            return name
    _font_cache[key] = fallback
    return fallback


def fonts(root):
    """ מחזיר dict של שמות גופנים זמינים, נבחרים פעם אחת לפי המערכת. """
    sans = pick_font(root, ["Segoe UI", "DejaVu Sans", "Helvetica", "Arial"])
    mono = pick_font(root, ["Consolas", "DejaVu Sans Mono", "Courier New", "monospace"],
                      fallback="TkFixedFont")
    return {"sans": sans, "mono": mono}


def apply_theme(root):
    """ מפעיל את ה-ttk style האחיד לפי הפלטה הנוכחית. אפשר לקרוא שוב לאחר set_mode. """
    style = ttk.Style(root)
    try:
        style.theme_use("clam")
    except tk.TclError:
        pass

    f = fonts(root)
    root.configure(background=BG)

    style.configure(".", background=BG, foreground=TEXT, font=(f["sans"], 10))
    style.configure("TFrame", background=BG)
    style.configure("Card.TFrame", background=CARD_BG, relief="flat")
    style.configure("TLabelframe", background=BG, foreground=TEXT)
    style.configure("TLabelframe.Label", background=BG, foreground=TEXT_MUTED,
                     font=(f["sans"], 9, "bold"))
    style.configure("TLabel", background=BG, foreground=TEXT)
    style.configure("Card.TLabel", background=CARD_BG, foreground=TEXT)
    style.configure("Muted.TLabel", background=BG, foreground=TEXT_MUTED)
    style.configure("Card.Muted.TLabel", background=CARD_BG, foreground=TEXT_MUTED)

    # Padding sized for touch (finger-sized targets), not just mouse — this
    # is a touchscreen kiosk app. Centralized here rather than per-button,
    # since every ttk button/tab/row in the app reads from this one style.
    style.configure("TButton", font=(f["sans"], 10), padding=(16, 10))
    style.configure("Accent.TButton", font=(f["sans"], 10, "bold"))
    style.map("Accent.TButton",
              background=[("!disabled", ACCENT)],
              foreground=[("!disabled", "#ffffff")])

    style.configure("Treeview", background=CARD_BG, fieldbackground=CARD_BG,
                     foreground=TEXT, rowheight=36, font=(f["sans"], 10))
    style.configure("Treeview.Heading", font=(f["sans"], 10, "bold"), padding=(8, 8))

    style.configure("TNotebook", background=BG, borderwidth=0)
    style.configure("TNotebook.Tab", font=(f["sans"], 10), padding=(20, 12))

    style.configure("TCombobox", fieldbackground=CARD_BG, background=CARD_BG, foreground=TEXT,
                     padding=(8, 8))
    style.configure("TEntry", fieldbackground=CARD_BG, foreground=TEXT, padding=(8, 8))
    style.configure("TSpinbox", fieldbackground=CARD_BG, foreground=TEXT, padding=(8, 6))
    style.configure("TCheckbutton", background=BG, foreground=TEXT, padding=(4, 8))
    style.configure("TRadiobutton", background=BG, foreground=TEXT, padding=(4, 8))

    # ttk's default scrollbar trough/thumb is mouse-width — too thin to grab
    # reliably with a finger.
    style.configure("Vertical.TScrollbar", arrowsize=24, width=24)
    style.configure("Horizontal.TScrollbar", arrowsize=24, width=24)

    return f


def set_mode(root, mode: str):
    """
    מחליף בין 'light' ל-'dark': מעדכן את הפלטה ומריץ מחדש את apply_theme (מעדכן חי
    את כל ווידג'טי ה-ttk). מחזיר את מילון הגופנים, כמו apply_theme.
    קריאה למסכים פתוחים כדי לעדכן ווידג'טי tk גולמיים היא באחריות הקורא (ראו
    refresh_theme בכל מסך).
    """
    if mode not in _PALETTES:
        raise ValueError(f"מצב תצוגה לא מוכר: {mode!r}")
    _apply_palette(mode)
    return apply_theme(root)


def maximize(root):
    """
    התחלה במסך מלא ממש — כולל השטח שבו יושבת שורת המשימות/Dock, לא רק 'הגדל' (zoomed)
    שמשאיר אותה חשופה. attributes('-fullscreen') הוא הדרך היחידה החוצה-פלטפורמות
    שמכסה גם אותה, אבל המחיר הוא שורת הכותרת (מזעור/שחזור/סגירה) נעלמת — לכן קושרים
    F11/Escape להחלפה למצב 'zoomed' רגיל (עם שורת כותרת) כדי שלא להיתקע בלי דרך
    למזער/לסגור את החלון בעכבר.
    """
    state = {"fullscreen": True}

    def _toggle_fullscreen(_event=None):
        state["fullscreen"] = not state["fullscreen"]
        try:
            root.attributes("-fullscreen", state["fullscreen"])
        except tk.TclError:
            pass
        if not state["fullscreen"]:
            try:
                root.state("zoomed")
            except tk.TclError:
                pass

    try:
        root.attributes("-fullscreen", True)
        root.bind("<F11>", _toggle_fullscreen)
        root.bind("<Escape>", _toggle_fullscreen)
        return
    except tk.TclError:
        pass
    try:
        root.state("zoomed")
        return
    except tk.TclError:
        pass
    try:
        root.attributes("-zoomed", True)
        return
    except tk.TclError:
        pass
    try:
        w = root.winfo_screenwidth()
        h = root.winfo_screenheight()
        root.geometry(f"{w}x{h}+0+0")
    except tk.TclError:
        pass


def maximize_window(win):
    """
    מתחיל Toplevel (הגדרות/מוצרים/היסטוריה/פירוט שקילה) ממוקסם — עם שורת כותרת
    ותפריט מזעור/שחזור/סגירה רגילים, לא fullscreen גולמי בלי שורת כותרת כמו
    maximize() לחלון הראשי. 'zoomed' עובד ב-Windows; fallback ל-'-zoomed'
    (Linux/X11) ואז לגיאומטריה מלאה מהמסך אם אף אחת מהן לא נתמכת בבילד הזה.
    """
    try:
        win.state("zoomed")
        return
    except tk.TclError:
        pass
    try:
        win.attributes("-zoomed", True)
        return
    except tk.TclError:
        pass
    try:
        w = win.winfo_screenwidth()
        h = win.winfo_screenheight()
        win.geometry(f"{w}x{h}+0+0")
    except tk.TclError:
        pass


def set_app_icon(root):
    """
    מגדיר את אייקון החלון/שורת המשימות מ-scale_app/assets (מקור: Pic/shekel.ico —
    לוגו "SHEKEL"). ל-Windows יש iconbitmap עם .ico (רזולוציות מדויקות לכל גודל
    תצוגה); כ-fallback חוצה-פלטפורמות (Linux) יש iconphoto עם PNG, שנתמך בכל
    build של Tk 8.6+.
    default=True/True מחיל את האייקון גם על כל Toplevel עתידי (הגדרות/מוצרים/היסטוריה).
    """
    try:
        root.iconbitmap(default=str(ASSETS_DIR / "icon.ico"))
        return
    except tk.TclError:
        pass
    try:
        icon_img = tk.PhotoImage(file=str(ASSETS_DIR / "icon.png"))
        root.iconphoto(True, icon_img)
        root._icon_photo_ref = icon_img   # שמירת הפניה — בלעדיה ה-GC יאסוף ותמונת האייקון תיעלם
    except tk.TclError:
        pass
