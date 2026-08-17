"""
פרוטוקול IA-3116-U2i (Intelligent Appliance) — כרטיס 16 ממסרים + 16 כניסות
דיגיטליות מבודדות, מחובר בפורט טורי נפרד מהמשקל (חיווט/פרוטוקול שונים
לחלוטין). פקודות ASCII פשוטות, ראו REL/IA-3116-U2i_UM.pdf. כתובת המכשיר
קבועה על 00 — אין תמיכה בשרשור כמה כרטיסים כרגע.

תפקיד הכרטיס באפליקציה (מוגדר ע"י המשתמש):
  ממסר 1   — מנוע המסוע. הפעלה/כיבוי ידניים (כפתור "התחל"/"עצור" בעמוד
             הראשי, MainWindow._on_relay_motor_toggle).
  כניסה 1  — מעבר סגור→פתוח = הקופסה כולה כבר על משטח השקילה. טריגר
             להתחלת שקילה (WeighingEngine.force_start_session, במקום סף
             המשקל, כשמצב "עבודה עם ממסרים" פעיל — ראו engine.py).
  כניסה 2  — מעבר פתוח→סגור = הקופסה מתחילה לצאת. טריגר לסיום השקילה
             (WeighingEngine.force_stop_session) ולתחילת מיון.
  ממסר 2/3/4 — מיון לפי verdict (צהוב/ירוק/אדום), פועל activate_sort_relay()
             למשך RelaySettings.sort_pulse_seconds, או עד שמיון חדש מגיע
             (המוקדם מביניהם) — ראו _poll_loop.

כל הגישה לפורט (polling הכניסות + שליחת פקודות ממסר) עוברת דרך thread רץ
יחיד (_poll_loop) כדי שלא יתחרו שני threads על אותו פורט טורי בבת אחת —
בדיוק הבאג שכבר מצאנו ותיקנו במשקל עצמו (ראו pause_device_access ב-engine.py).
פקודות ממסר לא נכתבות ישירות מהתהליכון הקורא (Tk) — הן רק מצטרפות לתור
(_pending), ומבוצעות בפועל בתוך _poll_loop, בין סקירה לסקירה.
"""

import queue
import threading
import time

try:
    import serial
    SERIAL_AVAILABLE = True
except ImportError:
    SERIAL_AVAILABLE = False

# פורט/baud של הממסרים נבחרים מאותה רשימת פורטים — ראו engine.list_serial_ports()
# (מסונן מ"n/a"), אין צורך בעותק כפול כאן.

DEVICE_ADDR = "00"
POLL_INTERVAL = 0.05

MOTOR_RELAY = 1
SORT_RELAYS = {"yellow": 2, "green": 3, "red": 4}


class RelaySettings:
    def __init__(self):
        self.sort_pulse_seconds = 2.0


