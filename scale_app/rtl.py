"""Linux-only Tk BiDi shim.

Tk does not implement the Unicode Bidirectional Algorithm on X11 — unlike
Windows, where GDI/Uniscribe reorders RTL runs before Tk ever draws them —
so Hebrew text renders in raw logical (storage) order, which looks mirrored
on screen. Confirmed on a real Ubuntu 24.04 box: every Hebrew label/button/
column header appeared reversed while digits and Latin text ("kg", "COM3")
stayed correct, and the same build renders correctly on Windows.

patch() monkey-patches the Tk/ttk entry points that carry purely *display*
text drawn by Tk itself — widget `text=` (Label/Button/LabelFrame/... at
construction and via .configure()), Treeview row text/values, Treeview
column headers, Notebook tab labels, Text/ScrolledText content, and
messagebox title/message — so every Hebrew-containing string is
bidi-reordered before Tcl draws it. Call once at startup, before building
any widgets. No-op on non-Linux platforms, where rendering is already
correct and double-reordering would break it.

Note there are (at least) THREE distinct option-serialization paths in
tkinter/ttk that all need their own patch: `Misc._options` (widget
construction + `.configure()`), `_format_optdict` directly (`Treeview.insert`
/`.item()`), and `_val_or_dict`→`_format_optdict` (`Treeview.heading`,
`Notebook.add`/`.insert`/`.tab`). Missing any one of them is exactly what
happened here — column headers and Settings' tab labels stayed reversed
after the first two were already fixed. Any *new* ttk method found to carry
Hebrew text should be checked against its actual serialization path, not
assumed to be covered by the existing patches.

Deliberately NOT patched:
- StringVar.set()/.get() and Combobox `values=`. Those back interactive,
  data-bound widgets (the product selector combobox is the concrete case in
  main_window.py) whose displayed string is read back via .get() and used
  as a dict-key/DB lookup — reordering it for display would silently break
  that lookup on Linux only. Call sites with that shape must translate
  explicitly (see main_window.py's _display_to_name map) rather than going
  through this shim.
- Window titles (Wm.title()/wm_title()). The title bar / Alt-Tab preview /
  taskbar text isn't drawn by Tk at all — the window manager (GNOME Shell/
  Mutter, via Pango) draws it, and already implements the Unicode Bidi
  Algorithm correctly. Patching this here double-reordered an
  already-correct string.
"""
import re
import sys

_HEBREW_RE = re.compile(r"[֐-׿]")

_ENABLED = sys.platform.startswith("linux")

if _ENABLED:
    from bidi.algorithm import get_display
else:
    get_display = None


def _needs_bidi(value):
    return isinstance(value, str) and _HEBREW_RE.search(value) is not None


def visual(value):
    """Reorder a Hebrew-containing string to Tk-drawable visual order.

    No-op (returns value unchanged) on non-Linux, on non-strings, and on
    strings with no Hebrew characters.
    """
    if _ENABLED and _needs_bidi(value):
        return get_display(value)
    return value


