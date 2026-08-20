"""
מסך הגדרות — חיבור / פרמטרי שקילה / דיאגנוסטיקה (דגימה חד-פעמית, LIVE, כיול Swan).

כל הפעולות שמקיימות תקשורת בפועל (connect/sample/live/calibration) מבוצעות ע"י
main.engine; חלון זה רק קורא/כותב Tk vars ומעדכן את main.engine.settings.
"""

import time
import tkinter as tk
from tkinter import ttk, scrolledtext, filedialog, messagebox

from . import autostart, rtl, theme
from . import backup as backup_mod
from . import engine as engine_mod
from . import lan_dashboard
from .engine import decide_weight
from .formatting import fmt_weight

START_TRIGGER_LABELS = {
    "threshold": "סף משקל (כמו היום בלי חיישנים)",
    "sensor1_close": "חיישן כניסה — בכניסת הקופסה (מוקדם)",
    "sensor1_open": "חיישן כניסה — כשהקופסה כולה על המשקל",
}
STOP_TRIGGER_LABELS = {
    "threshold": "סף משקל (כמו היום בלי חיישנים)",
    "sensor2_close": "חיישן יציאה — כשהקופסה מתחילה לצאת",
    "sensor2_open": "חיישן יציאה — כשהקופסה עזבה לחלוטין",
}

BAUD_LABELS = ["4800", "9600", "14400", "19200", "28800", "38400", "57600", "115200"]
ROUND_LABELS = ["1g", "1g", "2g", "5g", "10g", "20g", "50g", "100g"]
DISFORM_LABELS = ["XXXXXXX", "XXXXX.X", "XXXX.XX", "XXX.XXX", "XX.XXXX", "X.XXXXX"]


