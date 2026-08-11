"""
Scale Sampler - מאזני Swan (RS-232)

פרוטוקול קריאת משקל:
  TX: 'W'                                 (בייט בודד, בלי CR/LF)
  RX: <+/-><7 תווים — ספרות + נקודה אופציונלית><CR>

פרוטוקול כיול (Swan PC0035, ESC-based):
  ESC P <weight> ESC e   — התחלת כיול עם משקל יעד
  ESC N ESC e            — המשך לשלב הבא (אפס / משקל / שמירה)
"""

import tkinter as tk
from tkinter import ttk, scrolledtext
import threading
import queue
import time
import re
import statistics
import json
import datetime
from pathlib import Path

CONFIG_FILE = Path(__file__).parent / "scale_sampler_config.json"

try:
    import serial
    SERIAL_AVAILABLE = True
except ImportError:
    SERIAL_AVAILABLE = False


# ──────────────────────────────────────────────────────────────
# Frame parsing
# ──────────────────────────────────────────────────────────────

_SWAN_TEXT_RE = re.compile(r'^([+\-])([\d.]{7})$')


def parse_swan_frame(frame: str):
    """
    Swan text — [+/-] ואחריו 7 תווים (ספרות, ואולי נקודה אחת בפנים) ו-CR.
    לא משנה איפה הנקודה נמצאת (או אם בכלל קיימת) — פשוט קוראים את המספר.
    דוגמאות: '+000.010' וגם '+0000010' -> 0.010 kg
    """
    s = frame.strip("\r\n\x00")
    m = _SWAN_TEXT_RE.match(s)
    if not m:
        return None, "", None
    sign, body = m.groups()
    try:
        if "." in body:
            weight = float(sign + body)
        else:
            weight = int(body) / 1000.0
            if sign == "-":
                weight = -weight
    except ValueError:
        return None, "", None
    return weight, "", None


def parse_swan_frame_bytes(raw: bytes):
    """
    Swan PC0034 — פריים 18 בייט (ללא CR מוביל ו-LF סוגר):
    [S1] XX.XXX [G/N] RS [S2] XX.XXX RS [CS]
    מחזיר (weight|None, status_str, special|None)
    """
    if len(raw) < 17:
        return None, "", None

    status1 = chr(raw[0])
    if status1 == "H":
        return None, "", "OVL"
    if status1 == "-":
        return None, "", "UNDER"

    try:
        weight = float(raw[1:7].decode("ascii"))
    except (ValueError, UnicodeDecodeError):
        return None, "", None

    gross_net = chr(raw[7]) if len(raw) > 7 else "G"
    status2   = chr(raw[9]) if len(raw) > 9 else "M"
    stability = "S" if status2 == "S" else "U"
    gn_str    = gross_net if gross_net in ("G", "N") else "G"
    return weight, f"{stability} {gn_str}", None


