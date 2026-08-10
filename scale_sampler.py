"""
Scale Sampler - מאזני מירב / DINI / Swan

שלושה פרוטוקולים נתמכים:

  מירב (Merav / M6ConveyorAI):
    מוד 0:  <+/-><7 תווי משקל><CR>                       = 9 בייט
    מוד 3:  <+/-><7 תווי משקל><S|U><T|רווח><Z|רווח><CR>  = 12 בייט
    מצבי קצה: "  STOP  " / " UNDER  "

  DINI:
    <ST|US>,<GS|NT>,<weight>,<unit><CR><LF>              = ~19 בייט
    ST=יציב  US=לא יציב  GS=ברוטו  NT=נטו
    OL=מעל קיבולת

  Swan (PC0034):
    TX: ESC W ESC e  (1B 57 1B 65)  = 4 בייט
    RX: CR [S1] XX.XXX [G/N] RS [S2] XX.XXX RS [CS] LF = 20 בייט
    S1: W=יציב-חדש  R=יציב-חוזר  U=לא-יציב  Z=אפס  -=underload  H=overload
    S2: S=יציב  M=תנועה  -=underload
    CS: סכום בייטים 1–17 mod 256  (9600 baud, 8N1)
"""

import tkinter as tk
from tkinter import ttk, scrolledtext
import threading
import queue
import time
import re
import socket
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

def parse_frame(frame: str):
    """
    פרוטוקול מירב: <+/-><7 תווי משקל>[סטטוס]<CR>
    מחזיר (weight|None, status_str, special|None)
    """
    s = frame.strip("\r\n\x00")
    if not s.strip():
        return None, "", None

    up = s.upper()
    if "STOP" in up:
        return None, "", "STOP"
    if "UNDER" in up:
        return None, "", "UNDER"

    m = re.search(r"([+\-]?)\s*(\d+\.?\d*)", s)
    if not m:
        return None, "", None

    sign = -1.0 if m.group(1) == "-" else 1.0
    try:
        weight = sign * float(m.group(2))
    except ValueError:
        return None, "", None

    status = s[m.end():].strip()
    return weight, status, None


