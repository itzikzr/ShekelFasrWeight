"""
מסך תצורת משקל — פקודות ישירות מ-Swan_SC_Protocol.md שמשנות הגדרות על המשקל
עצמו (NVRAM שלו), לא הגדרות האפליקציה. נפרד בכוונה ממסך ההגדרות הרגיל.

כל פרמטר נקרא בנפרד (כפתור "קרא" משלו) ולא בבת אחת — כך המשתמש שולט מה לשלוח
מתי, ואין צורך במנגנון שרשור (chain) מרובה-שלבים. עדיין רק פקודה אחת בכל זמן נתון
(engine._device_busy שומר על זה) כי כל הפקודות חולקות אותו פורט טורי.

פותח ע"י main.engine.pause_device_access() וממשיך אותו בסגירה (resume_after_device_access)
— פקודות מכשיר ומצב שידור רציף לא יכולים לחלוק את הפורט בזמן אחד.
"""

import tkinter as tk
from tkinter import ttk, messagebox

from . import theme

BAUD_LABELS = ["4800", "9600", "14400", "19200", "28800", "38400", "57600", "115200"]
ROUND_LABELS = ["1g", "1g", "2g", "5g", "10g", "20g", "50g", "100g"]
DISFORM_LABELS = ["XXXXXXX", "XXXXX.X", "XXXX.XX", "XXX.XXX", "XX.XXXX", "X.XXXXX"]