class ScaleSampler:
    # ראש השקילה משדר כל 50ms -> ~20Hz. ראה Print.c:56 + Init.c:86
    FIRMWARE_STREAM_HZ = 20

    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("Scale Sampler — Swan")
        self.conn = None
        self.running = False
        # תהליכון הדגימה לא נוגע ב-Tk כלל — הוא דוחף הודעות לתור הזה,
        # והתהליכון הראשי מרוקן אותו ב-_pump_queue.
        self.ui_queue: queue.Queue = queue.Queue()
        self._event_count = 0
        self._build_ui()
        self._load_settings()
        self._pump_queue()

    # ──────────────────────────────────────────────
    # UI
    # ──────────────────────────────────────────────

    def _build_ui(self):
        pad = {"padx": 8, "pady": 4}
        self.root.minsize(960, 520)

        # ── שתי עמודות ראשיות ──
        frm_left = ttk.Frame(self.root)
        frm_left.grid(row=0, column=0, sticky="ns", padx=(4, 0), pady=4)
        frm_right = ttk.Frame(self.root)
        frm_right.grid(row=0, column=1, sticky="nsew", padx=(0, 4), pady=4)
        self.root.columnconfigure(1, weight=1)
        self.root.rowconfigure(0, weight=1)
        frm_right.rowconfigure(0, weight=1)
        frm_right.columnconfigure(0, weight=3)
        frm_right.columnconfigure(1, weight=1)
        frm_left.columnconfigure(0, weight=1)

        # ── שמאל: מצב עבודה ──
        frm_mode = ttk.LabelFrame(frm_left, text="מצב עבודה")
        frm_mode.grid(row=0, column=0, sticky="ew", **pad)

        self.work_mode = tk.StringVar(value="listen")
        ttk.Radiobutton(frm_mode, text="האזנה — ראש השקילה שולח כל הזמן",
                        variable=self.work_mode, value="listen",
                        command=self._on_mode_change).grid(row=0, column=0, sticky="w", padx=8, pady=2)
        ttk.Radiobutton(frm_mode, text="שליחת W — בקשה/תשובה",
                        variable=self.work_mode, value="poll",
                        command=self._on_mode_change).grid(row=1, column=0, sticky="w", padx=8, pady=2)
        self.mode_hint = ttk.Label(frm_mode, text="", foreground="#666666",
                                    font=("Arial", 8), wraplength=380, justify="right")
        self.mode_hint.grid(row=2, column=0, sticky="w", padx=8, pady=(0, 4))

        # ── שמאל: הגדרות Serial ──
        self.frm_serial = ttk.LabelFrame(frm_left, text="הגדרות Serial")
        self.frm_serial.grid(row=1, column=0, sticky="ew", **pad)
        ttk.Label(self.frm_serial, text="Port:").grid(row=0, column=0, **pad)
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(self.frm_serial, textvariable=self.port_var,
                                        width=18, state="readonly")
        self.port_combo.grid(row=0, column=1, **pad)
        ttk.Button(self.frm_serial, text="⟳", width=3,
                   command=self._refresh_ports).grid(row=0, column=2, padx=(0, 4))
        ttk.Label(self.frm_serial, text="Baud:").grid(row=0, column=3, **pad)
        self.baud_var = tk.StringVar(value="9600")
        ttk.Combobox(self.frm_serial, textvariable=self.baud_var,
                     values=["600", "1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200"],
                     width=8, state="readonly").grid(row=0, column=4, **pad)

        # ── שמאל: הגדרות דגימה ──
        frm_set = ttk.LabelFrame(frm_left, text="הגדרות דגימה")
        frm_set.grid(row=4, column=0, sticky="ew", **pad)

        self.show_each = tk.BooleanVar(value=True)
        ttk.Checkbutton(frm_set, text="הצג כל קריאה",
                        variable=self.show_each).grid(row=0, column=0, columnspan=2, sticky="w", padx=8)

        self.frm_manual = ttk.Frame(frm_set)
        self.frm_manual.grid(row=1, column=0, columnspan=4, sticky="ew")
        ttk.Label(self.frm_manual, text="משך דגימה (שניות):").grid(row=0, column=0, **pad)
        self.duration_var = tk.DoubleVar(value=1.0)
        ttk.Spinbox(self.frm_manual, textvariable=self.duration_var,
                    from_=0.1, to=60.0, increment=0.1, width=7,
                    format="%.1f").grid(row=0, column=1, **pad)

        self.auto_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(frm_set, text="שקילה אוטומטית",
                        variable=self.auto_var,
                        command=self._on_auto_change).grid(row=2, column=0, columnspan=2,
                                                           sticky="w", padx=8, pady=2)
        self.live_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(frm_set, text="שקילה LIVE",
                        variable=self.live_var,
                        command=self._on_live_change).grid(row=2, column=2, columnspan=2,
                                                          sticky="w", padx=8, pady=2)
        self.frm_auto = ttk.Frame(frm_set)
        self.frm_auto.grid(row=3, column=0, columnspan=4, sticky="ew")
        ttk.Label(self.frm_auto, text="סף הפעלה (kg):").grid(row=0, column=0, **pad)
        self.threshold_var = tk.DoubleVar(value=0.5)
        ttk.Spinbox(self.frm_auto, textvariable=self.threshold_var,
                    from_=0.0, to=9999.0, increment=0.1, width=8,
                    format="%.2f").grid(row=0, column=1, **pad)
        ttk.Label(self.frm_auto,
                  text="מעל הסף → מתחיל | מתחת → עוצר ומציג",
                  foreground="#555").grid(row=0, column=2, padx=6)

        # ── שמאל: כפתורים ──
        frm_btns = ttk.Frame(frm_left)
        frm_btns.grid(row=5, column=0, **pad)
        self.btn_connect = ttk.Button(frm_btns, text="התחבר", command=self._connect, width=11)
        self.btn_connect.pack(side="left", padx=3)
        self.btn_start = ttk.Button(frm_btns, text="▶ התחל דגימה",
                                    command=self._start_sampling, state="disabled", width=14)
        self.btn_start.pack(side="left", padx=3)
        self.btn_disconnect = ttk.Button(frm_btns, text="התנתק",
                                         command=self._disconnect, state="disabled", width=11)
        self.btn_disconnect.pack(side="left", padx=3)
        self.btn_calib = ttk.Button(frm_btns, text="כיול Swan",
                                    command=self._open_swan_calib, state="disabled", width=11)
        self.btn_calib.pack(side="left", padx=3)
        ttk.Button(frm_btns, text="נקה לוג", command=self._clear_log, width=9).pack(side="left", padx=3)

        # ── שמאל: תוצאות (מתחת לכפתורים) ──
        frm_res = ttk.LabelFrame(frm_left, text="תוצאות")
        frm_res.grid(row=6, column=0, sticky="ew", **pad)
        frm_res.columnconfigure(1, weight=1)

        # שורה 0-1: משקל גדול + קריאות/קצב
        ttk.Label(frm_res, text="משקל:", font=("Arial", 11)).grid(row=0, column=0, sticky="e", **pad)
        self.weight_var = tk.StringVar(value="---")
        ttk.Label(frm_res, textvariable=self.weight_var,
                  font=("Arial", 36, "bold"), foreground="#1a6fb5",
                  anchor="w").grid(row=0, column=1, sticky="w", **pad)

        frm_counts = ttk.Frame(frm_res)
        frm_counts.grid(row=0, column=2, rowspan=2, sticky="ns", padx=8)
        ttk.Label(frm_counts, text="קריאות:", font=("Arial", 9)).grid(row=0, column=0, sticky="e")
        self.readings_var = tk.StringVar(value="0")
        ttk.Label(frm_counts, textvariable=self.readings_var,
                  font=("Arial", 13, "bold")).grid(row=0, column=1, sticky="w", padx=4)
        ttk.Label(frm_counts, text="קריאות/שנייה:", font=("Arial", 9)).grid(row=1, column=0, sticky="e")
        self.rate_var = tk.StringVar(value="0")
        ttk.Label(frm_counts, textvariable=self.rate_var, font=("Arial", 13, "bold"),
                  foreground="#2a9d2a").grid(row=1, column=1, sticky="w", padx=4)

        # מפריד
        ttk.Separator(frm_res, orient="horizontal").grid(
            row=2, column=0, columnspan=3, sticky="ew", pady=4)

        # שורות פירוט — מתעדכנות בכל סיכום
        self.event_time_var   = tk.StringVar(value="")
        self.event_detail_var = tk.StringVar(value="")
        self.event_range_var  = tk.StringVar(value="")
        self.decided_by_var   = tk.StringVar(value="")

        ttk.Label(frm_res, textvariable=self.event_time_var,
                  font=("Consolas", 10), foreground="#005ab5").grid(
            row=3, column=0, columnspan=3, sticky="w", padx=8, pady=1)
        ttk.Label(frm_res, textvariable=self.event_detail_var,
                  font=("Consolas", 11, "bold"), foreground="#1a1a1a").grid(
            row=4, column=0, columnspan=3, sticky="w", padx=8, pady=1)
        ttk.Label(frm_res, textvariable=self.event_range_var,
                  font=("Consolas", 10), foreground="#444444").grid(
            row=5, column=0, columnspan=3, sticky="w", padx=8, pady=(1, 6))

        # ── ימין-שמאל: לוג קריאות ──
        frm_log = ttk.LabelFrame(frm_right, text="לוג קריאות")
        frm_log.grid(row=0, column=0, sticky="nsew", **pad)
        self.log_box = scrolledtext.ScrolledText(frm_log, height=18, state="disabled",
                                                  font=("Consolas", 9), width=46)
        self.log_box.pack(fill="both", expand=True, padx=4, pady=4)
        self.log_box.tag_config("reading", foreground="#1a1a1a")
        self.log_box.tag_config("error",   foreground="#cc0000")
        self.log_box.tag_config("warn",    foreground="#b56b00")
        self.log_box.tag_config("summary", foreground="#005ab5", font=("Consolas", 9, "bold"))
        self.log_box.tag_config("sep",     foreground="#888888")

        # ── ימין-ימין: היסטוריית שקילות (כל הגובה) ──
        frm_hist = ttk.LabelFrame(frm_right, text="היסטוריית שקילות")
        frm_hist.grid(row=0, column=1, sticky="nsew", padx=(0, 4), pady=4)
        frm_hist.rowconfigure(0, weight=1)
        frm_hist.columnconfigure(0, weight=1)

        cols = ("מס'", "משך", "משקל (kg)", "קריאות")
        self.history_tree = ttk.Treeview(frm_hist, columns=cols, show="headings")
        self.history_tree.heading("מס'",       text="מס'")
        self.history_tree.heading("משך",        text="משך")
        self.history_tree.heading("משקל (kg)", text="משקל (kg)")
        self.history_tree.heading("קריאות",    text="קריאות")
        self.history_tree.column("מס'",        width=36,  anchor="center", stretch=False)
        self.history_tree.column("משך",         width=65,  anchor="center", stretch=False)
        self.history_tree.column("משקל (kg)", width=90,  anchor="e",      stretch=False)
        self.history_tree.column("קריאות",     width=55,  anchor="center", stretch=True)

        sb_hist = ttk.Scrollbar(frm_hist, orient="vertical",
                                 command=self.history_tree.yview)
        self.history_tree.configure(yscrollcommand=sb_hist.set)
        self.history_tree.grid(row=0, column=0, sticky="nsew", padx=(4, 0), pady=4)
        sb_hist.grid(row=0, column=1, sticky="ns", pady=4)

        self.history_tree.tag_configure("latest", background="#d6f5d6",
                                         font=("Arial", 10, "bold"))
        self.history_tree.tag_configure("normal", font=("Arial", 10))

        self._refresh_ports()
        self._on_mode_change()
        self._on_auto_change()

    def _on_mode_change(self):
        if self.work_mode.get() == "listen":
            self.mode_hint.config(
                text="Swan האזנה: ראש השקילה שולח רציף — לא נשלח כלום.")
        else:
            self.mode_hint.config(
                text="Swan שליחה: W (בייט בודד) — בקשה/תשובה.")

    def _on_auto_change(self):
        if self.auto_var.get():
            if hasattr(self, "live_var"):
                self.live_var.set(False)
            self.frm_manual.grid_remove()
            self.frm_auto.grid()
            self.btn_start.config(text="▶ התחל ניטור")
        else:
            self.frm_auto.grid_remove()
            self.frm_manual.grid()
            self.btn_start.config(text="▶ התחל דגימה")

    def _on_live_change(self):
        if self.live_var.get():
            self.auto_var.set(False)
            self.frm_auto.grid_remove()
            self.frm_manual.grid()
            self.btn_start.config(text="▶ התחל LIVE")
        else:
            self.frm_manual.grid()
            self.btn_start.config(text="▶ התחל דגימה")

    def _load_settings(self):
        try:
            cfg = json.loads(CONFIG_FILE.read_text(encoding="utf-8"))
        except Exception:
            return
        saved_port = cfg.get("port", "")
        if saved_port and saved_port in self.port_combo["values"]:
            self.port_var.set(saved_port)
        self.baud_var.set(cfg.get("baud", "9600"))
        self.work_mode.set(cfg.get("work_mode", "listen"))
        self.duration_var.set(cfg.get("duration", 1.0))
        self.show_each.set(cfg.get("show_each", True))
        self.auto_var.set(cfg.get("auto_weigh", False))
        self.threshold_var.set(cfg.get("auto_threshold", 0.5))
        self.live_var.set(cfg.get("live_mode", False))
        self._on_mode_change()
        self._on_auto_change()
        self._on_live_change()

    def _save_settings(self):
        cfg = {
            "port":           self.port_var.get(),
            "baud":           self.baud_var.get(),
            "work_mode":      self.work_mode.get(),
            "duration":       self.duration_var.get(),
            "show_each":      self.show_each.get(),
            "auto_weigh":     self.auto_var.get(),
            "auto_threshold": self.threshold_var.get(),
            "live_mode":      self.live_var.get(),
        }
        try:
            CONFIG_FILE.write_text(json.dumps(cfg, indent=2, ensure_ascii=False),
                                   encoding="utf-8")
        except Exception:
            pass

    def _refresh_ports(self):
        if not SERIAL_AVAILABLE:
            self.port_combo.config(values=["(pyserial לא מותקן)"])
            self.port_combo.current(0)
            self._log("pyserial לא מותקן — הרץ: pip install pyserial", "error")
            return
        from serial.tools import list_ports
        ports = [(p.device, p.description) for p in list_ports.comports()]
        # natural sort so COM2 comes before COM10
        ports.sort(key=lambda x: (re.sub(r"\d+", "", x[0]),
                                  int(m.group()) if (m := re.search(r"\d+", x[0])) else 0))
        values = [f"{dev} | {desc}" for dev, desc in ports] if ports else ["(לא נמצאו פורטים)"]
        self.port_combo.config(values=values)
        self.port_combo.current(0)
        self._log(f"נמצאו {len(ports)} פורטים סיריאליים")

    def _log(self, msg: str, tag: str = "reading"):
        ts = time.strftime("%H:%M:%S")
        self.log_box.config(state="normal")
        self.log_box.insert("end", f"{ts}  {msg}\n", tag)
        self.log_box.see("end")
        self.log_box.config(state="disabled")

    def _clear_log(self):
        self.log_box.config(state="normal")
        self.log_box.delete("1.0", "end")
        self.log_box.config(state="disabled")

    def _pump_queue(self):
        """רץ בתהליכון הראשי בלבד: מרוקן את תור ההודעות מתהליכון הדגימה."""
        try:
            while True:
                kind, payload = self.ui_queue.get_nowait()
                if kind == "log":
                    self._log(*payload)
                elif kind == "done":
                    self._summarize(*payload)
                elif kind == "live":
                    self.weight_var.set(f"{payload:.3f}")
                elif kind == "auto_event":
                    self._summarize_event(*payload)
                elif kind == "auto_stopped":
                    self._on_auto_stopped()
                elif kind == "live_tick":
                    self._on_live_tick(*payload)
                elif kind == "live_stopped":
                    self._on_live_stopped()
                elif kind == "calib":
                    if hasattr(self, "_calib_update"):
                        self._calib_update(*payload)
                elif kind == "calib_disconnect":
                    self._disconnect()
        except queue.Empty:
            pass
        self.root.after(40, self._pump_queue)

    # ──────────────────────────────────────────────
    # Connection
    # ──────────────────────────────────────────────

    def _connect(self):
        try:
            if not SERIAL_AVAILABLE:
                self._log("שגיאה: pyserial לא מותקן — הרץ: pip install pyserial", "error")
                return
            import serial as _serial
            port_name = self.port_var.get().split("|")[0].strip()
            if port_name.startswith("("):
                self._log("לא נבחר פורט תקין — לחץ ⟳ לרענון", "error")
                return
            self.conn = _serial.Serial(
                port=port_name,
                baudrate=int(self.baud_var.get()),
                bytesize=8, parity="N", stopbits=1,
                timeout=0.05,
            )
            self._log(f"מחובר סיריאלי: {port_name} @ {self.baud_var.get()} baud", "summary")

            self.btn_connect.config(state="disabled")
            self.btn_start.config(state="normal")
            self.btn_disconnect.config(state="normal")
            self.btn_calib.config(state="normal")
            self._save_settings()

        except Exception as e:
            self._log(f"שגיאת חיבור: {e}", "error")

    def _disconnect(self):
        self.running = False
        if self.conn:
            try:
                self.conn.close()
            except Exception:
                pass
            self.conn = None
        self.btn_connect.config(state="normal")
        self.btn_start.config(state="disabled")
        self.btn_disconnect.config(state="disabled")
        self.btn_calib.config(state="disabled")
        self._log("מנותק")

    # ──────────────────────────────────────────────
    # Low-level I/O
    # ──────────────────────────────────────────────

    def _flush_input(self):
        """מרוקן נתונים ישנים שהצטברו בבאפר לפני תחילת דגימה."""
        try:
            self.conn.reset_input_buffer()
        except Exception:
            pass

    def _read_available(self) -> bytes:
        """קורא כל מה שזמין כרגע, בלי לחסום יותר מ-50ms."""
        try:
            n = self.conn.in_waiting
            return self.conn.read(n if n else 1)
        except Exception as e:
            raise IOError(str(e))

    def _write(self, data: bytes):
        self.conn.write(data)

    # ──────────────────────────────────────────────
    # Sampling
    # ──────────────────────────────────────────────

    def _start_sampling(self):
        if self.running:
            return
        # משתני Tk נקראים כאן בלבד — אסור לגשת אליהם מתהליכון הדגימה
        duration   = self.duration_var.get()
        listen     = self.work_mode.get() == "listen"
        show_each  = self.show_each.get()
        auto_weigh = self.auto_var.get()
        live_mode  = self.live_var.get()
        threshold  = self.threshold_var.get()

        self.running = True
        self.weight_var.set("...")
        self.decided_by_var.set("")
        self.readings_var.set("0")
        self.rate_var.set("0")
        mode_name  = "האזנה" if listen else "שליחת W"
        self._log("─" * 56, "sep")

        if auto_weigh:
            self.btn_start.config(text="⏹ עצור ניטור", command=self._stop_auto)
            self._log(f"ניטור אוטו [{mode_name}] — סף {threshold:.2f} kg", "summary")
            threading.Thread(target=self._auto_thread, daemon=True,
                             args=(listen, show_each, threshold)).start()
        elif live_mode:
            self.btn_start.config(text="⏹ עצור LIVE", command=self._stop_live)
            self._log(f"LIVE [{mode_name}] — כל {duration:.1f} שניות", "summary")
            threading.Thread(target=self._live_thread, daemon=True,
                             args=(duration, listen, show_each)).start()
        else:
            self.btn_start.config(state="disabled")
            self._log(f"מתחיל דגימה [{mode_name}] — {duration:.1f} שניות", "summary")
            threading.Thread(target=self._sample_thread, daemon=True,
                             args=(duration, listen, show_each)).start()

    def _stop_auto(self):
        self.running = False

    def _stop_live(self):
        self.running = False

    def _sample_thread(self, duration: float, listen: bool, show_each: bool):
        poll_cmd = None if listen else b"W"
        readings: list[tuple[float, float, str]] = []   # (t, weight, status)
        bad = 0
        specials: dict[str, int] = {}
        buf = b""
        idx = 0

        def emit(msg, tag="reading"):
            self.ui_queue.put(("log", (msg, tag)))

        try:
            self._flush_input()
        except Exception:
            pass

        start = time.perf_counter()

        try:
            while self.running and (time.perf_counter() - start) < duration:
                if poll_cmd:
                    self._write(poll_cmd)
                chunk = self._read_available()
                if not chunk:
                    continue
                buf += chunk

                buf = buf.replace(b"\n", b"\r")
                while b"\r" in buf:
                    raw, buf = buf.split(b"\r", 1)
                    t = time.perf_counter() - start
                    text = raw.decode("ascii", errors="replace")
                    w, status, special = parse_swan_frame(text)

                    if special:
                        specials[special] = specials.get(special, 0) + 1
                        if show_each:
                            emit(f"  {t:6.3f}s   [{special}]", "warn")
                    elif w is not None:
                        idx += 1
                        readings.append((t, w, status))
                        if show_each:
                            st = f"  {status}" if status else ""
                            emit(f"  #{idx:>4}   {t:6.3f}s   {w:+10.3f}{st}")
                    elif raw:
                        bad += 1
                        if show_each:
                            emit(f"  {t:6.3f}s   פסולת: {raw!r}", "error")

        except IOError as e:
            emit(f"שגיאת תקשורת: {e}", "error")
        except Exception as e:
            emit(f"שגיאה: {e}", "error")

        elapsed = time.perf_counter() - start
        self.running = False
        self.ui_queue.put(("done", (readings, bad, specials, elapsed, listen)))

    # ──────────────────────────────────────────────
    # Summary
    # ──────────────────────────────────────────────

    @staticmethod
    def _decide_weight(readings):
        """
        מחזיר (משקל, תיאור_בסיס).
        אלגוריתם: חלון הזזה N=6 — מחפשים את הרצף השטוח ביותר (מינימום stdev).
        זה מבטל אוטומטית את קצוות העלייה/ירידה של מסוע.
        fallback לחציון כאשר יש פחות מ-N קריאות.
        """
        N = 6
        weights = [w for _, w, _ in readings]
        count = len(weights)

        if count >= N:
            best_sd = float("inf")
            best_i = 0
            for i in range(count - N + 1):
                sd = statistics.stdev(weights[i:i + N])
                if sd < best_sd:
                    best_sd = sd
                    best_i = i
            window = weights[best_i:best_i + N]
            decided = statistics.median(window)
            basis = f"חלון שטוח #{best_i+1}–{best_i+N}  stdev={best_sd:.4f}"
            return decided, basis

        decided = statistics.median(weights)
        basis = f"חציון {count} קריאות"
        return decided, basis

    def _add_history_row(self, elapsed: float, decided: float, count: int = 0):
        """מוסיף שורה לטבלת ההיסטוריה ומסמן אותה כ'אחרונה' בירוק."""
        self._event_count += 1
        for iid in self.history_tree.get_children():
            self.history_tree.item(iid, tags=("normal",))
        self.history_tree.insert("", 0, tags=("latest",),
                                  values=(self._event_count, f"{elapsed:.2f}s",
                                          f"{decided:+.3f}",
                                          count if count else "—"))

    def _summarize(self, readings, bad, specials, elapsed, listen):
        self.btn_start.config(state="normal")
        self._log("─" * 56, "sep")

        if not readings:
            self.weight_var.set("—")
            self.decided_by_var.set("")
            self.readings_var.set("0")
            self.rate_var.set("0")
            self._log(f"לא התקבלו קריאות תקינות ({elapsed:.2f}s, פסולת: {bad})", "error")
            if not listen:
                self._log("במצב שליחת W: ודא שהמאזניים מחוברים ושה-baud תואם.", "warn")
            else:
                self._log("במצב האזנה: ודא שראש השקילה מוגדר לשידור רציף "
                          "ושה-baud תואם.", "warn")
            return

        weights = [w for _, w, _ in readings]
        count = len(weights)
        rate = count / elapsed if elapsed > 0 else 0

        decided, basis = self._decide_weight(readings)
        spread = max(weights) - min(weights)
        sd_str = f"  סטד: {statistics.stdev(weights):.4f}" if count > 1 else ""

        self.weight_var.set(f"{decided:.3f}")
        self.decided_by_var.set(basis)
        self.readings_var.set(str(count))
        self.rate_var.set(f"{rate:.1f}")
        self.event_time_var.set(f"משך: {elapsed:.2f}s   {count} קריאות @ {rate:.1f}/s")
        self.event_detail_var.set(f"משקל: {decided:+.3f}   ({basis})")
        self.event_range_var.set(
            f"מין/מקס: {min(weights):+.3f} / {max(weights):+.3f}   "
            f"פיזור: {spread:.3f}{sd_str}")
        self._add_history_row(elapsed, decided, count)

        self._log(
            f"סיכום: {count} קריאות ב-{elapsed:.2f}s = {rate:.1f}/שנייה", "summary")
        self._log(
            f"        משקל שנקבע: {decided:+.3f}   ({basis})", "summary")
        self._log(
            f"        ממוצע: {statistics.fmean(weights):+.3f}   "
            f"מין/מקס: {min(weights):+.3f} / {max(weights):+.3f}   פיזור: {spread:.3f}{sd_str}")
        if specials:
            self._log("        מצבי קצה: " +
                      ", ".join(f"{k}×{v}" for k, v in specials.items()), "warn")
        if bad:
            self._log(f"        מסגרות פסולות: {bad}", "warn")

        # השוואה לתקרת הקושחה
        if listen and rate > 0:
            pct = rate / self.FIRMWARE_STREAM_HZ * 100
            if pct < 80:
                self._log(f"        שים לב: {rate:.1f}/שנייה = {pct:.0f}% מתקרת "
                          f"הקושחה (~{self.FIRMWARE_STREAM_HZ}Hz) — ייתכן אובדן מסגרות.", "warn")

    # ──────────────────────────────────────────────
    # Auto-weigh thread
    # ──────────────────────────────────────────────

    def _auto_thread(self, listen: bool, show_each: bool, threshold: float):
        poll_cmd = None if listen else b"W"
        buf = b""

        DEBOUNCE_S = 0.4          # משקל חייב להיות מתחת לסף למשך זמן זה לפני סיום שקילה
        AUTO_EVENT_NUM = [0]      # מונה אירועים (list כדי לאפשר שינוי מתוך closure)

        state = "IDLE"            # "IDLE" | "WEIGHING"
        session_readings: list = []
        session_bad = 0
        session_specials: dict = {}
        session_start = 0.0
        session_wall_start: datetime.datetime = None
        session_idx = 0
        below_since: float = None

        def emit(msg, tag="reading"):
            self.ui_queue.put(("log", (msg, tag)))

        try:
            self._flush_input()
        except Exception:
            pass

        try:
            while self.running:
                if poll_cmd:
                    self._write(poll_cmd)
                chunk = self._read_available()
                if not chunk:
                    continue
                buf += chunk

                buf = buf.replace(b"\n", b"\r")
                while b"\r" in buf:
                    raw, buf = buf.split(b"\r", 1)
                    now = time.perf_counter()
                    text = raw.decode("ascii", errors="replace")
                    w, status, special = parse_swan_frame(text)

                    if special:
                        if state == "WEIGHING":
                            session_specials[special] = session_specials.get(special, 0) + 1
                        if show_each:
                            emit(f"  [{special}]", "warn")
                        continue

                    if w is None:
                        if raw:
                            if state == "WEIGHING":
                                session_bad += 1
                            if show_each:
                                emit(f"  פסולת: {raw!r}", "error")
                        continue

                    # קריאה תקינה — עדכן תצוגה חיה
                    self.ui_queue.put(("live", w))

                    if state == "IDLE":
                        if w > threshold:
                            # IDLE → WEIGHING
                            state = "WEIGHING"
                            session_start = now
                            session_wall_start = datetime.datetime.now()
                            session_readings = [(0.0, w, status)]
                            session_bad = 0
                            session_specials = {}
                            session_idx = 1
                            below_since = None
                            AUTO_EVENT_NUM[0] += 1
                            emit(f"── אירוע #{AUTO_EVENT_NUM[0]}: {w:+.3f} > סף {threshold:.2f} — מתחיל ──", "summary")
                            if show_each:
                                st = f"  {status}" if status else ""
                                emit(f"  #   1   {0.000:6.3f}s   {w:+10.3f}{st}")

                    elif state == "WEIGHING":
                        t_sess = now - session_start
                        session_idx += 1
                        session_readings.append((t_sess, w, status))
                        if show_each:
                            st = f"  {status}" if status else ""
                            emit(f"  #{session_idx:>4}   {t_sess:6.3f}s   {w:+10.3f}{st}")

                        if w <= threshold:
                            if below_since is None:
                                below_since = now
                            elif (now - below_since) >= DEBOUNCE_S:
                                # WEIGHING → IDLE — סיום אירוע
                                elapsed = now - session_start
                                wall_end = datetime.datetime.now()
                                state = "IDLE"
                                emit(f"── אירוע #{AUTO_EVENT_NUM[0]} הסתיים ({elapsed:.2f}s) ──", "summary")
                                self.ui_queue.put(("auto_event",
                                                  (session_readings, session_bad,
                                                   session_specials, elapsed, listen,
                                                   session_wall_start, wall_end)))
                                session_readings = []
                                below_since = None
                        else:
                            below_since = None  # עלה שוב מעל הסף — אפס debounce

        except IOError as e:
            emit(f"שגיאת תקשורת: {e}", "error")
        except Exception as e:
            emit(f"שגיאה: {e}", "error")

        # אם ניטור הופסק באמצע שקילה — הצג תוצאות חלקיות
        if state == "WEIGHING" and session_readings:
            elapsed = time.perf_counter() - session_start
            wall_end = datetime.datetime.now()
            emit(f"── ניטור הופסק באמצע שקילה — תוצאות חלקיות ({elapsed:.2f}s) ──", "warn")
            self.ui_queue.put(("auto_event",
                              (session_readings, session_bad, session_specials, elapsed, listen,
                               session_wall_start, wall_end)))

        self.running = False
        self.ui_queue.put(("auto_stopped", None))

    def _summarize_event(self, readings, bad, specials, elapsed, listen,
                         wall_start=None, wall_end=None):
        """מציג סיכום אירוע שקילה בודד במצב אוטו — לא מחזיר כפתור."""
        self._log("─" * 56, "sep")
        if not readings:
            self._log(f"אירוע ללא קריאות תקינות ({elapsed:.2f}s)", "error")
            return

        weights = [w for _, w, _ in readings]
        count = len(weights)
        rate = count / elapsed if elapsed > 0 else 0

        decided, basis = self._decide_weight(readings)
        spread = max(weights) - min(weights)
        sd_str = f"  סטד: {statistics.stdev(weights):.4f}" if count > 1 else ""

        self.weight_var.set(f"{decided:.3f}")
        self.decided_by_var.set(basis)
        self.readings_var.set(str(count))
        self.rate_var.set(f"{rate:.1f}")
        self.event_detail_var.set(f"משקל: {decided:+.3f}   ({basis})")
        self.event_range_var.set(
            f"מין/מקס: {min(weights):+.3f} / {max(weights):+.3f}   "
            f"פיזור: {spread:.3f}{sd_str}")

        # שורת זמן: שעת התחלה → שעת סיום  (משך)
        fmt = "%H:%M:%S"
        end_time_str = wall_end.strftime(fmt) if wall_end else datetime.datetime.now().strftime(fmt)
        time_str = (f"{wall_start.strftime(fmt)} → {end_time_str}   "
                    if wall_start else "")
        self.event_time_var.set(f"{time_str}משך: {elapsed:.2f}s   {count} קריאות @ {rate:.1f}/s")
        self._add_history_row(elapsed, decided, count)

        self._log(f"  {time_str}משך: {elapsed:.2f}s   {count} קריאות @ {rate:.1f}/s", "summary")
        self._log(f"  משקל: {decided:+.3f}   ({basis})", "summary")
        self._log(f"  מין/מקס: {min(weights):+.3f}/{max(weights):+.3f}   "
                  f"פיזור: {spread:.3f}{sd_str}")
        if specials:
            self._log("  מצבי קצה: " + ", ".join(f"{k}×{v}" for k, v in specials.items()), "warn")
        if bad:
            self._log(f"  מסגרות פסולות: {bad}", "warn")

    def _on_auto_stopped(self):
        self.btn_start.config(command=self._start_sampling)
        self._on_auto_change()
        self._log("ניטור הסתיים", "summary")

    # ──────────────────────────────────────────────
    # Live thread
    # ──────────────────────────────────────────────

    def _live_thread(self, duration: float, listen: bool, show_each: bool):
        poll_cmd = None if listen else b"W"

        buf = b""
        tick_num = 0

        def emit(msg, tag="reading"):
            self.ui_queue.put(("log", (msg, tag)))

        try:
            self._flush_input()
        except Exception:
            pass

        try:
            while self.running:
                window_readings: list[tuple[float, float, str]] = []
                window_start = time.perf_counter()
                window_wall  = datetime.datetime.now()

                # ── אסוף קריאות למשך duration שניות ──
                while self.running and (time.perf_counter() - window_start) < duration:
                    if poll_cmd:
                        self._write(poll_cmd)
                    chunk = self._read_available()
                    if not chunk:
                        continue
                    buf += chunk

                    buf = buf.replace(b"\n", b"\r")
                    while b"\r" in buf:
                        raw, buf = buf.split(b"\r", 1)
                        t_rel = time.perf_counter() - window_start
                        text = raw.decode("ascii", errors="replace")
                        w, status, special = parse_swan_frame(text)

                        if w is not None:
                            window_readings.append((t_rel, w, status))
                            self.ui_queue.put(("live", w))
                            if show_each:
                                st = f"  {status}" if status else ""
                                emit(f"  #{len(window_readings):>4}   {t_rel:6.3f}s   {w:+10.3f}{st}")

                # ── סיכום החלון ──
                wall_end = datetime.datetime.now()
                elapsed  = time.perf_counter() - window_start
                if window_readings:
                    tick_num += 1
                    self.ui_queue.put(("live_tick",
                                      (tick_num, window_wall, wall_end,
                                       window_readings, elapsed)))

        except IOError as e:
            emit(f"שגיאת תקשורת: {e}", "error")
        except Exception as e:
            emit(f"שגיאה: {e}", "error")

        self.running = False
        self.ui_queue.put(("live_stopped", None))

    def _on_live_tick(self, tick_num, wall_start, wall_end, readings, elapsed):
        weights = [w for _, w, _ in readings]
        count   = len(weights)
        rate    = count / elapsed if elapsed > 0 else 0
        decided, basis = self._decide_weight(readings)

        fmt      = "%H:%M:%S"
        time_str = wall_end.strftime(fmt)

        self.weight_var.set(f"{decided:.3f}")
        self.readings_var.set(str(count))
        self.rate_var.set(f"{rate:.1f}")
        self.event_time_var.set(
            f"{wall_start.strftime(fmt)} → {time_str}   "
            f"{count} קריאות @ {rate:.1f}/s")
        self.event_detail_var.set(f"משקל: {decided:+.3f}   ({basis})")
        self.event_range_var.set(
            f"מין/מקס: {min(weights):+.3f} / {max(weights):+.3f}   "
            f"פיזור: {max(weights)-min(weights):.3f}")
        self._add_history_row(elapsed, decided, count)
        self._log(
            f"  #{tick_num:>3}  {time_str}   {count:>4} קריאות   {decided:+.3f} kg", "summary")

    def _on_live_stopped(self):
        self.btn_start.config(command=self._start_sampling)
        self._on_live_change()
        self._log("LIVE הסתיים", "summary")


    # ──────────────────────────────────────────────
    # Swan calibration wizard
    # ──────────────────────────────────────────────

    def _open_swan_calib(self):
        if self.running:
            self._log("עצור את הדגימה לפני הכיול", "warn")
            return

        dlg = tk.Toplevel(self.root)
        dlg.title("כיול Swan — PC0035")
        dlg.resizable(False, False)
        dlg.grab_set()
        dlg.focus_set()
        dlg.minsize(440, 420)

        self._calib_event  = threading.Event()
        self._calib_active = True
        self.btn_start.config(state="disabled")

        # ── משקל כיול ──
        frm_top = ttk.LabelFrame(dlg, text="הגדרות כיול")
        frm_top.pack(fill="x", padx=12, pady=(12, 6))
        ttk.Label(frm_top, text="משקל כיול:").grid(row=0, column=0, padx=8, pady=6, sticky="e")
        calib_weight_var = tk.IntVar(value=3000)
        spn_w = ttk.Spinbox(frm_top, textvariable=calib_weight_var,
                             from_=100, to=99999, increment=100, width=9, format="%d")
        spn_w.grid(row=0, column=1, padx=4, pady=6, sticky="w")
        ttk.Label(frm_top, text="גרמים").grid(row=0, column=2, sticky="w")

        # ── סטטוס ──
        frm_st = ttk.LabelFrame(dlg, text="מצב")
        frm_st.pack(fill="x", padx=12, pady=6)
        step_var = tk.StringVar(value="הכנס משקל כיול ולחץ 'התחל כיול'")
        ttk.Label(frm_st, textvariable=step_var,
                  font=("Arial", 11, "bold"), foreground="#1a3a6b",
                  wraplength=390, justify="right", anchor="w").pack(
            padx=10, pady=(10, 4), fill="x")
        prog_var = tk.StringVar(value="")
        ttk.Label(frm_st, textvariable=prog_var,
                  font=("Consolas", 9), foreground="#555").pack(padx=10, pady=(0, 4))
        pb = ttk.Progressbar(frm_st, orient="horizontal", length=400, maximum=14)
        pb.pack(padx=10, pady=(0, 10))

        # ── לוג תגובות המאזניים ──
        frm_resp = ttk.LabelFrame(dlg, text="תגובת המאזניים")
        frm_resp.pack(fill="x", padx=12, pady=6)
        resp_box = scrolledtext.ScrolledText(frm_resp, height=4, state="disabled",
                                              font=("Consolas", 8), bg="#f4f4f4")
        resp_box.pack(padx=6, pady=6, fill="x")

        def resp_log(txt):
            resp_box.config(state="normal")
            resp_box.insert("end", txt + "\n")
            resp_box.see("end")
            resp_box.config(state="disabled")

        # ── כפתורים ──
        frm_b = ttk.Frame(dlg)
        frm_b.pack(padx=12, pady=(4, 12))
        btn_act = ttk.Button(frm_b, text="התחל כיול", width=14)
        btn_act.pack(side="left", padx=6)
        btn_cls = ttk.Button(frm_b, text="סגור", width=10)
        btn_cls.pack(side="left", padx=6)

        # ── helpers ──
        def _close():
            self._calib_active = False
            self._calib_event.set()
            if hasattr(self, "_calib_update"):
                del self._calib_update
            try:
                self.btn_start.config(state="normal" if self.conn else "disabled")
            except Exception:
                pass
            dlg.destroy()

        def _on_continue():
            self._calib_event.set()
            btn_act.config(state="disabled", text="ממתין...")
            btn_cls.config(state="disabled")

        def _on_start():
            w = calib_weight_var.get()
            btn_act.config(state="disabled")
            spn_w.config(state="disabled")
            btn_cls.config(state="disabled")
            threading.Thread(target=self._calib_thread, daemon=True, args=(w,)).start()

        btn_act.config(command=_on_start)
        btn_cls.config(command=_close)
        dlg.protocol("WM_DELETE_WINDOW", lambda: None)

        # ── update dispatcher ──
        def calib_update(kind, data):
            if kind == "status":
                step_var.set(str(data))
            elif kind == "progress":
                n = int(data)
                pb["value"] = min(n, 14)
                prog_var.set("*" * min(n, 14) + f"  ({n}/14)")
            elif kind == "response":
                resp_log(str(data))
            elif kind == "step":
                pb["value"] = 0
                prog_var.set("")
                name = data[0] if data else ""
                if name == "clear_platform":
                    step_var.set("הסר הכל מהמשטח ולחץ 'המשך'")
                    btn_act.config(state="normal", text="המשך ▶", command=_on_continue)
                    btn_cls.config(state="disabled")
                elif name == "put_weight":
                    wg = data[1] if len(data) > 1 else "?"
                    step_var.set(f"הנח {wg}g על המשטח ולחץ 'המשך'")
                    btn_act.config(state="normal", text="המשך ▶", command=_on_continue)
                    btn_cls.config(state="disabled")
                elif name == "confirm_save":
                    step_var.set("כיול הושלם — לחץ 'המשך' לשמירה ואיתחול")
                    btn_act.config(state="normal", text="המשך ▶", command=_on_continue)
                    btn_cls.config(state="disabled")
                elif name == "done":
                    step_var.set("שמירה בוצעה — ממתין לאיתחול המאזניים...")
                    btn_act.config(state="disabled")
                    btn_cls.config(state="disabled")
                    pb["value"] = 14
            elif kind == "complete":
                step_var.set("כיול הושלם בהצלחה ✓")
                prog_var.set("ניתן לסגור ולהתחבר מחדש")
                pb["value"] = 14
                btn_act.config(state="disabled")
                btn_cls.config(state="normal")
                dlg.protocol("WM_DELETE_WINDOW", _close)
            elif kind == "error":
                step_var.set(f"שגיאה: {data}")
                prog_var.set("")
                btn_act.config(state="disabled")
                btn_cls.config(state="normal")
                dlg.protocol("WM_DELETE_WINDOW", _close)

        self._calib_update = calib_update

    def _calib_thread(self, weight_g: int):
        def emit(kind, data=None):
            self.ui_queue.put(("calib", (kind, data)))

        def read_until(patterns, timeout_idle=1.0, timeout_total=30.0):
            buf = b""
            t0 = time.perf_counter()
            t_last = t0
            while time.perf_counter() - t0 < timeout_total:
                if not self._calib_active:
                    break
                try:
                    chunk = self._read_available()
                except Exception:
                    break
                if chunk:
                    buf += chunk
                    t_last = time.perf_counter()
                    new_ast = chunk.count(b"*")
                    if new_ast:
                        emit("progress", buf.count(b"*"))
                    decoded = buf.decode("ascii", errors="replace")
                    if any(p in decoded for p in patterns):
                        time.sleep(0.15)
                        try:
                            extra = self._read_available()
                            if extra:
                                buf += extra
                        except Exception:
                            pass
                        break
                else:
                    if buf and time.perf_counter() - t_last > timeout_idle:
                        break
            return buf.decode("ascii", errors="replace")

        try:
            # ── שלב 0: ESC P weight ESC e ──
            self._flush_input()
            self._write(b"\x1B\x50" + str(weight_g).encode("ascii") + b"\x1B\x65")
            emit("status", f"נשלח: ESC P {weight_g} ESC e — ממתין...")

            text = read_until(["Clear", "clear", "platform"], timeout_idle=0.8, timeout_total=5.0)
            if not self._calib_active:
                return
            emit("response", text.strip())
            if not any(w in text.lower() for w in ["clear", "platform"]):
                emit("error", f"תגובה לא צפויה:\n{text!r}")
                return
            emit("step", ("clear_platform",))

            if not self._calib_event.wait(timeout=120):
                emit("error", "זמן ההמתנה פג")
                return
            self._calib_event.clear()
            if not self._calib_active:
                return

            # ── שלב 1: ESC N ESC e — כיול אפס ──
            self._write(b"\x1B\x4E\x1B\x65")
            emit("status", "מבצע כיול אפס...")

            text = read_until(["Put", "put"], timeout_idle=1.2, timeout_total=25.0)
            if not self._calib_active:
                return
            emit("response", text.strip()[:300])
            if not any(w in text for w in ["Put", "put"]):
                emit("error", f"לא התקבלה הנחיה 'Put':\n{text!r}")
                return
            emit("step", ("put_weight", weight_g, text.count("*")))

            if not self._calib_event.wait(timeout=300):
                emit("error", "זמן ההמתנה פג")
                return
            self._calib_event.clear()
            if not self._calib_active:
                return

            # ── שלב 2: ESC N ESC e — כיול משקל ──
            self._write(b"\x1B\x4E\x1B\x65")
            emit("status", "מבצע כיול משקל...")

            text = read_until(["save", "Save"], timeout_idle=1.2, timeout_total=25.0)
            if not self._calib_active:
                return
            emit("response", text.strip()[:300])
            if "save" not in text.lower():
                emit("error", f"לא התקבלה הנחיה לשמירה:\n{text!r}")
                return
            emit("step", ("confirm_save",))

            if not self._calib_event.wait(timeout=60):
                emit("error", "זמן ההמתנה פג")
                return
            self._calib_event.clear()
            if not self._calib_active:
                return

            # ── שלב 3: ESC N ESC e — שמירה ואיתחול ──
            self._write(b"\x1B\x4E\x1B\x65")
            emit("status", "שומר ומאתחל...")

            text = read_until(["DONE", "REBOOT"], timeout_idle=2.0, timeout_total=12.0)
            emit("response", text.strip()[:200])
            emit("step", ("done",))

            for i in range(7):
                if not self._calib_active:
                    return
                time.sleep(1)
                emit("status", f"ממתין לאיתחול... {i + 1}/7")

            emit("complete", None)
            self.ui_queue.put(("calib_disconnect", None))

        except IOError as e:
            emit("error", f"שגיאת תקשורת: {e}")
        except Exception as e:
            emit("error", f"שגיאה: {e}")


# ──────────────────────────────────────────────
# Entry point
# ──────────────────────────────────────────────

if __name__ == "__main__":
    root = tk.Tk()
    root.state("zoomed")
    app = ScaleSampler(root)
    root.mainloop()