def parse_dini_frame(frame: str):
    """
    פרוטוקול DINI: ST,GS,   0.000,kg
    שדה 0: ST=יציב / US=לא יציב
    שדה 1: GS=ברוטו / NT=נטו
    שדה 2: משקל (רווחים + מספר)
    שדה 3: יחידה (kg/g/lb...)
    מחזיר (weight|None, status_str, special|None)  — אותה חתימה כמו parse_frame
    """
    s = frame.strip("\r\n\x00 ")
    if not s:
        return None, "", None

    parts = s.split(",")
    if len(parts) < 3:
        return None, "", None

    status_raw  = parts[0].strip().upper()   # ST / US / error codes
    weight_raw  = parts[2].strip()
    unit        = parts[3].strip() if len(parts) > 3 else ""

    # מצבי קצה: OL=overload, UND=underload וכו'
    wu = weight_raw.upper()
    if any(x in wu for x in ("OL", "OF", "----", "OVER")):
        return None, "", "OL"
    if any(x in wu for x in ("UND", "UNDER")):
        return None, "", "UNDER"
    if any(x in wu for x in ("ERR", "----")):
        return None, "", "ERROR"

    try:
        weight = float(weight_raw)
    except ValueError:
        return None, "", None

    # סטטוס: S=יציב (ST) U=לא יציב (US/כל השאר)
    stability = "S" if status_raw == "ST" else "U"
    # הוסף סוג משקל וסוג יחידה לשדה הסטטוס (לתצוגה)
    wtype = parts[1].strip() if len(parts) > 1 else ""
    status_str = f"{stability} {wtype} {unit}".strip()
    return weight, status_str, None


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
        self.root.title("Scale Sampler — מירב")
        self.conn = None
        self.conn_mode = None
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

        # ── שמאל: סוג חיבור ──
        frm_type = ttk.LabelFrame(frm_left, text="סוג חיבור")
        frm_type.grid(row=1, column=0, sticky="ew", **pad)
        self.conn_type = tk.StringVar(value="serial")
        ttk.Radiobutton(frm_type, text="Serial (RS-232/RS-485)",
                        variable=self.conn_type, value="serial",
                        command=self._on_type_change).grid(row=0, column=0, padx=8)
        ttk.Radiobutton(frm_type, text="TCP",
                        variable=self.conn_type, value="tcp",
                        command=self._on_type_change).grid(row=0, column=1, padx=8)

        # ── שמאל: הגדרות Serial ──
        self.frm_serial = ttk.LabelFrame(frm_left, text="הגדרות Serial")
        self.frm_serial.grid(row=2, column=0, sticky="ew", **pad)
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
                     values=["600", "1200", "2400", "4800", "9600", "19200", "38400"],
                     width=8, state="readonly").grid(row=0, column=4, **pad)

        # ── שמאל: הגדרות TCP ──
        self.frm_tcp = ttk.LabelFrame(frm_left, text="הגדרות TCP")
        self.frm_tcp.grid(row=3, column=0, sticky="ew", **pad)
        ttk.Label(self.frm_tcp, text="IP:").grid(row=0, column=0, **pad)
        self.ip_var = tk.StringVar(value="192.168.1.100")
        ttk.Entry(self.frm_tcp, textvariable=self.ip_var, width=16).grid(row=0, column=1, **pad)
        ttk.Label(self.frm_tcp, text="Port:").grid(row=0, column=2, **pad)
        self.tcp_port_var = tk.StringVar(value="10001")
        ttk.Entry(self.frm_tcp, textvariable=self.tcp_port_var, width=7).grid(row=0, column=3, **pad)

        # ── שמאל: הגדרות דגימה ──
        frm_set = ttk.LabelFrame(frm_left, text="הגדרות דגימה")
        frm_set.grid(row=4, column=0, sticky="ew", **pad)

        ttk.Label(frm_set, text="פרוטוקול:").grid(row=0, column=0, sticky="e", **pad)
        self.protocol_var = tk.StringVar(value="merav")
        ttk.Radiobutton(frm_set, text="מירב (Merav)",
                        variable=self.protocol_var, value="merav",
                        command=self._on_protocol_change).grid(row=0, column=1, sticky="w")
        ttk.Radiobutton(frm_set, text="DINI",
                        variable=self.protocol_var, value="dini",
                        command=self._on_protocol_change).grid(row=0, column=2, sticky="w")
        ttk.Radiobutton(frm_set, text="Swan",
                        variable=self.protocol_var, value="swan",
                        command=self._on_protocol_change).grid(row=0, column=3, sticky="w")
        self.show_each = tk.BooleanVar(value=True)
        ttk.Checkbutton(frm_set, text="הצג כל קריאה",
                        variable=self.show_each).grid(row=0, column=3, padx=8)

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

        cols = ("מס'", "שעה", "משקל (kg)")
        self.history_tree = ttk.Treeview(frm_hist, columns=cols, show="headings")
        self.history_tree.heading("מס'",       text="מס'")
        self.history_tree.heading("שעה",        text="שעה")
        self.history_tree.heading("משקל (kg)", text="משקל (kg)")
        self.history_tree.column("מס'",        width=36,  anchor="center", stretch=False)
        self.history_tree.column("שעה",         width=65,  anchor="center", stretch=False)
        self.history_tree.column("משקל (kg)", width=90,  anchor="e",      stretch=True)

        sb_hist = ttk.Scrollbar(frm_hist, orient="vertical",
                                 command=self.history_tree.yview)
        self.history_tree.configure(yscrollcommand=sb_hist.set)
        self.history_tree.grid(row=0, column=0, sticky="nsew", padx=(4, 0), pady=4)
        sb_hist.grid(row=0, column=1, sticky="ns", pady=4)

        self.history_tree.tag_configure("latest", background="#d6f5d6",
                                         font=("Arial", 10, "bold"))
        self.history_tree.tag_configure("normal", font=("Arial", 10))

        self._refresh_ports()
        self._on_type_change()
        self._on_protocol_change()
        self._on_auto_change()

    def _on_mode_change(self):
        proto = self.protocol_var.get() if hasattr(self, "protocol_var") else "merav"
        if proto == "swan":
            self.mode_hint.config(
                text="Swan: תמיד שליחת ESC W ESC e (1B 57 1B 65) — אין מצב שידור רציף.")
        elif self.work_mode.get() == "listen":
            self.mode_hint.config(
                text="פסיבי: לא נשלח כלום לראש השקילה. דורש מוד תקשורת 0 או 3 "
                     "בתפריט F4.2.1. תקרת הקושחה ~20 קריאות/שנייה.")
        else:
            self.mode_hint.config(
                text="נשלח 'W' וממתינים לתשובה. שים לב: קושחת M6ConveyorAI "
                     "אינה מגיבה ל-W — השתמש במצב האזנה מולה.")

    def _on_protocol_change(self):
        if self.protocol_var.get() == "swan":
            self.work_mode.set("poll")
        self._on_mode_change()

    def _on_auto_change(self):
        if self.auto_var.get():
            self.frm_manual.grid_remove()
            self.frm_auto.grid()
            self.btn_start.config(text="▶ התחל ניטור")
        else:
            self.frm_auto.grid_remove()
            self.frm_manual.grid()
            self.btn_start.config(text="▶ התחל דגימה")

    def _load_settings(self):
        try:
            cfg = json.loads(CONFIG_FILE.read_text(encoding="utf-8"))
        except Exception:
            return
        self.conn_type.set(cfg.get("conn_type", "serial"))
        saved_port = cfg.get("port", "")
        if saved_port and saved_port in self.port_combo["values"]:
            self.port_var.set(saved_port)
        self.baud_var.set(cfg.get("baud", "9600"))
        self.ip_var.set(cfg.get("ip", "192.168.1.100"))
        self.tcp_port_var.set(cfg.get("tcp_port", "10001"))
        self.work_mode.set(cfg.get("work_mode", "listen"))
        self.protocol_var.set(cfg.get("protocol", "merav"))
        self.duration_var.set(cfg.get("duration", 1.0))
        self.show_each.set(cfg.get("show_each", True))
        self.auto_var.set(cfg.get("auto_weigh", False))
        self.threshold_var.set(cfg.get("auto_threshold", 0.5))
        self._on_type_change()
        self._on_protocol_change()
        self._on_auto_change()

    def _save_settings(self):
        cfg = {
            "conn_type":      self.conn_type.get(),
            "port":           self.port_var.get(),
            "baud":           self.baud_var.get(),
            "ip":             self.ip_var.get(),
            "tcp_port":       self.tcp_port_var.get(),
            "work_mode":      self.work_mode.get(),
            "protocol":       self.protocol_var.get(),
            "duration":       self.duration_var.get(),
            "show_each":      self.show_each.get(),
            "auto_weigh":     self.auto_var.get(),
            "auto_threshold": self.threshold_var.get(),
        }
        try:
            CONFIG_FILE.write_text(json.dumps(cfg, indent=2, ensure_ascii=False),
                                   encoding="utf-8")
        except Exception:
            pass

    def _on_type_change(self):
        if self.conn_type.get() == "serial":
            self.frm_tcp.grid_remove()
            self.frm_serial.grid()
        else:
            self.frm_serial.grid_remove()
            self.frm_tcp.grid()

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
        except queue.Empty:
            pass
        self.root.after(40, self._pump_queue)

    # ──────────────────────────────────────────────
    # Connection
    # ──────────────────────────────────────────────

    def _connect(self):
        try:
            if self.conn_type.get() == "serial":
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
                self.conn_mode = "serial"
                self._log(f"מחובר סיריאלי: {port_name} @ {self.baud_var.get()} baud", "summary")
            else:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(3.0)
                sock.connect((self.ip_var.get(), int(self.tcp_port_var.get())))
                sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                sock.settimeout(0.05)
                self.conn = sock
                self.conn_mode = "tcp"
                self._log(f"מחובר TCP: {self.ip_var.get()}:{self.tcp_port_var.get()}", "summary")

            self.btn_connect.config(state="disabled")
            self.btn_start.config(state="normal")
            self.btn_disconnect.config(state="normal")
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
        self.conn_mode = None
        self.btn_connect.config(state="normal")
        self.btn_start.config(state="disabled")
        self.btn_disconnect.config(state="disabled")
        self._log("מנותק")

    # ──────────────────────────────────────────────
    # Low-level I/O
    # ──────────────────────────────────────────────

    def _flush_input(self):
        """מרוקן נתונים ישנים שהצטברו בבאפר לפני תחילת דגימה."""
        try:
            if self.conn_mode == "serial":
                self.conn.reset_input_buffer()
            else:
                self.conn.settimeout(0.01)
                while True:
                    if not self.conn.recv(4096):
                        break
        except Exception:
            pass
        finally:
            if self.conn_mode == "tcp":
                try:
                    self.conn.settimeout(0.05)
                except Exception:
                    pass

    def _read_available(self) -> bytes:
        """קורא כל מה שזמין כרגע, בלי לחסום יותר מ-50ms."""
        try:
            if self.conn_mode == "serial":
                n = self.conn.in_waiting
                return self.conn.read(n if n else 1)
            else:
                try:
                    return self.conn.recv(512)
                except socket.timeout:
                    return b""
        except Exception as e:
            raise IOError(str(e))

    def _write(self, data: bytes):
        if self.conn_mode == "serial":
            self.conn.write(data)
        else:
            self.conn.sendall(data)

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
        protocol   = self.protocol_var.get()
        auto_weigh = self.auto_var.get()
        threshold  = self.threshold_var.get()

        self.running = True
        self.weight_var.set("...")
        self.decided_by_var.set("")
        self.readings_var.set("0")
        self.rate_var.set("0")
        mode_name  = "האזנה" if listen else "שליחת W"
        proto_name = {"dini": "DINI", "swan": "Swan"}.get(protocol, "מירב")
        self._log("─" * 56, "sep")

        if auto_weigh:
            self.btn_start.config(text="⏹ עצור ניטור", command=self._stop_auto)
            self._log(f"ניטור אוטו [{mode_name} | {proto_name}] — סף {threshold:.2f} kg", "summary")
            threading.Thread(target=self._auto_thread, daemon=True,
                             args=(listen, show_each, protocol, threshold)).start()
        else:
            self.btn_start.config(state="disabled")
            self._log(f"מתחיל דגימה [{mode_name} | {proto_name}] — {duration:.1f} שניות", "summary")
            threading.Thread(target=self._sample_thread, daemon=True,
                             args=(duration, listen, show_each, protocol)).start()

    def _stop_auto(self):
        self.running = False

    def _sample_thread(self, duration: float, listen: bool, show_each: bool, protocol: str):
        if protocol == "swan":
            use_bytes   = True
            poll_cmd    = b"\x1B\x57\x1B\x65"
            text_parser = None
        elif protocol == "dini":
            use_bytes   = False
            poll_cmd    = None if listen else b"READ\r\n"
            text_parser = parse_dini_frame
        else:
            use_bytes   = False
            poll_cmd    = None if listen else b"W\r\n"
            text_parser = parse_frame
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
                    if use_bytes:
                        w, status, special = parse_swan_frame_bytes(raw)
                    else:
                        text = raw.decode("ascii", errors="replace")
                        w, status, special = text_parser(text)

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
                    elif text.strip():
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

    def _add_history_row(self, time_str: str, decided: float):
        """מוסיף שורה לטבלת ההיסטוריה ומסמן אותה כ'אחרונה' בירוק."""
        self._event_count += 1
        # הסר הדגשה מכל השורות הקיימות
        for iid in self.history_tree.get_children():
            self.history_tree.item(iid, tags=("normal",))
        # הכנס חדשה בראש עם הדגשה
        self.history_tree.insert("", 0, tags=("latest",),
                                  values=(self._event_count, time_str,
                                          f"{decided:+.3f}"))

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
                self._log("במצב שליחת W: אם המאזניים הם M6ConveyorAI — "
                          "הקושחה לא מגיבה ל-W. עבור למצב האזנה.", "warn")
            else:
                self._log("במצב האזנה: ודא שמוד התקשורת בתפריט F4.2.1 הוא 0 או 3, "
                          "ושה-baud תואם (F4.2.3).", "warn")
            return

        weights = [w for _, w, _ in readings]
        count = len(weights)
        rate = count / elapsed if elapsed > 0 else 0

        decided, basis = self._decide_weight(readings)
        spread = max(weights) - min(weights)
        sd_str = f"  סטד: {statistics.stdev(weights):.4f}" if count > 1 else ""

        now_str = datetime.datetime.now().strftime("%H:%M:%S")
        self.weight_var.set(f"{decided:.3f}")
        self.decided_by_var.set(basis)
        self.readings_var.set(str(count))
        self.rate_var.set(f"{rate:.1f}")
        self.event_time_var.set(f"משך: {elapsed:.2f}s   {count} קריאות @ {rate:.1f}/s")
        self.event_detail_var.set(f"משקל: {decided:+.3f}   ({basis})")
        self.event_range_var.set(
            f"מין/מקס: {min(weights):+.3f} / {max(weights):+.3f}   "
            f"פיזור: {spread:.3f}{sd_str}")
        self._add_history_row(now_str, decided)

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

    def _auto_thread(self, listen: bool, show_each: bool, protocol: str, threshold: float):
        if protocol == "swan":
            use_bytes   = True
            poll_cmd    = b"\x1B\x57\x1B\x65"
            text_parser = None
        elif protocol == "dini":
            use_bytes   = False
            poll_cmd    = None if listen else b"READ\r\n"
            text_parser = parse_dini_frame
        else:
            use_bytes   = False
            poll_cmd    = None if listen else b"W\r\n"
            text_parser = parse_frame
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
                    if use_bytes:
                        w, status, special = parse_swan_frame_bytes(raw)
                    else:
                        text = raw.decode("ascii", errors="replace")
                        w, status, special = text_parser(text)

                    if special:
                        if state == "WEIGHING":
                            session_specials[special] = session_specials.get(special, 0) + 1
                        if show_each:
                            emit(f"  [{special}]", "warn")
                        continue

                    if w is None:
                        if text.strip():
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
        self._add_history_row(end_time_str, decided)

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
        self._on_auto_change()  # משחזר טקסט נכון לפי מצב ה-checkbox
        self._log("ניטור הסתיים", "summary")


# ──────────────────────────────────────────────
# Entry point
# ──────────────────────────────────────────────

if __name__ == "__main__":
    root = tk.Tk()
    root.state("zoomed")
    app = ScaleSampler(root)
    root.mainloop()