class ScaleConfigWindow(tk.Toplevel):
    def __init__(self, main):
        super().__init__(main.root)
        self.main = main
        self.engine = main.engine
        self.title("תצורת משקל — Swan SC")
        self.geometry("600x760")
        self.configure(background=theme.BG)

        # לא רק מפסיק לקרוא — משתיק גם את השידור בפועל על המשקל, אחרת חבילות משקל
        # ('S'/'W') ממשיכות לזרום ומבלבלות את קריאת התגובות של פקודות התצורה כאן.
        self.engine.pause_device_access()

        pad = {"padx": 6, "pady": 4}

        # ── פרטי מכשיר ──
        info_frame = ttk.LabelFrame(self, text="פרטי מכשיר")
        info_frame.pack(fill="x", padx=12, pady=(12, 6))
        info_frame.columnconfigure(0, weight=1)

        self.info_var = tk.StringVar(value="—")
        self.version_var = tk.StringVar(value="—")
        self.raw_ad_var = tk.StringVar(value="—")

        # "מידע" מגיע מהמשקל כבלוק מרובה-שורות (עד 8 שורות) — לא שדה קצר כמו
        # גרסה/AD, אז מקבל שורה משלו עם גלישה, לא את הפריסה הקומפקטית הרגילה.
        info_row = ttk.Frame(info_frame)
        info_row.grid(row=0, column=0, columnspan=3, sticky="ew")
        info_row.columnconfigure(1, weight=1)
        self.btn_read_info = ttk.Button(info_row, text="קרא", width=6,
                                        command=lambda: self._run(self.engine.get_info))
        self.btn_read_info.grid(row=0, column=0, sticky="nw", padx=6, pady=4)
        ttk.Label(info_row, textvariable=self.info_var, wraplength=460, justify="right",
                 anchor="e").grid(row=0, column=1, sticky="ew", padx=6, pady=4)
        ttk.Label(info_row, text="מידע:").grid(row=0, column=2, sticky="ne", padx=6, pady=4)

        self.btn_read_version = self._add_readonly_row(
            info_frame, 1, "גרסה:", self.version_var, self.engine.get_version)
        self.btn_read_raw_ad = self._add_readonly_row(
            info_frame, 2, "AD גולמי:", self.raw_ad_var, self.engine.get_raw_ad)

        # ── הגדרות משקל ──
        settings_frame = ttk.LabelFrame(self, text="הגדרות משקל")
        settings_frame.pack(fill="x", padx=12, pady=6)
        settings_frame.columnconfigure(0, weight=1)

        self.full_scale_var = tk.StringVar(value="—")
        full_scale_entry = ttk.Entry(settings_frame, textvariable=self.full_scale_var,
                                     width=10, justify="center")
        self.btn_read_full_scale, self.btn_full_scale = self._add_settable_row(
            settings_frame, 0, 'קיבולת מלאה (ק"ג):', full_scale_entry,
            self.engine.query_full_scale, self._on_set_full_scale)

        self.round_var = tk.StringVar()
        self.round_combo = ttk.Combobox(settings_frame, textvariable=self.round_var,
                                        values=ROUND_LABELS, state="readonly", width=10)
        self.btn_read_round, self.btn_round = self._add_settable_row(
            settings_frame, 1, "צעד עיגול:", self.round_combo,
            self.engine.query_rounding, self._on_set_rounding)

        self.disform_var = tk.StringVar()
        self.disform_combo = ttk.Combobox(settings_frame, textvariable=self.disform_var,
                                          values=DISFORM_LABELS, state="readonly", width=10)
        self.btn_read_disform, self.btn_disform = self._add_settable_row(
            settings_frame, 2, "פורמט עשרוני:", self.disform_combo,
            self.engine.query_disform, self._on_set_disform)

        ttk.Separator(settings_frame, orient="horizontal").grid(
            row=3, column=0, columnspan=4, sticky="ew", pady=8)

        self.baud_warning_label = ttk.Label(
            settings_frame, text="⚠ שינוי קצב התקשורת מאתחל את המשקל ומנתק את התקשורת!",
            foreground=theme.RED)
        self.baud_warning_label.grid(row=4, column=0, columnspan=4, sticky="e", padx=8)

        self.baud_var = tk.StringVar()
        self.baud_combo = ttk.Combobox(settings_frame, textvariable=self.baud_var,
                                       values=BAUD_LABELS, state="readonly", width=10)
        self.btn_read_baud, self.btn_baud = self._add_settable_row(
            settings_frame, 5, "קצב תקשורת (Baud):", self.baud_combo,
            self.engine.query_baud, self._on_set_baud)

        # ── פעולות ──
        actions_frame = ttk.LabelFrame(self, text="פעולות")
        actions_frame.pack(fill="x", padx=12, pady=6)
        btns = ttk.Frame(actions_frame)
        btns.pack(pady=6)
        self.btn_zero = ttk.Button(btns, text="איפוס (Zero)", command=self._on_zero)
        self.btn_zero.pack(side="left", padx=6)
        self.btn_tare = ttk.Button(btns, text="טרה (Tare)", command=self._on_tare)
        self.btn_tare.pack(side="left", padx=6)
        self.btn_clear_tare = ttk.Button(btns, text="ביטול טרה", command=self._on_clear_tare)
        self.btn_clear_tare.pack(side="left", padx=6)
        self.action_status_var = tk.StringVar(value="")
        ttk.Label(actions_frame, textvariable=self.action_status_var).pack(pady=(0, 8))

        # ── איפוס יצרן ──
        reset_frame = ttk.LabelFrame(self, text="איפוס יצרן — פעולה הרסנית")
        reset_frame.pack(fill="x", padx=12, pady=6)
        self.reset_warning_label = ttk.Label(
            reset_frame,
            text='מאפס את כל הגדרות המשקל (כיול, קיבולת, טרה, baud וכו׳) לברירת המחדל.\n'
                 'הקלד RESET (אותיות גדולות, בדיוק) כדי לאפשר את הכפתור.',
            foreground=theme.RED, wraplength=520, justify="right")
        self.reset_warning_label.pack(padx=8, pady=(8, 4))
        self.reset_confirm_var = tk.StringVar()
        ttk.Entry(reset_frame, textvariable=self.reset_confirm_var, width=20,
                 justify="center").pack(pady=4)
        self.btn_factory_reset = ttk.Button(reset_frame, text="אפס להגדרות יצרן",
                                            state="disabled", command=self._on_factory_reset)
        self.btn_factory_reset.pack(pady=(4, 10))
        self.reset_confirm_var.trace_add("write", lambda *a: self._sync_buttons())

        self.protocol("WM_DELETE_WINDOW", self._on_close)
        self._sync_buttons()

    # ──────────────────────────────────────────────
    # שורות עזר — כל פרמטר עם כפתור "קרא" משלו
    # ──────────────────────────────────────────────

    def _add_readonly_row(self, parent, row, label, var, read_fn):
        """ שורה: תווית + ערך (בקריאה בלבד) + כפתור 'קרא'. מחזיר את כפתור הקריאה. """
        pad = {"padx": 6, "pady": 4}
        ttk.Label(parent, text=label).grid(row=row, column=2, sticky="e", **pad)
        ttk.Label(parent, textvariable=var).grid(row=row, column=1, sticky="e", **pad)
        btn = ttk.Button(parent, text="קרא", width=6,
                         command=lambda: self._run(read_fn))
        btn.grid(row=row, column=0, sticky="w", **pad)
        return btn

    def _add_settable_row(self, parent, row, label, input_widget, read_fn, on_set):
        """ שורה: תווית + שדה קלט/עריכה + כפתור 'קרא' + כפתור 'שמור'. מחזיר (btn_read, btn_save). """
        pad = {"padx": 6, "pady": 4}
        ttk.Label(parent, text=label).grid(row=row, column=3, sticky="e", **pad)
        input_widget.grid(row=row, column=2, sticky="w", **pad)
        btn_save = ttk.Button(parent, text="שמור", width=6, command=on_set)
        btn_save.grid(row=row, column=1, sticky="w", **pad)
        btn_read = ttk.Button(parent, text="קרא", width=6, command=lambda: self._run(read_fn))
        btn_read.grid(row=row, column=0, sticky="w", **pad)
        return btn_read, btn_save

    def _run(self, fn):
        self._set_busy(True)
        fn()

    # ──────────────────────────────────────────────
    # Button enable/disable
    # ──────────────────────────────────────────────

    def _all_action_buttons(self):
        return (self.btn_read_info, self.btn_read_version, self.btn_read_raw_ad,
                self.btn_read_full_scale, self.btn_full_scale,
                self.btn_read_round, self.btn_round,
                self.btn_read_disform, self.btn_disform,
                self.btn_read_baud, self.btn_baud,
                self.btn_zero, self.btn_tare, self.btn_clear_tare)

    def _set_busy(self, busy: bool):
        state = "disabled" if busy else ("normal" if self.engine.connected else "disabled")
        for b in self._all_action_buttons():
            b.config(state=state)
        self._sync_buttons()

    def _sync_buttons(self):
        reset_ok = (self.reset_confirm_var.get() == "RESET" and self.engine.connected
                   and self.btn_zero.cget("state") != "disabled")
        self.btn_factory_reset.config(state="normal" if reset_ok else "disabled")

    # ──────────────────────────────────────────────
    # Actions
    # ──────────────────────────────────────────────

    def _on_set_full_scale(self):
        try:
            kg = float(self.full_scale_var.get())
        except ValueError:
            messagebox.showerror("שגיאה", 'יש להזין מספר תקין (ק"ג)')
            return
        self._run(lambda: self.engine.set_full_scale(int(round(kg * 1000))))

    def _on_set_rounding(self):
        if self.round_var.get() not in ROUND_LABELS:
            return
        self._run(lambda: self.engine.set_rounding(ROUND_LABELS.index(self.round_var.get())))

    def _on_set_disform(self):
        if self.disform_var.get() not in DISFORM_LABELS:
            return
        self._run(lambda: self.engine.set_disform(DISFORM_LABELS.index(self.disform_var.get())))

    def _on_set_baud(self):
        if self.baud_var.get() not in BAUD_LABELS:
            return
        idx = BAUD_LABELS.index(self.baud_var.get())
        if not messagebox.askyesno(
                "אזהרה", f"שינוי קצב התקשורת ל-{BAUD_LABELS[idx]} יאתחל את המשקל ויגרום "
                        "לניתוק! תצטרך להתחבר מחדש בהגדרות בקצב החדש. להמשיך?"):
            return
        self._run(lambda: self.engine.set_baud(idx))

    def _on_zero(self):
        self.action_status_var.set("מאפס...")
        self._run(self.engine.zero)

    def _on_tare(self):
        self.action_status_var.set("מבצע טרה...")
        self._run(self.engine.tare)

    def _on_clear_tare(self):
        self._run(self.engine.clear_tare)

    def _on_factory_reset(self):
        if not messagebox.askyesno(
                "אזהרה — פעולה הרסנית",
                "פעולה זו תאפס את כל הגדרות המשקל (כיול, קיבולת, טרה, baud) לברירת המחדל. "
                "לא ניתן לבטל. להמשיך?"):
            return
        self._run(self.engine.factory_reset)

    # ──────────────────────────────────────────────
    # אירועי engine, מועברים מ-MainWindow._pump_queue
    # ──────────────────────────────────────────────

    @staticmethod
    def _safe_label(labels, index, fallback):
        """
        labels[index] בלי לקרוס אם index מחוץ לטווח — נצפה בפועל על חומרה אמיתית
        (למשל Disform=008 כשברשימה יש רק 6 אפשרויות, 0–5). אם לא תואם — מציגים
        את המספר הגולמי כפי שהתקבל במקום להתעלם/לקרוס.
        """
        if index is not None and 0 <= index < len(labels):
            return labels[index]
        return fallback if fallback is not None else (str(index) if index is not None else "—")

    def handle_engine_event(self, kind, payload):
        if kind in ("device_full_scale", "device_full_scale_set"):
            if payload.get("ok"):
                self.full_scale_var.set(f"{payload['grams'] / 1000:.3f}")
            else:
                messagebox.showerror("שגיאה", payload.get("error", "שגיאה לא ידועה"))

        elif kind == "device_baud":
            # "Baud=XXXXXXX" מדווח את קצב ה-baud הנוכחי בפועל (למשל 9600), לא אינדקס.
            if payload.get("ok"):
                baud = payload.get("baud")
                self.baud_var.set(str(baud) if baud is not None else "—")
            else:
                messagebox.showerror("שגיאה", payload.get("error", "שגיאה"))

        elif kind == "device_baud_set":
            if payload.get("ok") and payload.get("index") is not None:
                self.baud_var.set(self._safe_label(BAUD_LABELS, payload["index"], None))
            elif not payload.get("ok"):
                messagebox.showerror("שגיאה", payload.get("error", "שגיאה"))
            if payload.get("ok"):
                messagebox.showinfo("בוצע", "הפקודה נשלחה. המשקל מתאתחל — התחבר מחדש "
                                            "בקצב החדש דרך הגדרות.")

        elif kind in ("device_rounding", "device_rounding_set"):
            if payload.get("ok") and payload.get("index") is not None:
                self.round_var.set(self._safe_label(ROUND_LABELS, payload["index"], None))
            elif not payload.get("ok"):
                messagebox.showerror("שגיאה", payload.get("error", "שגיאה"))

        elif kind in ("device_disform", "device_disform_set"):
            if payload.get("ok") and payload.get("index") is not None:
                self.disform_var.set(self._safe_label(DISFORM_LABELS, payload["index"], None))
            elif not payload.get("ok"):
                messagebox.showerror("שגיאה", payload.get("error", "שגיאה"))

        elif kind == "device_zero":
            self.action_status_var.set("אפס בוצע ✓" if payload.get("ok")
                                       else f"אפס נכשל: {payload.get('error', '')}")

        elif kind == "device_tare":
            self.action_status_var.set("טרה בוצעה ✓" if payload.get("ok")
                                       else f"טרה נכשלה: {payload.get('error', '')}")

        elif kind == "device_clear_tare":
            self.action_status_var.set("טרה בוטלה")

        elif kind == "device_info":
            self.info_var.set(payload.get("text") or "—")

        elif kind == "device_version":
            self.version_var.set(payload.get("text") or "—")

        elif kind == "device_raw_ad":
            self.raw_ad_var.set(payload.get("text") or "—")

        elif kind == "device_factory_reset":
            if not payload.get("ok"):
                messagebox.showerror("שגיאה", payload.get("error", "שגיאה"))

        self._set_busy(False)

    def refresh_theme(self):
        self.configure(background=theme.BG)
        self.baud_warning_label.configure(foreground=theme.RED)
        self.reset_warning_label.configure(foreground=theme.RED)

    def _on_close(self):
        self.engine.resume_after_device_access()
        self.destroy()