class SettingsWindow(tk.Toplevel):
    def __init__(self, main):
        super().__init__(main.root)
        self.main = main
        self.engine = main.engine
        self.title("הגדרות")
        self.minsize(640, 560)
        theme.maximize_window(self)
        self.configure(background=theme.BG)

        self._live_running = False
        self._calib_dialog = None
        # true בזמן ש"פרטי מכשיר"/"הגדרות משקל"/"איפוס יצרן" פעיל — טאבים אלה
        # שולחים פקודות ישירות למשקל שלא יכולות לחלוק את הפורט עם השידור הרציף,
        # ראו _on_tab_changed.
        self._device_tab_active = False

        self.nb = ttk.Notebook(self)
        self.nb.pack(fill="both", expand=True, padx=10, pady=10)
        nb = self.nb

        self.tab_conn  = ttk.Frame(nb, padding=14)
        self.tab_relay = ttk.Frame(nb, padding=14)
        self.tab_weigh = ttk.Frame(nb, padding=14)
        self.tab_diag  = ttk.Frame(nb, padding=14)
        self.tab_device_info = ttk.Frame(nb, padding=14)
        self.tab_scale_config = ttk.Frame(nb, padding=14)
        self.tab_factory_reset = ttk.Frame(nb, padding=14)
        self.tab_backup = ttk.Frame(nb, padding=14)
        self.tab_dashboard = ttk.Frame(nb, padding=14)
        self.tab_updates = ttk.Frame(nb, padding=14)
        nb.add(self.tab_conn, text="חיבור")
        nb.add(self.tab_relay, text="ממסרים")
        nb.add(self.tab_weigh, text="שקילה")
        nb.add(self.tab_diag, text="דיאגנוסטיקה")
        nb.add(self.tab_device_info, text="פרטי מכשיר")
        nb.add(self.tab_scale_config, text="הגדרות משקל")
        nb.add(self.tab_factory_reset, text="איפוס יצרן")
        nb.add(self.tab_backup, text="גיבוי")
        nb.add(self.tab_dashboard, text="דשבורד רשת")
        nb.add(self.tab_updates, text="עדכונים")

        self._build_connection_tab()
        self._build_relay_tab()
        self._build_weighing_tab()
        self._build_diagnostics_tab()
        self._build_device_info_tab()
        self._build_scale_config_tab()
        self._build_factory_reset_tab()
        self._build_backup_tab()
        self._build_dashboard_tab()
        self._build_updates_tab()
        self._sync_connection_state()

        nb.bind("<<NotebookTabChanged>>", self._on_tab_changed)
        self.protocol("WM_DELETE_WINDOW", self.destroy)

    # ──────────────────────────────────────────────
    # חיבור
    # ──────────────────────────────────────────────

    def _build_connection_tab(self):
        t = self.tab_conn
        pad = {"padx": 6, "pady": 6}

        ttk.Label(t, text="Port:").grid(row=0, column=0, sticky="e", **pad)
        self.port_var = tk.StringVar(value=self.main.port)
        self.port_combo = ttk.Combobox(t, textvariable=self.port_var, width=30, state="readonly")
        self.port_combo.grid(row=0, column=1, **pad)
        ttk.Button(t, text="⟳", width=3, command=self._refresh_ports).grid(row=0, column=2, **pad)

        ttk.Label(t, text="Baud:").grid(row=1, column=0, sticky="e", **pad)
        self.baud_var = tk.StringVar(value=self.main.baud)
        ttk.Combobox(t, textvariable=self.baud_var, state="readonly", width=10,
                     values=["600", "1200", "2400", "4800", "9600", "19200", "38400",
                             "57600", "115200"]).grid(row=1, column=1, sticky="w", **pad)

        self.conn_status_var = tk.StringVar()
        ttk.Label(t, textvariable=self.conn_status_var,
                  style="Muted.TLabel").grid(row=2, column=0, columnspan=3, sticky="w", **pad)

        btn_frame = ttk.Frame(t)
        btn_frame.grid(row=3, column=0, columnspan=3, sticky="w", **pad)
        self.btn_connect = ttk.Button(btn_frame, text="התחבר", style="Accent.TButton",
                                       command=self._on_connect)
        self.btn_connect.pack(side="left", padx=4)
        self.btn_disconnect = ttk.Button(btn_frame, text="התנתק", command=self._on_disconnect)
        self.btn_disconnect.pack(side="left", padx=4)

        if not engine_mod.SERIAL_AVAILABLE:
            ttk.Label(t, text="pyserial לא מותקן — הרץ: pip install pyserial",
                      foreground=theme.RED).grid(row=4, column=0, columnspan=3, sticky="w", **pad)

        if autostart.is_supported():
            ttk.Separator(t, orient="horizontal").grid(
                row=5, column=0, columnspan=3, sticky="ew", pady=(10, 4))
            self.autostart_var = tk.BooleanVar(value=autostart.is_enabled())
            ttk.Checkbutton(t, text="הפעלה אוטומטית עם עליית המחשב",
                            variable=self.autostart_var,
                            command=self._on_autostart_toggle).grid(
                row=6, column=0, columnspan=3, sticky="w", **pad)

        ttk.Separator(t, orient="horizontal").grid(
            row=7, column=0, columnspan=3, sticky="ew", pady=(10, 4))
        # עבר לכאן מכפתור בכותרת המסך הראשי — כדי לפנות מקום ל"▶ התחל".
        self.dark_mode_var = tk.BooleanVar(value=self.main.dark_mode)
        ttk.Checkbutton(t, text="מצב כהה", variable=self.dark_mode_var,
                        command=self._on_dark_mode_toggle).grid(
            row=8, column=0, columnspan=3, sticky="w", **pad)

        self._refresh_ports()

    def _refresh_ports(self):
        ports = engine_mod.list_serial_ports()
        values = [f"{dev} | {desc}" for dev, desc in ports] if ports else ["(לא נמצאו פורטים)"]
        self.port_combo.config(values=values)
        if self.main.port:
            for v in values:
                if v.startswith(self.main.port):
                    self.port_var.set(v)
                    return
        if values:
            self.port_combo.current(0)

    def _sync_connection_state(self):
        connected = self.main.engine.connected
        self.conn_status_var.set("מחובר" if connected else "מנותק")
        self.btn_connect.config(state="disabled" if connected else "normal")
        self.btn_disconnect.config(state="normal" if connected else "disabled")
        self.port_combo.config(state="disabled" if connected else "readonly")
        self._sync_diag_buttons()
        self._set_busy(False)

    def _on_connect(self):
        if not engine_mod.SERIAL_AVAILABLE:
            messagebox.showerror("שגיאה", "pyserial לא מותקן — הרץ: pip install pyserial")
            return
        port_name = self.port_var.get().split("|")[0].strip()
        if not port_name or port_name.startswith("("):
            messagebox.showerror("שגיאה", "לא נבחר פורט תקין — לחץ ⟳ לרענון")
            return
        try:
            self.main.connect(port_name, self.baud_var.get())
        except Exception as e:
            messagebox.showerror("שגיאת חיבור", str(e))
            return
        self._sync_connection_state()

    def _on_disconnect(self):
        self.main.disconnect()
        self._sync_connection_state()

    def _on_autostart_toggle(self):
        try:
            if self.autostart_var.get():
                autostart.enable()
            else:
                autostart.disable()
        except Exception as e:
            messagebox.showerror("שגיאה", str(e))
            self.autostart_var.set(autostart.is_enabled())

    def _on_dark_mode_toggle(self):
        # main._toggle_theme() הופך את dark_mode הנוכחי — לא קורא את הערך
        # מה-checkbox עצמו — אז לחיצה מכאן ותוצאתה עקביות בכל מקרה.
        self.main._toggle_theme()

    # ──────────────────────────────────────────────
    # ממסרים (כרטיס IA-3116-U2i) — חיבור סיריאלי נפרד ועצמאי מהמשקל.
    # ──────────────────────────────────────────────

    def _build_relay_tab(self):
        t = self.tab_relay
        pad = {"padx": 6, "pady": 6}

        self.relay_enabled_var = tk.BooleanVar(value=self.main.relay_enabled)
        ttk.Checkbutton(t, text="השתמש בממסרים", variable=self.relay_enabled_var,
                        command=self._on_relay_enabled_toggle).grid(
            row=0, column=0, columnspan=3, sticky="w", **pad)
        ttk.Label(t, text="מנוע המסוע (ממסר 1), חיישני התחלה/סיום שקילה "
                          "(כניסות 1-2) ומיון לפי תוצאה (ממסרים 2-4).",
                  style="Muted.TLabel").grid(row=1, column=0, columnspan=3, sticky="w", **pad)

        self._start_trigger_display_to_key = {
            rtl.visual(label): key for key, label in START_TRIGGER_LABELS.items()
        }
        self._stop_trigger_display_to_key = {
            rtl.visual(label): key for key, label in STOP_TRIGGER_LABELS.items()
        }

        ttk.Label(t, text="מתחיל שקילה לפי:").grid(row=2, column=0, sticky="e", **pad)
        self.start_trigger_var = tk.StringVar(
            value=rtl.visual(START_TRIGGER_LABELS.get(self.main.start_trigger, "")))
        self.start_trigger_combo = ttk.Combobox(
            t, textvariable=self.start_trigger_var, state="readonly", width=38,
            values=[rtl.visual(v) for v in START_TRIGGER_LABELS.values()])
        self.start_trigger_combo.grid(row=2, column=1, columnspan=2, sticky="w", **pad)
        self.start_trigger_combo.bind("<<ComboboxSelected>>", self._on_start_trigger_changed)

        ttk.Label(t, text="מסיים שקילה לפי:").grid(row=3, column=0, sticky="e", **pad)
        self.stop_trigger_var = tk.StringVar(
            value=rtl.visual(STOP_TRIGGER_LABELS.get(self.main.stop_trigger, "")))
        self.stop_trigger_combo = ttk.Combobox(
            t, textvariable=self.stop_trigger_var, state="readonly", width=38,
            values=[rtl.visual(v) for v in STOP_TRIGGER_LABELS.values()])
        self.stop_trigger_combo.grid(row=3, column=1, columnspan=2, sticky="w", **pad)
        self.stop_trigger_combo.bind("<<ComboboxSelected>>", self._on_stop_trigger_changed)

        ttk.Label(t, text="\"סף משקל\" בהתחלה/סיום פועל כמו לפני שהיו חיישנים כלל — "
                          "אבל אם נבחר להתחלה, שקילה חדשה מתחילה רק אחרי לחיצה על "
                          "\"התחל\" (מפתח הפעלה). ממסרי המיון (2-4) ממשיכים לפעול "
                          "כרגיל בסוף כל שקילה בכל מקרה.",
                  style="Muted.TLabel", wraplength=420).grid(
            row=4, column=0, columnspan=3, sticky="w", **pad)

        ttk.Separator(t, orient="horizontal").grid(
            row=5, column=0, columnspan=3, sticky="ew", pady=(6, 6))

        ttk.Label(t, text="Port:").grid(row=6, column=0, sticky="e", **pad)
        self.relay_port_var = tk.StringVar(value=self.main.relay_port)
        self.relay_port_combo = ttk.Combobox(t, textvariable=self.relay_port_var,
                                             width=30, state="readonly")
        self.relay_port_combo.grid(row=6, column=1, **pad)
        ttk.Button(t, text="⟳", width=3, command=self._refresh_relay_ports).grid(
            row=6, column=2, **pad)

        ttk.Label(t, text="Baud:").grid(row=7, column=0, sticky="e", **pad)
        self.relay_baud_var = tk.StringVar(value=self.main.relay_baud)
        ttk.Combobox(t, textvariable=self.relay_baud_var, state="readonly", width=10,
                     values=["1200", "2400", "4800", "9600", "19200", "38400",
                             "57600", "115200"]).grid(row=7, column=1, sticky="w", **pad)

        self.relay_status_var = tk.StringVar()
        ttk.Label(t, textvariable=self.relay_status_var,
                  style="Muted.TLabel").grid(row=8, column=0, columnspan=3, sticky="w", **pad)

        relay_btn_frame = ttk.Frame(t)
        relay_btn_frame.grid(row=9, column=0, columnspan=3, sticky="w", **pad)
        self.btn_relay_connect = ttk.Button(relay_btn_frame, text="התחבר",
                                            style="Accent.TButton", command=self._on_relay_connect)
        self.btn_relay_connect.pack(side="left", padx=4)
        self.btn_relay_disconnect = ttk.Button(relay_btn_frame, text="התנתק",
                                               command=self._on_relay_disconnect)
        self.btn_relay_disconnect.pack(side="left", padx=4)

        ttk.Separator(t, orient="horizontal").grid(
            row=10, column=0, columnspan=3, sticky="ew", pady=(10, 6))

        ttk.Label(t, text="זמן הפעלת ממסר מיון (שניות):").grid(row=11, column=0, sticky="e", **pad)
        self.relay_pulse_var = tk.DoubleVar(value=self.main.relay_engine.settings.sort_pulse_seconds)
        ttk.Spinbox(t, textvariable=self.relay_pulse_var, from_=0.2, to=30.0,
                    increment=0.1, width=8, format="%.1f").grid(row=11, column=1, sticky="w", **pad)
        ttk.Label(t, text="ממסר המיון נכבה קודם זמן זה אם מיון חדש מתחיל",
                  style="Muted.TLabel").grid(row=12, column=0, columnspan=2, sticky="w", **pad)
        self.relay_pulse_var.trace_add("write", lambda *a: self._apply_relay_pulse_duration())

        self._refresh_relay_ports()
        self._sync_relay_connection_state()

    def _refresh_relay_ports(self):
        ports = engine_mod.list_serial_ports()
        values = [f"{dev} | {desc}" for dev, desc in ports] if ports else ["(לא נמצאו פורטים)"]
        self.relay_port_combo.config(values=values)
        if self.main.relay_port:
            for v in values:
                if v.startswith(self.main.relay_port):
                    self.relay_port_var.set(v)
                    return
        if values:
            self.relay_port_combo.current(0)

    def _sync_relay_connection_state(self):
        connected = self.main.relay_engine.connected
        self.relay_status_var.set("מחובר" if connected else "מנותק")
        self.btn_relay_connect.config(state="disabled" if connected else "normal")
        self.btn_relay_disconnect.config(state="normal" if connected else "disabled")
        self.relay_port_combo.config(state="disabled" if connected else "readonly")

    def _on_relay_enabled_toggle(self):
        self.main.relay_enabled = self.relay_enabled_var.get()
        self.main.save_settings()
        self.main._sync_relay_sensor_mode()
        self.main._sync_relay_ui()

    def _on_start_trigger_changed(self, event=None):
        key = self._start_trigger_display_to_key.get(self.start_trigger_var.get())
        if key is None:
            return
        self.main.start_trigger = key
        self.main.save_settings()
        self.main._sync_relay_sensor_mode()

    def _on_stop_trigger_changed(self, event=None):
        key = self._stop_trigger_display_to_key.get(self.stop_trigger_var.get())
        if key is None:
            return
        self.main.stop_trigger = key
        self.main.save_settings()
        self.main._sync_relay_sensor_mode()

    def _on_relay_connect(self):
        if not engine_mod.SERIAL_AVAILABLE:
            messagebox.showerror("שגיאה", "pyserial לא מותקן — הרץ: pip install pyserial")
            return
        port_name = self.relay_port_var.get().split("|")[0].strip()
        if not port_name or port_name.startswith("("):
            messagebox.showerror("שגיאה", "לא נבחר פורט תקין — לחץ ⟳ לרענון")
            return
        try:
            self.main.connect_relays(port_name, self.relay_baud_var.get())
        except Exception as e:
            messagebox.showerror("שגיאת חיבור", str(e))
            return
        self._sync_relay_connection_state()

    def _on_relay_disconnect(self):
        self.main.disconnect_relays()
        self._sync_relay_connection_state()

    def _apply_relay_pulse_duration(self):
        try:
            self.main.relay_engine.settings.sort_pulse_seconds = float(self.relay_pulse_var.get())
        except (tk.TclError, ValueError):
            return
        self.main.save_settings()

    # ──────────────────────────────────────────────
    # גיבוי אוטומטי — ראו backup.py (המנגנון) ו-main_window.py (התזמון היומי)
    # ──────────────────────────────────────────────

    def _build_backup_tab(self):
        t = self.tab_backup
        pad = {"padx": 6, "pady": 6}

        self.backup_enabled_var = tk.BooleanVar(value=self.main.backup_enabled)
        ttk.Checkbutton(t, text="גיבוי אוטומטי יומי", variable=self.backup_enabled_var,
                        command=self._on_backup_enabled_toggle).grid(
            row=0, column=0, columnspan=3, sticky="w", **pad)
        ttk.Label(t, text="גיבוי אחד ליום (לפי תאריך) — נבדק בעלייה ותוך כדי ריצה, "
                          f"נשמרים {backup_mod.BACKUP_RETENTION_COUNT} הגיבויים האחרונים בלבד.",
                  style="Muted.TLabel").grid(row=1, column=0, columnspan=3, sticky="w", **pad)

        ttk.Label(t, text="תיקיית גיבוי:").grid(row=2, column=0, sticky="e", **pad)
        self.backup_dir_var = tk.StringVar(value=self.main.backup_dir)
        ttk.Entry(t, textvariable=self.backup_dir_var, width=45,
                 state="readonly").grid(row=2, column=1, sticky="w", **pad)
        ttk.Button(t, text="בחר תיקייה...", command=self._on_choose_backup_dir).grid(
            row=2, column=2, **pad)

        self.backup_status_var = tk.StringVar()
        ttk.Label(t, textvariable=self.backup_status_var,
                  style="Muted.TLabel").grid(row=3, column=0, columnspan=3, sticky="w", **pad)

        ttk.Button(t, text="גבה עכשיו", style="Accent.TButton",
                  command=self._on_backup_now).grid(row=4, column=0, sticky="w", **pad)

        self._refresh_backup_status()

    def _refresh_backup_status(self):
        parts = []
        parts.append(f"גיבוי אחרון: {self.main.backup_last_date}" if self.main.backup_last_date
                     else "עוד לא בוצע גיבוי")
        if self.main.backup_last_error:
            parts.append(f"שגיאה אחרונה: {self.main.backup_last_error}")
        self.backup_status_var.set("   |   ".join(parts))

    def _on_backup_enabled_toggle(self):
        self.main.backup_enabled = self.backup_enabled_var.get()
        self.main.save_settings()

    def _on_choose_backup_dir(self):
        chosen = filedialog.askdirectory(initialdir=self.main.backup_dir or ".")
        if not chosen:
            return
        self.main.backup_dir = chosen
        self.backup_dir_var.set(chosen)
        self.main.save_settings()

    def _on_backup_now(self):
        ok, message = self.main.run_backup_now()
        self._refresh_backup_status()
        (messagebox.showinfo if ok else messagebox.showerror)("גיבוי", message)

    # ──────────────────────────────────────────────
    # דשבורד רשת מקומית — ראו lan_dashboard.py
    # ──────────────────────────────────────────────

    def _build_dashboard_tab(self):
        t = self.tab_dashboard
        pad = {"padx": 6, "pady": 6}

        self.dashboard_enabled_var = tk.BooleanVar(value=self.main.lan_dashboard_enabled)
        ttk.Checkbutton(t, text="הפעל דשבורד רשת מקומית", variable=self.dashboard_enabled_var,
                        command=self._on_dashboard_enabled_toggle).grid(
            row=0, column=0, columnspan=3, sticky="w", **pad)
        ttk.Label(t, text="עמוד קריאה-בלבד (סטטוס חיבור, משקל חי, סיכום היום) — נגיש מכל "
                          "מכשיר באותה רשת מקומית (טלפון וכו'). אין בו אף פעולת שליטה "
                          "או הגדרה — רק תצוגה.",
                  style="Muted.TLabel", wraplength=520,
                  justify="right").grid(row=1, column=0, columnspan=3, sticky="w", **pad)

        ttk.Label(t, text="פורט:").grid(row=2, column=0, sticky="e", **pad)
        self.dashboard_port_var = tk.IntVar(value=self.main.lan_dashboard_port)
        port_spin = ttk.Spinbox(t, textvariable=self.dashboard_port_var, from_=1024, to=65535,
                                width=8)
        port_spin.grid(row=2, column=1, sticky="w", **pad)
        # מגיב רק על עזיבת השדה/Enter, לא על כל הקשה — אחרת כל ספרה שמוקלדת
        # (למשל "8" -> "80" -> "809" -> "8090") הייתה מפעילה מחדש את השרת.
        port_spin.bind("<FocusOut>", lambda e: self._on_dashboard_port_changed())
        port_spin.bind("<Return>", lambda e: self._on_dashboard_port_changed())

        self.dashboard_status_var = tk.StringVar()
        ttk.Label(t, textvariable=self.dashboard_status_var,
                  style="Muted.TLabel").grid(row=3, column=0, columnspan=3, sticky="w", **pad)

        self._refresh_dashboard_status()

    def _refresh_dashboard_status(self):
        if self.main.lan_dashboard.running:
            url = f"http://{lan_dashboard.local_lan_ip()}:{self.main.lan_dashboard_port}/"
            self.dashboard_status_var.set(f"פעיל — פתח בטלפון/מחשב אחר: {url}")
        else:
            self.dashboard_status_var.set("כבוי")

    def _on_dashboard_enabled_toggle(self):
        self.main.lan_dashboard_enabled = self.dashboard_enabled_var.get()
        self.main.save_settings()
        if self.main.lan_dashboard_enabled:
            self.main.start_lan_dashboard()
        else:
            self.main.stop_lan_dashboard()
        self._refresh_dashboard_status()

    def _on_dashboard_port_changed(self):
        try:
            port = int(self.dashboard_port_var.get())
        except (tk.TclError, ValueError):
            return
        if port == self.main.lan_dashboard_port:
            return
        self.main.lan_dashboard_port = port
        self.main.save_settings()
        if self.main.lan_dashboard.running:
            # שינוי פורט תוך כדי ריצה — מפעילים מחדש על הפורט החדש.
            self.main.stop_lan_dashboard()
            self.main.start_lan_dashboard()
        self._refresh_dashboard_status()

    # ──────────────────────────────────────────────
    # בדיקת עדכון גרסה מרוחקת — ראו update_check.py
    # ──────────────────────────────────────────────

    def _build_updates_tab(self):
        t = self.tab_updates
        pad = {"padx": 6, "pady": 6}

        self.update_enabled_var = tk.BooleanVar(value=self.main.update_check_enabled)
        ttk.Checkbutton(t, text="בדוק עדכונים אוטומטית", variable=self.update_enabled_var,
                        command=self._on_update_enabled_toggle).grid(
            row=0, column=0, columnspan=2, sticky="w", **pad)
        ttk.Label(t, text="בדיקה אחת ליום (git fetch, לא pull) — נבדק בעלייה ותוך כדי ריצה. "
                          "אף פעם לא מעדכן בפועל בלי לחיצה מפורשת על \"עדכן\" למטה.",
                  style="Muted.TLabel", wraplength=520,
                  justify="right").grid(row=1, column=0, columnspan=2, sticky="w", **pad)

        self.update_status_var = tk.StringVar()
        ttk.Label(t, textvariable=self.update_status_var, style="Muted.TLabel",
                 wraplength=520, justify="right").grid(
            row=2, column=0, columnspan=2, sticky="w", **pad)

        btns = ttk.Frame(t)
        btns.grid(row=3, column=0, columnspan=2, sticky="w", **pad)
        ttk.Button(btns, text="בדוק עכשיו", command=self._on_check_update_now).pack(
            side="left", padx=4)
        self.btn_apply_update = ttk.Button(btns, text="עדכן (git pull)",
                                           style="Accent.TButton",
                                           command=self._on_apply_update)
        self.btn_apply_update.pack(side="left", padx=4)

        self.refresh_update_status()

    def refresh_update_status(self):
        """ נקרא ע"י MainWindow אחרי כל תוצאת בדיקה (גם אוטומטית, גם ידנית). """
        status = self.main._update_status
        if status.get("ok") is None:
            self.update_status_var.set("עוד לא נבדק")
        elif not status.get("ok"):
            self.update_status_var.set(f"הבדיקה נכשלה: {status.get('error', '')}")
        elif status.get("update_available"):
            self.update_status_var.set(
                f"יש עדכון חדש ({status.get('commits_behind')} commits): "
                f"{status.get('latest_summary', '')}")
        else:
            self.update_status_var.set("התוכנה מעודכנת לגרסה האחרונה")
        self.btn_apply_update.config(
            state="normal" if status.get("ok") and status.get("update_available") else "disabled")

    def _on_update_enabled_toggle(self):
        self.main.update_check_enabled = self.update_enabled_var.get()
        self.main.save_settings()

    def _on_check_update_now(self):
        self.update_status_var.set("בודק...")
        self.main.check_for_update_now()

    def _on_apply_update(self):
        if not messagebox.askyesno("עדכון", "להוריד ולהתקין את העדכון (git pull)? "
                                            "יש לסגור ולהפעיל מחדש את התוכנה בסיום."):
            return
        self.btn_apply_update.config(state="disabled")
        self.update_status_var.set("מעדכן...")
        self.main.apply_update_now()

    def handle_update_apply_result(self, result):
        """ נקרא ע"י MainWindow._on_update_apply_result. """
        if result.get("ok"):
            messagebox.showinfo("עדכון", "העדכון הותקן בהצלחה — סגור והפעל מחדש "
                                        "את התוכנה כדי שהשינויים יחולו.")
        else:
            messagebox.showerror("עדכון", f"העדכון נכשל:\n{result.get('error', '')}")
        self.refresh_update_status()

    # ──────────────────────────────────────────────
    # שקילה
    # ──────────────────────────────────────────────

    def _build_weighing_tab(self):
        t = self.tab_weigh
        pad = {"padx": 6, "pady": 6}
        s = self.main.engine.settings

        self.mode_var = tk.StringVar(value="listen" if s.listen else "poll")
        ttk.Radiobutton(t, text="האזנה — ראש השקילה שולח כל הזמן", variable=self.mode_var,
                        value="listen", command=self._apply_weighing).grid(
            row=0, column=0, columnspan=2, sticky="w", **pad)
        ttk.Radiobutton(t, text="שליחת W — בקשה/תשובה", variable=self.mode_var,
                        value="poll", command=self._apply_weighing).grid(
            row=1, column=0, columnspan=2, sticky="w", **pad)

        ttk.Label(t, text="סף הפעלה (kg):").grid(row=2, column=0, sticky="e", **pad)
        self.threshold_var = tk.DoubleVar(value=s.threshold)
        ttk.Spinbox(t, textvariable=self.threshold_var, from_=0.0, to=9999.0,
                    increment=0.1, width=10, format="%.2f").grid(row=2, column=1, sticky="w", **pad)

        ttk.Label(t, text="דיליי סיום (שניות):").grid(row=3, column=0, sticky="e", **pad)
        self.debounce_var = tk.DoubleVar(value=s.debounce_s)
        ttk.Spinbox(t, textvariable=self.debounce_var, from_=0.1, to=10.0,
                    increment=0.1, width=10, format="%.1f").grid(row=3, column=1, sticky="w", **pad)

        ttk.Label(t, text="מעל הסף → מתחילה שקילה  |  מתחת לסף ברצף → מסתיימת ונרשמת",
                  style="Muted.TLabel").grid(row=4, column=0, columnspan=2, sticky="w", **pad)

        ttk.Label(t, text="משקל מינימלי לשמירה (kg):").grid(row=5, column=0, sticky="e", **pad)
        self.min_save_var = tk.DoubleVar(value=s.min_save_weight)
        ttk.Spinbox(t, textvariable=self.min_save_var, from_=0.0, to=9999.0,
                    increment=0.01, width=10, format="%.3f").grid(row=5, column=1, sticky="w", **pad)

        ttk.Label(t, text="שקילות מתחת לערך זה יוצגו אך לא יישמרו בהיסטוריה",
                  style="Muted.TLabel").grid(row=6, column=0, columnspan=2, sticky="w", **pad)

        self.threshold_var.trace_add("write", lambda *a: self._apply_weighing())
        self.debounce_var.trace_add("write", lambda *a: self._apply_weighing())
        self.min_save_var.trace_add("write", lambda *a: self._apply_weighing())

    def _apply_weighing(self):
        s = self.main.engine.settings
        s.listen = self.mode_var.get() == "listen"
        try:
            s.threshold = float(self.threshold_var.get())
        except (tk.TclError, ValueError):
            pass
        try:
            s.debounce_s = float(self.debounce_var.get())
        except (tk.TclError, ValueError):
            pass
        try:
            s.min_save_weight = float(self.min_save_var.get())
        except (tk.TclError, ValueError):
            pass
        self.main.save_settings()

    # ──────────────────────────────────────────────
    # דיאגנוסטיקה
    # ──────────────────────────────────────────────

    def _build_diagnostics_tab(self):
        t = self.tab_diag
        pad = {"padx": 6, "pady": 6}

        self.show_each_var = tk.BooleanVar(value=self.main.engine.settings.show_each)
        ttk.Checkbutton(t, text="הצג כל קריאה בלוג", variable=self.show_each_var,
                        command=self._apply_show_each).grid(
            row=0, column=0, columnspan=4, sticky="w", **pad)

        ttk.Label(t, text="דגימה חד-פעמית:").grid(row=1, column=0, sticky="e", **pad)
        self.sample_duration_var = tk.DoubleVar(value=self.main.sample_duration)
        ttk.Spinbox(t, textvariable=self.sample_duration_var, from_=0.1, to=60.0,
                    increment=0.1, width=8, format="%.1f").grid(row=1, column=1, sticky="w", **pad)
        ttk.Label(t, text="שניות").grid(row=1, column=2, sticky="w")
        self.btn_sample = ttk.Button(t, text="▶ התחל דגימה", command=self._on_sample)
        self.btn_sample.grid(row=1, column=3, sticky="w", **pad)

        ttk.Label(t, text="LIVE — חלון:").grid(row=2, column=0, sticky="e", **pad)
        self.live_window_var = tk.DoubleVar(value=self.main.live_window)
        ttk.Spinbox(t, textvariable=self.live_window_var, from_=0.1, to=60.0,
                    increment=0.1, width=8, format="%.1f").grid(row=2, column=1, sticky="w", **pad)
        ttk.Label(t, text="שניות").grid(row=2, column=2, sticky="w")
        self.btn_live = ttk.Button(t, text="▶ התחל LIVE", command=self._on_live_toggle)
        self.btn_live.grid(row=2, column=3, sticky="w", **pad)

        self.btn_calib = ttk.Button(t, text="כיול Swan (PC0035)", command=self._open_calibration)
        self.btn_calib.grid(row=3, column=0, columnspan=4, sticky="w", pady=(4, 10), padx=6)

        ttk.Label(t, text="לוג תקשורת גולמי:").grid(row=4, column=0, columnspan=4, sticky="w", padx=6)
        self.log_box = scrolledtext.ScrolledText(t, height=12, state="disabled",
                                                  font=(self.main.fonts["mono"], 9))
        self.log_box.grid(row=5, column=0, columnspan=4, sticky="nsew", padx=6, pady=(0, 6))
        self._apply_log_colors()
        t.rowconfigure(5, weight=1)
        t.columnconfigure(3, weight=1)

        self._sync_diag_buttons()

    def _apply_log_colors(self):
        self.log_box.config(background=theme.CARD_BG, foreground=theme.TEXT,
                            insertbackground=theme.TEXT)
        self.log_box.tag_config("reading", foreground=theme.TEXT)
        self.log_box.tag_config("error", foreground=theme.RED)
        self.log_box.tag_config("warn", foreground="#b56b00")
        self.log_box.tag_config("summary", foreground=theme.ACCENT,
                                 font=(self.main.fonts["mono"], 9, "bold"))

    def _sync_diag_buttons(self):
        connected = self.main.engine.connected
        self.btn_sample.config(state="normal" if connected else "disabled")
        self.btn_live.config(state="normal" if connected else "disabled")
        self.btn_calib.config(state="normal" if connected else "disabled")

    def refresh_theme(self):
        self.configure(background=theme.BG)
        self._apply_log_colors()
        self.baud_warning_label.configure(foreground=theme.RED)
        self.reset_warning_label.configure(foreground=theme.RED)
        if self._calib_dialog is not None and self._calib_dialog.winfo_exists():
            self._calib_dialog.refresh_theme()

    def _on_tab_changed(self, _event=None):
        """
        "פרטי מכשיר"/"הגדרות משקל"/"איפוס יצרן" שולחות פקודות ישירות למשקל
        שלא יכולות לחלוק את הפורט עם השידור הרציף — ראו
        engine.pause_device_access(). המעבר לטאבים האלה עוצר את השידור; המעבר
        חזרה (לכל טאב אחר) או סגירת החלון (destroy) מפעיל אותו מחדש.
        """
        current = self.nb.select()
        is_device_tab = current in (str(self.tab_device_info), str(self.tab_scale_config),
                                    str(self.tab_factory_reset))
        if is_device_tab and not self._device_tab_active:
            self.main.engine.pause_device_access()
            self._device_tab_active = True
            self._set_busy(False)
        elif not is_device_tab and self._device_tab_active:
            self.main.engine.resume_after_device_access()
            self._device_tab_active = False

    # ──────────────────────────────────────────────
    # תצורת משקל — פקודות ישירות מ-Swan_SC_Protocol.md (NVRAM המשקל, לא הגדרות
    # האפליקציה). כל פרמטר נקרא בנפרד (כפתור "קרא" משלו), אין שרשור מרובה-שלבים.
    # ──────────────────────────────────────────────

    def _build_device_info_tab(self):
        t = self.tab_device_info

        info_frame = ttk.LabelFrame(t, text="פרטי מכשיר")
        info_frame.pack(fill="x")
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

    def _build_scale_config_tab(self):
        t = self.tab_scale_config

        settings_frame = ttk.LabelFrame(t, text="הגדרות משקל")
        settings_frame.pack(fill="x", pady=6)
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

        # שם נפרד בכוונה מ-baud_var (טאב חיבור) — זה קצב ה-baud של המשקל עצמו
        # (NVRAM שלו, נקרא/נכתב ב-B), לא קצב הפורט של האפליקציה. שיתוף השם גרם
        # לדריסה שקטה של baud_var של טאב החיבור (אותה מחלקה, __init__ אחד) —
        # הקומבו שם התרוקן ו-int('') קרס ב-connect(). נצפה בפועל.
        self.device_baud_var = tk.StringVar()
        self.device_baud_combo = ttk.Combobox(settings_frame, textvariable=self.device_baud_var,
                                              values=BAUD_LABELS, state="readonly", width=10)
        self.btn_read_baud, self.btn_baud = self._add_settable_row(
            settings_frame, 5, "קצב תקשורת (Baud):", self.device_baud_combo,
            self.engine.query_baud, self._on_set_baud)

        actions_frame = ttk.LabelFrame(t, text="פעולות")
        actions_frame.pack(fill="x", pady=6)
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

    def _build_factory_reset_tab(self):
        t = self.tab_factory_reset
        reset_frame = ttk.LabelFrame(t, text="איפוס יצרן — פעולה הרסנית")
        reset_frame.pack(fill="x")
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
        self.reset_confirm_var.trace_add("write", lambda *a: self._sync_reset_button())

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
        self._sync_reset_button()

    def _sync_reset_button(self):
        reset_ok = (self.reset_confirm_var.get() == "RESET" and self.engine.connected
                   and self.btn_zero.cget("state") != "disabled")
        self.btn_factory_reset.config(state="normal" if reset_ok else "disabled")

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
        if self.device_baud_var.get() not in BAUD_LABELS:
            return
        idx = BAUD_LABELS.index(self.device_baud_var.get())
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

    def _apply_show_each(self):
        self.main.engine.settings.show_each = self.show_each_var.get()
        self.main.save_settings()

    def _log(self, msg, tag="reading"):
        ts = time.strftime("%H:%M:%S")
        self.log_box.config(state="normal")
        self.log_box.insert("end", f"{ts}  {msg}\n", tag)
        self.log_box.see("end")
        self.log_box.config(state="disabled")

    def _on_sample(self):
        self.main.sample_duration = self.sample_duration_var.get()
        self.main.save_settings()
        self.btn_sample.config(state="disabled")
        self._log("─" * 50, "summary")
        self._log(f"מתחיל דגימה — {self.main.sample_duration:.1f} שניות", "summary")
        self.main.engine.sample_once(self.main.sample_duration)

    def _on_live_toggle(self):
        if self._live_running:
            self.main.engine.stop_live()
        else:
            self.main.live_window = self.live_window_var.get()
            self.main.save_settings()
            self._live_running = True
            self.btn_live.config(text="⏹ עצור LIVE")
            self._log("─" * 50, "summary")
            self._log(f"LIVE — חלון {self.main.live_window:.1f} שניות", "summary")
            self.main.engine.start_live(self.main.live_window)

    def _open_calibration(self):
        # לא רק מפסיק לקרוא — גם משתיק את השידור בפועל על המשקל, אחרת חבילות משקל
        # ('S') ממשיכות לזרום ומבלבלות את קריאת התגובות של פקודות הכיול.
        self.main.engine.pause_device_access()
        self.btn_sample.config(state="disabled")
        self.btn_live.config(state="disabled")
        self.btn_calib.config(state="disabled")
        self._calib_dialog = CalibrationDialog(self)

    # ── אירועי engine, מועברים מ-MainWindow._pump_queue ──
    def handle_engine_event(self, kind, payload):
        if kind == "log":
            self._log(*payload)
        elif kind == "sample_done":
            readings, bad, elapsed = payload
            self.btn_sample.config(state="normal" if self.main.engine.connected else "disabled")
            self._summarize_sample(readings, bad, elapsed)
        elif kind == "live_tick":
            self._on_live_tick(*payload)
        elif kind == "live_stopped":
            self._live_running = False
            self.btn_live.config(text="▶ התחל LIVE")
            self._log("LIVE הסתיים", "summary")
        elif kind == "calib":
            if self._calib_dialog is not None and self._calib_dialog.winfo_exists():
                self._calib_dialog.handle_update(*payload)
        elif kind in ("relay_connected", "relay_disconnected", "relay_connection_lost"):
            self._sync_relay_connection_state()
            if kind == "relay_connection_lost":
                messagebox.showerror("ממסרים", "החיבור לכרטיס הממסרים אבד — "
                                      "חזרה אוטומטית למצב סף-משקל.")
        elif kind == "relay_error":
            self.relay_status_var.set(f"מחובר — שגיאה אחרונה: {payload}")
        elif kind in ("device_full_scale", "device_full_scale_set"):
            if payload.get("ok"):
                self.full_scale_var.set(f"{payload['grams'] / 1000:.3f}")
            else:
                messagebox.showerror("שגיאה", payload.get("error", "שגיאה לא ידועה"))
            self._set_busy(False)
        elif kind == "device_baud":
            # "Baud=XXXXXXX" מדווח את קצב ה-baud הנוכחי בפועל (למשל 9600), לא אינדקס.
            if payload.get("ok"):
                baud = payload.get("baud")
                self.device_baud_var.set(str(baud) if baud is not None else "—")
            else:
                messagebox.showerror("שגיאה", payload.get("error", "שגיאה"))
            self._set_busy(False)
        elif kind == "device_baud_set":
            if payload.get("ok") and payload.get("index") is not None:
                self.device_baud_var.set(self._safe_label(BAUD_LABELS, payload["index"], None))
            elif not payload.get("ok"):
                messagebox.showerror("שגיאה", payload.get("error", "שגיאה"))
            if payload.get("ok"):
                messagebox.showinfo("בוצע", "הפקודה נשלחה. המשקל מתאתחל — התחבר מחדש "
                                            "בקצב החדש דרך הגדרות.")
            self._set_busy(False)
        elif kind in ("device_rounding", "device_rounding_set"):
            if payload.get("ok") and payload.get("index") is not None:
                self.round_var.set(self._safe_label(ROUND_LABELS, payload["index"], None))
            elif not payload.get("ok"):
                messagebox.showerror("שגיאה", payload.get("error", "שגיאה"))
            self._set_busy(False)
        elif kind in ("device_disform", "device_disform_set"):
            if payload.get("ok") and payload.get("index") is not None:
                self.disform_var.set(self._safe_label(DISFORM_LABELS, payload["index"], None))
            elif not payload.get("ok"):
                messagebox.showerror("שגיאה", payload.get("error", "שגיאה"))
            self._set_busy(False)
        elif kind == "device_zero":
            self.action_status_var.set("אפס בוצע ✓" if payload.get("ok")
                                       else f"אפס נכשל: {payload.get('error', '')}")
            self._set_busy(False)
        elif kind == "device_tare":
            self.action_status_var.set("טרה בוצעה ✓" if payload.get("ok")
                                       else f"טרה נכשלה: {payload.get('error', '')}")
            self._set_busy(False)
        elif kind == "device_clear_tare":
            self.action_status_var.set("טרה בוטלה")
            self._set_busy(False)
        elif kind == "device_info":
            self.info_var.set(payload.get("text") or "—")
            self._set_busy(False)
        elif kind == "device_version":
            self.version_var.set(payload.get("text") or "—")
            self._set_busy(False)
        elif kind == "device_raw_ad":
            self.raw_ad_var.set(payload.get("text") or "—")
            self._set_busy(False)
        elif kind == "device_factory_reset":
            if not payload.get("ok"):
                messagebox.showerror("שגיאה", payload.get("error", "שגיאה"))
            self._set_busy(False)

    def _summarize_sample(self, readings, bad, elapsed):
        if not readings:
            self._log(f"לא התקבלו קריאות תקינות ({elapsed:.2f}s, פסולת: {bad})", "error")
            return
        decided, basis, _win_start, _win_end = decide_weight(readings)
        weights = [w for _, w, _ in readings]
        rate = len(weights) / elapsed if elapsed > 0 else 0
        self._log(f"סיכום: {len(weights)} קריאות ב-{elapsed:.2f}s = {rate:.1f}/שנייה", "summary")
        self._log(f"        משקל שנקבע: {fmt_weight(decided, force_sign=True)}   ({basis})", "summary")
        self._log(f"        מין/מקס: {fmt_weight(min(weights), force_sign=True)} / "
                  f"{fmt_weight(max(weights), force_sign=True)}")
        if bad:
            self._log(f"        מסגרות פסולות: {bad}", "warn")

    def _on_live_tick(self, tick_num, wall_start, wall_end, readings, elapsed):
        decided, basis, _win_start, _win_end = decide_weight(readings)
        weights = [w for _, w, _ in readings]
        rate = len(weights) / elapsed if elapsed > 0 else 0
        self._log(f"  #{tick_num:>3}  {wall_end.strftime('%H:%M:%S')}   "
                  f"{len(weights):>4} קריאות @ {rate:.1f}/s   "
                  f"{fmt_weight(decided, force_sign=True)} kg", "summary")

    def destroy(self):
        if self._live_running:
            self.main.engine.stop_live()
        if self._device_tab_active:
            self.main.engine.resume_after_device_access()
        super().destroy()