class RelayEngine:
    """
    מנהל את החיבור הטורי לכרטיס הממסרים. on_input1_edge/on_input2_edge הם
    callbacks אופציונליים (נקבעים ע"י MainWindow) שנקראים ישירות מ-_poll_loop
    כשמתגלה המעבר המתאים — לא עוברים דרך ui_queue, כי אלה רק קריאות פונקציה
    פשוטות (force_start_session/force_stop_session ב-WeighingEngine, שמגדירות
    דגל בוליאני — בטוח חוצה-thread כמו EngineSettings, ראו engine.py), ואין
    צורך בעיכוב הסבב-הלוך-חזור דרך התהליכון הראשי לאירוע קריטי-לזמן כזה.
    """

    def __init__(self, ui_queue: "queue.Queue"):
        self.ui_queue = ui_queue
        self.settings = RelaySettings()
        self.conn = None

        self.on_input1_edge = None
        self.on_input2_edge = None

        self._poll_thread = None
        self._running = False
        self._pending = []          # [(relay_num, bool), ...] — מבוצע בתוך _poll_loop
        self._sort_relay_active = None
        self._sort_off_at = None
        self._prev_input1 = None
        self._prev_input2 = None

    @property
    def connected(self):
        return self.conn is not None

    def connect(self, port_name: str, baud: int):
        if not SERIAL_AVAILABLE:
            raise RuntimeError("pyserial לא מותקן — הרץ: pip install pyserial")
        self.conn = serial.Serial(port=port_name, baudrate=baud, bytesize=8,
                                   parity="N", stopbits=1, timeout=0.2)
        self._prev_input1 = None
        self._prev_input2 = None
        self._sort_relay_active = None
        self._sort_off_at = None
        self._running = True
        self._poll_thread = threading.Thread(target=self._poll_loop, daemon=True)
        self._poll_thread.start()
        self._emit("relay_connected")

    def disconnect(self):
        self._running = False
        if self._poll_thread is not None:
            self._poll_thread.join(timeout=2.0)
            self._poll_thread = None
        if self.conn:
            try:
                self.conn.close()
            except Exception:
                pass
            self.conn = None
        self._emit("relay_disconnected")

    def set_relay(self, relay_num: int, on: bool):
        """ מוסיף פקודת ממסר לתור — תבוצע בפועל מתוך _poll_loop. """
        self._pending.append((relay_num, on))

    def activate_sort_relay(self, verdict: str):
        """
        מפעיל את ממסר המיון המתאים ל-verdict (green/red/yellow), למשך
        settings.sort_pulse_seconds. אם ממסר מיון קודם עדיין פעיל (קופסה
        קודמת עדיין בתהליך), הוא נכבה מיידית לפני שהחדש נדלק — כך שרק ממסר
        מיון אחד פעיל בכל זמן נתון.
        """
        relay_num = SORT_RELAYS.get(verdict)
        if relay_num is None or not self.connected:
            return
        if self._sort_relay_active is not None and self._sort_relay_active != relay_num:
            self.set_relay(self._sort_relay_active, False)
        self.set_relay(relay_num, True)
        self._sort_relay_active = relay_num
        self._sort_off_at = time.time() + self.settings.sort_pulse_seconds

    def _emit(self, kind, payload=None):
        self.ui_queue.put((kind, payload))

    def _write(self, data: bytes):
        self.conn.write(data)
        try:
            self.conn.flush()
        except Exception:
            pass

    def _read_line(self, timeout=0.3):
        buf = b""
        t0 = time.perf_counter()
        while time.perf_counter() - t0 < timeout:
            try:
                n = self.conn.in_waiting
                chunk = self.conn.read(n if n else 1)
            except Exception:
                break
            if chunk:
                buf += chunk
                if b"\r" in buf:
                    break
            elif buf:
                break
        return buf.decode("ascii", errors="replace").strip()

    def _send_relay_command(self, relay_num, on):
        cmd = "3" if on else "4"
        text = f"!{DEVICE_ADDR}{cmd}{relay_num - 1:02X}\r"
        try:
            self._write(text.encode("ascii"))
            self._read_line()
        except Exception as e:
            self._emit("relay_error", str(e))

    def _query_status(self):
        try:
            self._write(f"?{DEVICE_ADDR}2\r".encode("ascii"))
            resp = self._read_line()
        except Exception as e:
            self._emit("relay_error", str(e))
            return None
        if not resp.startswith("_") or len(resp) < 9:
            return None
        return resp[1:9]

    def _poll_loop(self):
        try:
            while self._running:
                while self._pending:
                    relay_num, on = self._pending.pop(0)
                    self._send_relay_command(relay_num, on)

                if self._sort_off_at is not None and time.time() >= self._sort_off_at:
                    self._send_relay_command(self._sort_relay_active, False)
                    self._sort_relay_active = None
                    self._sort_off_at = None

                status = self._query_status()
                if status is not None:
                    self._process_status(status)

                time.sleep(POLL_INTERVAL)
        except Exception as e:
            self._emit("relay_error", str(e))
            # לא רק מסמנים _running=False — סוגרים גם את conn ומאפסים אותו,
            # אחרת self.connected (conn is not None) עדיין יחזיר True אחרי
            # שה-thread שקורא כניסות/שולח פקודות כבר מת, ו-use_relay_sensors
            # יישאר "פעיל" בלי שאף דבר יזין force_start/force_stop לעולם —
            # השקילה תיתקע במצב WEIGHING לנצח. ראו MainWindow._pump_queue.
            self._running = False
            if self.conn:
                try:
                    self.conn.close()
                except Exception:
                    pass
                self.conn = None
            self._emit("relay_connection_lost")
            return
        self._running = False

    def _process_status(self, status_hex):
        """ status_hex = 8 תווי הקס (ABCDEFGH מהתגובה ל-?aa2). D (אינדקס 3)
        מכיל את ביטים 0-3 של כניסות 1-4; bit0=כניסה1, bit1=כניסה2. """
        try:
            input_nibble = int(status_hex[3], 16)
        except (ValueError, IndexError):
            return
        input1 = bool(input_nibble & 0x1)
        input2 = bool(input_nibble & 0x2)

        if self._prev_input1 is True and input1 is False and self.on_input1_edge:
            self.on_input1_edge()
        if self._prev_input2 is False and input2 is True and self.on_input2_edge:
            self.on_input2_edge()

        self._prev_input1 = input1
        self._prev_input2 = input2