def patch():
    """Monkey-patch the display-only Tk/ttk entry points. No-op off Linux."""
    if not _ENABLED:
        return

    import tkinter
    from tkinter import ttk, messagebox

    # Widget construction (classic + ttk, since ttk.Widget.__init__ delegates
    # to tkinter.Widget.__init__) and .configure()/.config()/widget['x']=y —
    # all funnel through Misc._options. Only the "text" option is touched;
    # other options (e.g. Combobox "values") are left alone deliberately.
    _orig_options = tkinter.Misc._options

    def _options(self, cnf, kw=None):
        if isinstance(cnf, dict) and "text" in cnf:
            cnf = dict(cnf)
            cnf["text"] = visual(cnf["text"])
        if isinstance(kw, dict) and "text" in kw:
            kw = dict(kw)
            kw["text"] = visual(kw["text"])
        return _orig_options(self, cnf, kw)

    tkinter.Misc._options = _options

    # Text / ScrolledText content (log boxes) — pure display, never read
    # back for matching.
    _orig_text_insert = tkinter.Text.insert

    def _text_insert(self, index, chars, *args):
        return _orig_text_insert(self, index, visual(chars), *args)

    tkinter.Text.insert = _text_insert

    # Treeview row text/values — read back elsewhere via the (numeric) iid,
    # never by re-parsing the displayed cell text, so safe to reorder.
    _orig_tv_insert = ttk.Treeview.insert

    def _tv_insert(self, parent, index, iid=None, **kw):
        if "text" in kw:
            kw["text"] = visual(kw["text"])
        if "values" in kw:
            kw["values"] = tuple(visual(v) for v in kw["values"])
        return _orig_tv_insert(self, parent, index, iid, **kw)

    ttk.Treeview.insert = _tv_insert

    _orig_tv_item = ttk.Treeview.item

    def _tv_item(self, item, option=None, **kw):
        if "text" in kw:
            kw["text"] = visual(kw["text"])
        if "values" in kw:
            kw["values"] = tuple(visual(v) for v in kw["values"])
        return _orig_tv_item(self, item, option, **kw)

    ttk.Treeview.item = _tv_item

    # Treeview column headers go through a THIRD, different ttk-internal
    # serialization path (_val_or_dict/_format_optdict), not Misc._options —
    # missed in the first pass, confirmed on hardware (every column header
    # app-wide, e.g. "מוצר"/"תקין"/"סה\"כ", stayed reversed after the text=
    # patch above supposedly covered "all" widget text).
    _orig_tv_heading = ttk.Treeview.heading

    def _tv_heading(self, column, option=None, **kw):
        if "text" in kw:
            kw["text"] = visual(kw["text"])
        return _orig_tv_heading(self, column, option, **kw)

    ttk.Treeview.heading = _tv_heading

    # ttk.Notebook tab labels (SettingsWindow's "חיבור"/"שקילה"/"דיאגנוסטיקה")
    # — same _format_optdict path as Treeview.heading, same fix.
    _orig_nb_add = ttk.Notebook.add
    _orig_nb_insert = ttk.Notebook.insert
    _orig_nb_tab = ttk.Notebook.tab

    def _nb_add(self, child, **kw):
        if "text" in kw:
            kw["text"] = visual(kw["text"])
        return _orig_nb_add(self, child, **kw)

    def _nb_insert(self, pos, child, **kw):
        if "text" in kw:
            kw["text"] = visual(kw["text"])
        return _orig_nb_insert(self, pos, child, **kw)

    def _nb_tab(self, tab_id, option=None, **kw):
        if "text" in kw:
            kw["text"] = visual(kw["text"])
        return _orig_nb_tab(self, tab_id, option, **kw)

    ttk.Notebook.add = _nb_add
    ttk.Notebook.insert = _nb_insert
    ttk.Notebook.tab = _nb_tab

    # Window titles are deliberately NOT patched: unlike widget content (which
    # Tk draws itself, with no bidi handling), the title bar / Alt-Tab preview
    # / taskbar text is drawn by the window manager (GNOME Shell/Mutter via
    # Pango), which already implements the Unicode Bidi Algorithm correctly.
    # Confirmed on real hardware: patching wm_title()/title() here reordered
    # an already-correct string, producing a *reversed* title — the opposite
    # of every other case this module fixes.

    # messagebox.showinfo/showerror/showwarning/askyesno/... all funnel
    # through _show(title, message, ...). Only `message` is reordered — it's
    # rendered as a Label inside the dialog, Tk-drawn like any other widget
    # text. `title` becomes the dialog's actual WM title (same tk_messageBox
    # -title mechanism as Wm.title()), so it hits the same window-manager
    # rendering path documented above and must NOT be reordered here either
    # — confirmed on real hardware, an error dialog's title showed reversed
    # while its body text was correctly fixed.
    _orig_show = messagebox._show

    def _show(title=None, message=None, _icon=None, _type=None, **options):
        return _orig_show(title, visual(message), _icon, _type, **options)

    messagebox._show = _show