class CalibrationDialog(tk.Toplevel):
    """ אשף כיול Swan PC0035: הסרת משטח → כיול אפס → כיול משקל → שמירה+איתחול. """

    def __init__(self, settings_window: SettingsWindow):
        super().__init__(settings_window)
        self.settings_window = settings_window
        self.engine = settings_window.main.engine
        self.title("כיול Swan — PC0035")
        theme.maximize_window(self)
        self.grab_set()
        self.focus_set()
        self.minsize(440, 420)
        self.configure(background=theme.BG)

        # החלון ממוקסם כמו כל שאר החלונות, אבל התוכן קצר — ממורכז ב-container
        # אחד באמצע המסך, לא נגרר בפינה של שטח ריק.
        container = ttk.Frame(self)
        container.place(relx=0.5, rely=0.4, anchor="center")

        frm_top = ttk.LabelFrame(container, text="הגדרות כיול")
        frm_top.pack(fill="x", padx=12, pady=(12, 6))
        ttk.Label(frm_top, text="משקל כיול:").grid(row=0, column=0, padx=8, pady=6, sticky="e")
        self.calib_weight_var = tk.IntVar(value=3000)
        self.spn_w = ttk.Spinbox(frm_top, textvariable=self.calib_weight_var,
                                  from_=100, to=99999, increment=100, width=9, format="%d")
        self.spn_w.grid(row=0, column=1, padx=4, pady=6, sticky="w")
        ttk.Label(frm_top, text="גרמים").grid(row=0, column=2, sticky="w")

        frm_st = ttk.LabelFrame(container, text="מצב")
        frm_st.pack(fill="x", padx=12, pady=6)
        self.step_var = tk.StringVar(value="הכנס משקל כיול ולחץ 'התחל כיול'")
        self.step_label = ttk.Label(frm_st, textvariable=self.step_var, font=("Arial", 11, "bold"),
                                    foreground=theme.ACCENT_DARK, wraplength=390, justify="right",
                                    anchor="w")
        self.step_label.pack(padx=10, pady=(10, 4), fill="x")
        self.prog_var = tk.StringVar(value="")
        self.prog_label = ttk.Label(frm_st, textvariable=self.prog_var, font=("Consolas", 9),
                                    foreground=theme.TEXT_MUTED)
        self.prog_label.pack(padx=10, pady=(0, 4))
        self.pb = ttk.Progressbar(frm_st, orient="horizontal", length=400, maximum=14)
        self.pb.pack(padx=10, pady=(0, 10))

        frm_resp = ttk.LabelFrame(container, text="תגובת המאזניים")
        frm_resp.pack(fill="x", padx=12, pady=6)
        self.resp_box = scrolledtext.ScrolledText(frm_resp, height=4, state="disabled",
                                                    font=("Consolas", 8))
        self.resp_box.pack(padx=6, pady=6, fill="x")
        self._apply_resp_colors()

        frm_b = ttk.Frame(container)
        frm_b.pack(padx=12, pady=(4, 12))
        self.btn_act = ttk.Button(frm_b, text="התחל כיול", width=14, command=self._on_start)
        self.btn_act.pack(side="left", padx=6)
        self.btn_cls = ttk.Button(frm_b, text="סגור", width=10, command=self._close)
        self.btn_cls.pack(side="left", padx=6)

        # X בכותרת החלון מתנהג בדיוק כמו כפתור "סגור" — כולל אותה חסימה בזמן
        # שלב פעיל בכיול (btn_cls במצב disabled) — לא no-op גורף כמו קודם.
        # _close() מבטל את הכיול (calib_cancel) ומשחזר גישה למכשיר בכל מקרה,
        # אז אין סיכון חדש כאן, רק התאמה בין שני הדרכים לסגור את החלון.
        self.protocol("WM_DELETE_WINDOW", self._on_close_attempt)

    def _apply_resp_colors(self):
        self.resp_box.config(background=theme.CARD_BG, foreground=theme.TEXT,
                             insertbackground=theme.TEXT)

    def refresh_theme(self):
        self.configure(background=theme.BG)
        self.step_label.configure(foreground=theme.ACCENT_DARK)
        self.prog_label.configure(foreground=theme.TEXT_MUTED)
        self._apply_resp_colors()

    def _resp_log(self, txt):
        self.resp_box.config(state="normal")
        self.resp_box.insert("end", txt + "\n")
        self.resp_box.see("end")
        self.resp_box.config(state="disabled")

    def _on_close_attempt(self):
        """ נקרא מ-X בכותרת החלון — מתעלם בזמן שלב פעיל בכיול, בדיוק כמו
        btn_cls המכובה (disabled) באותם רגעים (ראו _on_start/_on_continue). """
        if str(self.btn_cls["state"]) == "disabled":
            return
        self._close()

    def _close(self):
        self.engine.calib_cancel()
        self.engine.resume_after_device_access()
        try:
            self.settings_window._sync_diag_buttons()
        except Exception:
            pass
        self.destroy()

    def _on_continue(self):
        self.engine.calib_continue()
        self.btn_act.config(state="disabled", text="ממתין...")
        self.btn_cls.config(state="disabled")

    def _on_start(self):
        w = self.calib_weight_var.get()
        self.btn_act.config(state="disabled")
        self.spn_w.config(state="disabled")
        self.btn_cls.config(state="disabled")
        self.engine.start_calibration(w)

    def handle_update(self, kind, data):
        if kind == "status":
            self.step_var.set(str(data))
        elif kind == "progress":
            n = int(data)
            self.pb["value"] = min(n, 14)
            self.prog_var.set("*" * min(n, 14) + f"  ({n}/14)")
        elif kind == "response":
            self._resp_log(str(data))
        elif kind == "step":
            self.pb["value"] = 0
            self.prog_var.set("")
            name = data[0] if data else ""
            if name == "clear_platform":
                self.step_var.set("הסר הכל מהמשטח ולחץ 'המשך'")
                self.btn_act.config(state="normal", text="המשך ▶", command=self._on_continue)
                self.btn_cls.config(state="disabled")
            elif name == "put_weight":
                wg = data[1] if len(data) > 1 else "?"
                self.step_var.set(f"הנח {wg}g על המשטח ולחץ 'המשך'")
                self.btn_act.config(state="normal", text="המשך ▶", command=self._on_continue)
                self.btn_cls.config(state="disabled")
            elif name == "confirm_save":
                self.step_var.set("כיול הושלם — לחץ 'המשך' לשמירה ואיתחול")
                self.btn_act.config(state="normal", text="המשך ▶", command=self._on_continue)
                self.btn_cls.config(state="disabled")
            elif name == "done":
                self.step_var.set("שמירה בוצעה — ממתין לאיתחול המאזניים...")
                self.btn_act.config(state="disabled")
                self.btn_cls.config(state="disabled")
                self.pb["value"] = 14
        elif kind == "complete":
            self.step_var.set("כיול הושלם בהצלחה ✓")
            self.prog_var.set("ניתן לסגור ולהתחבר מחדש")
            self.pb["value"] = 14
            self.btn_act.config(state="disabled")
            self.btn_cls.config(state="normal")
            self.protocol("WM_DELETE_WINDOW", self._close)
        elif kind == "error":
            self.step_var.set(f"שגיאה: {data}")
            self.prog_var.set("")
            self.btn_act.config(state="disabled")
            self.btn_cls.config(state="normal")
            self.protocol("WM_DELETE_WINDOW", self._close)
