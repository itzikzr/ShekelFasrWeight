"""
WeighingEngine — חיבור RS-232 + כל התהליכונים הרצים מול ראש השקילה Swan.

כלל תהליכונים: שום thread כאן לא נוגע ב-Tk. כל תקשורת עם ה-UI קורית ע"י דחיפת
tuple (kind, payload) ל-ui_queue, שמנוקז רק מהתהליכון הראשי (root.after ב-main_window.py).
בגלל שהמצב הרציף (monitor) רץ ברקע לאורך כל חיי החיבור וחוצה הרבה שקילות, ה-threshold/
debounce/listen נשמרים ב-EngineSettings (ולא כ-Tk var) כדי שהתהליכון הראשי יוכל לעדכן אותם
תוך כדי תנועה בלי לדרוש התחברות מחדש.
"""

import threading
import queue
import time
import datetime
import re
import statistics

from .swan import parse_swan_frame
from .formatting import fmt_weight

try:
    import serial
    from serial.tools import list_ports as _list_ports
    SERIAL_AVAILABLE = True
except ImportError:
    SERIAL_AVAILABLE = False
    _list_ports = None


def list_serial_ports():
    """
    (device, description) לכל פורט עם התקן אמיתי מזוהה מאחוריו. עובד גם ב-Windows
    (COMx) וגם בלינוקס (/dev/ttyUSBx). פורטים בלי תיאור אמיתי (pyserial מחזיר
    "n/a" — למשל /dev/ttyS0..31 בלינוקס, כניסות סיריאליות ישנות שקיימות תמיד
    בליבה גם בלי שום חומרה מחוברת) מסוננים החוצה — הם לא שימושיים לחיבור בפועל
    ורק מציפים את הרשימה.
    """
    if not SERIAL_AVAILABLE:
        return []
    ports = [(p.device, p.description) for p in _list_ports.comports()
             if p.description and p.description != "n/a"]
    ports.sort(key=lambda x: (re.sub(r"\d+", "", x[0]),
                              int(m.group()) if (m := re.search(r"\d+", x[0])) else 0))
    return ports


PEAK_FRACTION_FLOOR = 0.5  # מתעלמים מהחיפוש בקריאות מתחת ל-50% מהשיא שהתגלה באותה שקילה


def decide_weight(readings):
    """
    מחזיר (משקל, תיאור_בסיס, אינדקס_התחלה, אינדקס_סוף) — שני האינדקסים האחרונים
    מסמנים את תחום ה-readings המקורי (פייתוני, [start:end)) שבו נעשה שימוש בפועל
    לחישוב, כדי שהקורא יוכל לסמן אותן בנפרד (ראו main_window._on_session_done).

    אלגוריתם: חלון הזזה N=6 — מחפשים את הרצף השטוח ביותר (מינימום stdev), אבל
    ורק בין קריאות שגבוהות מ-PEAK_FRACTION_FLOOR (50%) מהשיא שהתגלה באותה שקילה.
    ההגבלה הזו קריטית: בלעדיה, אם הפריט הוסר מהמשקל מהר אחרי שהתייצב, הזנב השקט
    שאחרי ההסרה (המשקל חוזר ל-~0 ונשאר שם ברצף) יכול להיות שטוח יותר סטטיסטית
    מהמדרגה האמיתית הקצרה — והאלגוריתם היה "בוחר" בטעות באפס במקום במשקל בפועל.
    ראו את שקילות 105-108 (מדרגה ארוכה — עבד גם בלי ההגבלה) מול 109-112 (מדרגה
    קצרה — בלי ההגבלה נבחר הזנב האפסי) שהובילו לתיקון הזה.
    fallback לחציון קריאות המועמדות (או כל הקריאות אם אין אף אחת מעל הסף) כשיש
    פחות מ-N מועמדות.
    """
    N = 6
    weights = [w for _, w, _ in readings]
    count = len(weights)
    if count == 0:
        return 0.0, "אין קריאות", 0, 0

    peak = max(weights)
    cutoff = peak * PEAK_FRACTION_FLOOR
    candidates = [i for i, w in enumerate(weights) if w > cutoff]

    if len(candidates) >= N:
        best_sd = float("inf")
        best_pos = 0
        for pos in range(len(candidates) - N + 1):
            window_idx = candidates[pos:pos + N]
            sd = statistics.stdev(weights[i] for i in window_idx)
            if sd < best_sd:
                best_sd = sd
                best_pos = pos
        window_idx = candidates[best_pos:best_pos + N]
        decided = statistics.median(weights[i] for i in window_idx)
        start_idx, end_idx = window_idx[0], window_idx[-1] + 1
        basis = f"חלון שטוח #{start_idx+1}–{end_idx}  stdev={best_sd:.4f}"
        return decided, basis, start_idx, end_idx

    fallback_idx = candidates or list(range(count))
    decided = statistics.median(weights[i] for i in fallback_idx)
    basis = f"חציון {len(fallback_idx)} קריאות"
    return decided, basis, fallback_idx[0], fallback_idx[-1] + 1


class EngineSettings:
    """ ערכים פשוטים (לא Tk) שהתהליכון הרציף קורא כל טיק. """

    def __init__(self):
        self.listen = True          # True=האזנה, False=שליחת 'W'
        self.threshold = 0.5        # kg — סף להתחלת שקילה
        self.debounce_s = 0.4       # שניות מתחת לסף לפני סיום שקילה
        self.show_each = True       # האם לשדר ל-log כל קריאה בודדת (דיאגנוסטיקה)
        self.min_save_weight = 0.0  # kg — משקל שהוחלט מתחתיו לא נשמר ל-DB
        # שני צירים עצמאיים לגמרי, לא דגל בינארי אחד: מה מתחיל שקילה
        # ("threshold" | "sensor1_close" | "sensor1_open") ומה מסיים אותה
        # ("threshold" | "sensor2_close" | "sensor2_open") — ראו
        # on_sensor1_close/on_sensor1_open/on_sensor2_close/on_sensor2_open
        # למטה ו-MainWindow._sync_relay_sensor_mode. ברירות המחדל כאן הן
        # "לא ממסרים בכלל" (ערכים אינרטיים בזמן בנייה); הערכים האמיתיים
        # מגיעים מ-_sync_relay_sensor_mode לאחר טעינת ההגדרות.
        self.start_trigger = "threshold"
        self.stop_trigger = "threshold"
        # True = ממסרים מחוברים ופעילים (בלי קשר לאיזה start_trigger/
        # stop_trigger נבחר). כש-start_trigger=="threshold" וגם relay_active
        # פעיל, התחלת שקילה חדשה דורשת גם armed=True ("כפתור התחל הוא מפתח
        # הפעלה", לא רק סף משקל תמיד-פעיל) — בדיוק כמו require_armed הישן.
        # כש-relay_active כבוי (אין ממסרים בכלל), armed לא משפיע — סף המשקל
        # פעיל תמיד, כמו לפני שהתכונה הזו נוספה. ממסרי המיון (2-4) פועלים
        # כרגיל בסוף שקילה בכל מקרה, בלי קשר לכל זה.
        self.relay_active = False
        # True בזמן שהמנוע (ממסר 1) דלוק — כלומר המפעיל לחץ "התחל". רלוונטי
        # רק כש-start_trigger=="threshold" וגם relay_active פעיל.
        self.armed = False


class WeighingEngine:
    FIRMWARE_STREAM_HZ = 20

    def __init__(self, ui_queue: "queue.Queue"):
        self.ui_queue = ui_queue
        self.conn = None
        self.settings = EngineSettings()

        self._monitor_running = False
        self._monitor_thread = None
        self._diag_running = False
        self._stream_started = False

        self._calib_active = False
        self._calib_event = threading.Event()

        self._device_busy = False

        # דגלים בוליאניים פשוטים, לא Tk — נכתבים מ-RelayEngine._poll_loop
        # (thread נפרד) ונקראים כל טיק ע"י _monitor_loop, בדיוק כמו
        # EngineSettings; בטוח בלי lock כי אלה קריאה/כתיבה אטומיות של bool.
        self._pending_force_start = False
        self._pending_force_stop = False

    # ארבע השיטות הבאות נקראות מ-RelayEngine (thread נפרד) בכל מעבר גולמי
    # של כניסה 1/2 — הן, לא RelayEngine, קובעות אם המעבר הזה בפועל רלוונטי,
    # לפי start_trigger/stop_trigger הנוכחיים ב-EngineSettings. כך אפשר
    # לשנות את הבחירה בהגדרות בזמן ריצה בלי לחבר מחדש callbacks.

    def on_sensor1_close(self):
        if self.settings.start_trigger == "sensor1_close":
            self._pending_force_start = True

    def on_sensor1_open(self):
        if self.settings.start_trigger == "sensor1_open":
            self._pending_force_start = True

    def on_sensor2_close(self):
        if self.settings.stop_trigger == "sensor2_close":
            self._pending_force_stop = True

    def on_sensor2_open(self):
        if self.settings.stop_trigger == "sensor2_open":
            self._pending_force_stop = True

    # ──────────────────────────────────────────────
    # Connection
    # ──────────────────────────────────────────────

    @property
    def connected(self):
        return self.conn is not None

    def connect(self, port_name: str, baud: int):
        if not SERIAL_AVAILABLE:
            raise RuntimeError("pyserial לא מותקן — הרץ: pip install pyserial")
        self.conn = serial.Serial(port=port_name, baudrate=baud, bytesize=8,
                                   parity="N", stopbits=1, timeout=0.05)
        self._stream_started = False
        if self.settings.listen:
            # שידור רציף — 'S' גדולה מתחילה את השידור, נשלחת פעם אחת בחיבור.
            self._write(b"S")
            self._stream_started = True

    def disconnect(self):
        self.stop_monitor()
        self.stop_live()
        self._diag_running = False
        if self.conn:
            if self._stream_started:
                try:
                    self._write(b"s")
                except Exception:
                    pass
                self._stream_started = False
            try:
                self.conn.close()
            except Exception:
                pass
            self.conn = None

    def _flush_input(self):
        try:
            self.conn.reset_input_buffer()
        except Exception:
            pass

    def _read_available(self) -> bytes:
        try:
            n = self.conn.in_waiting
            return self.conn.read(n if n else 1)
        except Exception as e:
            raise IOError(str(e))

    def _write(self, data: bytes):
        self.conn.write(data)
        # בלי flush מפורש, כתיבה ל-port ממתמשת בתור ה-OS בלי הבטחת שידור בפועל
        # לפני שהקוד ממשיך — נצפה כתקלה ספציפית ל-Linux/FTDI (לא ב-Windows):
        # פקודות תצורה חד-תווית (F/B/R/I/V/A) לא קיבלו שום תגובה בכלל, לא רק
        # תגובה "מלוכלכת". flush() חוסם עד שה-OS מדווח שהנתונים באמת נשלחו.
        try:
            self.conn.flush()
        except Exception:
            pass

    def _emit(self, kind, payload=None):
        self.ui_queue.put((kind, payload))

    def _read_until(self, patterns, timeout_idle=1.0, timeout_total=30.0,
                    on_chunk=None, should_continue=None):
        """
        קורא מהפורט עד שאחד מ-patterns מופיע בטקסט שהתקבל, עד שחולף timeout_idle
        בלי נתונים חדשים, או עד timeout_total כולל. patterns ריק = לוקח כל מה שמגיע
        עד שקט (שימושי לתגובות קצרות בפורמט לא קבוע כמו Z/T/I/A).
        on_chunk(buf: bytes) — אופציונלי, נקרא עם המאגר המצטבר בכל פעם שמגיעים בייטים
        חדשים (למשל כדי לספור '*' להתקדמות כיול). should_continue() — אופציונלי,
        נבדק בכל איטרציה; מחזיר False מפסיק את הקריאה מוקדם (למשל ביטול כיול).
        """
        buf = b""
        t0 = time.perf_counter()
        t_last = t0
        while time.perf_counter() - t0 < timeout_total:
            if should_continue is not None and not should_continue():
                break
            try:
                chunk = self._read_available()
            except Exception:
                break
            if chunk:
                buf += chunk
                t_last = time.perf_counter()
                if on_chunk:
                    on_chunk(buf)
                decoded = buf.decode("ascii", errors="replace")
                if patterns and any(p in decoded for p in patterns):
                    time.sleep(0.1)
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

    def _parse_int_after(self, text, key):
        """ מחלץ מספר אחרי 'key=' בתגובת המשקל (למשל 'Full=0099999g' -> 99999). """
        m = re.search(re.escape(key) + r"=(\d+)", text)
        return int(m.group(1)) if m else None

    # ──────────────────────────────────────────────
    # Continuous auto-weigh — מסך הבית
    # ──────────────────────────────────────────────

    def start_monitor(self):
        if self._monitor_running or not self.connected:
            return
        self._monitor_running = True
        self._monitor_thread = threading.Thread(target=self._monitor_loop, daemon=True)
        self._monitor_thread.start()

    def stop_monitor(self, wait: bool = False):
        self._monitor_running = False
        if wait and self._monitor_thread is not None:
            self._monitor_thread.join(timeout=2.0)

    def pause_device_access(self):
        """
        עוצר את המוניטור (וממתין שהוא באמת יפסיק — לא רק מסמן דגל) ומשתיק את השידור
        הרציף בפועל על המשקל עצמו ('s'), לא רק מפסיק לקרוא אותו. בלי זה, בזמן מצב
        האזנה, ראש השקילה ממשיך לשדר חבילות משקל כל הזמן — ואלה מתערבבות עם התגובות
        של פקודות תצורה/כיול וגורמות ל-_read_until לא לזהות "שקט" (idle) בכלל, כך
        שהוא רץ עד timeout_total גם כשהתגובה האמיתית כבר הגיעה. קוראים לזה לפני כל
        מסך שמדבר עם המשקל בפקודות ישירות (כיול, תצורת משקל) — ראו resume_after_device_access.
        """
        self.stop_monitor(wait=True)
        if self._stream_started:
            try:
                self._write(b"s")
                # השהיה קטנה כדי לתת למשקל לצאת בפועל ממצב שידור רציף לפני
                # שפקודות תצורה נשלחות — ואז ניקוי מה שהגיע בזמן המעבר (סוף
                # חבילת משקל אחרונה וכו') כדי שהתגובה הראשונה שנקרא לא תהיה
                # מלוכלכת משאריות השידור.
                time.sleep(0.2)
                self._flush_input()
            except Exception:
                pass
            self._stream_started = False

    def resume_after_device_access(self):
        """ ההופכי של pause_device_access — מפעיל מחדש שידור (אם מצב האזנה) ואת המוניטור. """
        if not self.connected:
            return
        if self.settings.listen:
            try:
                self._write(b"S")
                self._stream_started = True
            except Exception:
                pass
        self.start_monitor()

    def _monitor_loop(self):
        buf = b""
        state = "IDLE"                # "IDLE" | "WEIGHING"
        session_readings = []
        session_start = 0.0
        session_wall_start = None
        below_since = None

        try:
            self._flush_input()
        except Exception:
            pass

        try:
            while self._monitor_running:
                poll_cmd = None if self.settings.listen else b"W"
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
                    w, status, special, decimals = parse_swan_frame(text)

                    if special or w is None:
                        if raw and self.settings.show_each:
                            self._emit("log", (f"פסולת: {raw!r}", "warn" if special else "error"))
                        continue

                    self._emit("live", (w, decimals))
                    threshold  = self.settings.threshold
                    debounce_s = self.settings.debounce_s

                    start_trigger = self.settings.start_trigger
                    stop_trigger = self.settings.stop_trigger

                    if state == "IDLE":
                        # start_trigger != "threshold": כניסת חיישן (close/open
                        # של כניסה1, ראו on_sensor1_close/on_sensor1_open) מחליפה
                        # את בדיקת הסף — לא בודקים w בכלל, בדיוק כמו שהתבקש
                        # ("עובד אז מחליף").
                        if start_trigger != "threshold":
                            start_now = self._pending_force_start
                        else:
                            # relay_active וגם start_trigger=="threshold": ממסרים
                            # מחוברים אבל בחירת ההתחלה היא סף משקל — סף המשקל
                            # הרגיל, אבל רק כשהמפעיל לחץ "התחל" (armed=True).
                            # לא ממסרים בכלל -> relay_active=False -> בדיוק כמו קודם.
                            gate_ok = (not self.settings.relay_active) or self.settings.armed
                            start_now = gate_ok and (w > threshold)
                        if start_now:
                            self._pending_force_start = False
                            # מבטלים גם דגל סיום תקוע — כניסה2 יכולה להידלק
                            # (רעד/רעש, או קופסה שרק חיישן 2 שלה נגע בלי
                            # שחיישן 1 נדלק בשבילה) *בזמן ש-IDLE*, לפני שיש
                            # שקילה פעילה בכלל; on_sensor2_close/on_sensor2_open
                            # נכתבים מ-thread אחר בלי לדעת שאנחנו ב-IDLE, אז הדגל
                            # נשאר True בלי שאף אחד קורא אותו. בלי האיפוס הזה,
                            # ברגע שהשקילה הבאה מתחילה כאן, הענף של WEIGHING
                            # היה מיד "רואה" את הדגל התקוע ומסיים אותה כמעט
                            # ברגע שהתחילה — זה מה שנצפה בפועל ("ברגע שמזהים
                            # כניסה1, זה כותב את משקל הקופסה" מיד, עם נתונים
                            # כמעט ריקים) כשכניסה2 הגיעה לפני כניסה1 עבור
                            # הקופסה הנוכחית.
                            self._pending_force_stop = False
                            state = "WEIGHING"
                            session_start = now
                            session_wall_start = datetime.datetime.now()
                            session_readings = [(0.0, w, status)]
                            below_since = None
                            self._emit("session_start", w)
                            if self.settings.show_each:
                                self._emit("log", (f"── שקילה חדשה: {fmt_weight(w, force_sign=True)} "
                                                   f"> סף {threshold:.2f} ──", "summary"))

                    elif state == "WEIGHING":
                        if start_trigger != "threshold" and self._pending_force_start:
                            # כניסה1 נדלקה בשנית *באמצע* שקילה, בלי שכניסה2
                            # סימנה סיום קודם — המפעיל הוריד את הקופסה והריץ
                            # שוב, לא רעש שיש להתעלם ממנו. מתחילים שקילה חדשה
                            # מאפס (כמו IDLE->WEIGHING הרגיל) במקום להמשיך לצבור
                            # מ-session_start המקורי — זה מה שגרם ל"זמן מהפעם
                            # הראשונה" שנצפה בפועל: elapsed חושב תמיד מה-start
                            # הישן, לעולם לא התאפס כשכניסה1 נדלקה שוב.
                            self._pending_force_start = False
                            # וכמו ב-IDLE->WEIGHING: מבטלים גם דגל סיום תקוע
                            # שנשאר מהשקילה הישנה שמתבטלת כרגע — אחרת השקילה
                            # החדשה הזו תיגמר באופן מיידי בדיוק מהסיבה שתוקנה
                            # למעלה.
                            self._pending_force_stop = False
                            session_start = now
                            session_wall_start = datetime.datetime.now()
                            session_readings = [(0.0, w, status)]
                            below_since = None
                            self._emit("session_start", w)
                            if self.settings.show_each:
                                self._emit("log", ("── שקילה הופעלה מחדש (כניסה1 נדלקה שוב) ──",
                                                   "summary"))
                            continue

                        t_sess = now - session_start
                        session_readings.append((t_sess, w, status))
                        if self.settings.show_each:
                            self._emit("log", (f"  #{len(session_readings):>4}   "
                                               f"{t_sess:6.3f}s   "
                                               f"{fmt_weight(w, force_sign=True):>10}", "reading"))

                        # ממשיכים לצבור readings בשני המצבים (decide_weight
                        # צריך אותם) — רק תנאי הסיום עצמו משתנה.
                        if stop_trigger != "threshold":
                            if self._pending_force_stop:
                                self._pending_force_stop = False
                                # מבטלים גם דגל התחלה תקוע — כניסה1 יכולה
                                # להאיר שוב (קופסה נוספת שנדחקת/מתקרבת, רעד
                                # בחיווט וכו') *בזמן* שהשקילה הנוכחית עדיין
                                # פעילה; on_sensor1_close/on_sensor1_open
                                # נכתבים מ-thread אחר בלי לדעת שאנחנו כבר ב-WEIGHING, אז הדגל
                                # נשאר True בלי שאף אחד קורא אותו. בלי האיפוס
                                # הזה, הרגע שחוזרים ל-IDLE כאן היה מתחיל שקילה
                                # חדשה מיידית מהדגל התקוע, לפני שכניסה1 באמת
                                # נדלקה מחדש בשביל הקופסה הבאה — זה מה שנצפה
                                # בפועל ("ברגע שמזהים סיום כניסה1, מתחיל תהליך
                                # חדש") כשכניסה2 (סיום) הגיעה לפני שכניסה1
                                # התאפסה בפועל עבור הקופסה הנוכחית.
                                self._pending_force_start = False
                                elapsed = now - session_start
                                state = "IDLE"
                                self._emit("session_done",
                                           (session_readings, elapsed, session_wall_start,
                                            datetime.datetime.now()))
                                session_readings = []
                                below_since = None
                        elif w <= threshold:
                            if below_since is None:
                                below_since = now
                            elif (now - below_since) >= debounce_s:
                                elapsed = now - session_start
                                state = "IDLE"
                                self._emit("session_done",
                                           (session_readings, elapsed, session_wall_start,
                                            datetime.datetime.now()))
                                session_readings = []
                                below_since = None
                        else:
                            below_since = None

        except IOError as e:
            self._emit("log", (f"שגיאת תקשורת: {e}", "error"))
        except Exception as e:
            self._emit("log", (f"שגיאה: {e}", "error"))

        if state == "WEIGHING" and session_readings:
            elapsed = time.perf_counter() - session_start
            self._emit("session_done",
                       (session_readings, elapsed, session_wall_start, datetime.datetime.now()))

        self._monitor_running = False
        self._emit("monitor_stopped", None)

    # ──────────────────────────────────────────────
    # Diagnostics: דגימה חד-פעמית
    # ──────────────────────────────────────────────

    def sample_once(self, duration: float):
        if self._diag_running:
            return
        self._diag_running = True
        threading.Thread(target=self._sample_loop, daemon=True, args=(duration,)).start()

    def stop_diagnostics(self):
        self._diag_running = False

    def _sample_loop(self, duration: float):
        buf = b""
        readings = []
        bad = 0
        idx = 0

        try:
            self._flush_input()
        except Exception:
            pass

        start = time.perf_counter()
        try:
            while self._diag_running and (time.perf_counter() - start) < duration:
                poll_cmd = None if self.settings.listen else b"W"
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
                    w, status, special, _decimals = parse_swan_frame(text)
                    if w is not None:
                        idx += 1
                        readings.append((t, w, status))
                        if self.settings.show_each:
                            self._emit("log", (f"  #{idx:>4}   {t:6.3f}s   "
                                               f"{fmt_weight(w, force_sign=True):>10}", "reading"))
                    elif raw:
                        bad += 1
                        if self.settings.show_each:
                            self._emit("log", (f"  {t:6.3f}s   פסולת: {raw!r}", "error"))
        except IOError as e:
            self._emit("log", (f"שגיאת תקשורת: {e}", "error"))
        except Exception as e:
            self._emit("log", (f"שגיאה: {e}", "error"))

        elapsed = time.perf_counter() - start
        self._diag_running = False
        self._emit("sample_done", (readings, bad, elapsed))

    # ──────────────────────────────────────────────
    # Diagnostics: LIVE — חלונות חוזרים
    # ──────────────────────────────────────────────

    def start_live(self, window_s: float):
        if self._diag_running:
            return
        self._diag_running = True
        threading.Thread(target=self._live_loop, daemon=True, args=(window_s,)).start()

    def stop_live(self):
        self._diag_running = False

    def _live_loop(self, window_s: float):
        buf = b""
        tick_num = 0

        try:
            self._flush_input()
        except Exception:
            pass

        try:
            while self._diag_running:
                window_readings = []
                window_start = time.perf_counter()
                window_wall = datetime.datetime.now()

                while self._diag_running and (time.perf_counter() - window_start) < window_s:
                    poll_cmd = None if self.settings.listen else b"W"
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
                        w, status, special, decimals = parse_swan_frame(text)
                        if w is not None:
                            window_readings.append((t_rel, w, status))
                            self._emit("live", (w, decimals))
                            if self.settings.show_each:
                                self._emit("log", (f"  #{len(window_readings):>4}   "
                                                   f"{t_rel:6.3f}s   "
                                                   f"{fmt_weight(w, force_sign=True):>10}", "reading"))

                elapsed = time.perf_counter() - window_start
                if window_readings:
                    tick_num += 1
                    self._emit("live_tick", (tick_num, window_wall, datetime.datetime.now(),
                                             window_readings, elapsed))
        except IOError as e:
            self._emit("log", (f"שגיאת תקשורת: {e}", "error"))
        except Exception as e:
            self._emit("log", (f"שגיאה: {e}", "error"))

        self._diag_running = False
        self._emit("live_stopped", None)

    # ──────────────────────────────────────────────
    # Swan calibration wizard (PC0035)
    # ──────────────────────────────────────────────

    def start_calibration(self, weight_g: int):
        self._calib_active = True
        self._calib_event.clear()
        threading.Thread(target=self._calib_thread, daemon=True, args=(weight_g,)).start()

    def calib_continue(self):
        self._calib_event.set()

    def calib_cancel(self):
        self._calib_active = False
        self._calib_event.set()

    def _calib_thread(self, weight_g: int):
        def emit(kind, data=None):
            self._emit("calib", (kind, data))

        def read_until(patterns, timeout_idle=1.0, timeout_total=30.0):
            def on_chunk(buf):
                if buf.count(b"*"):
                    emit("progress", buf.count(b"*"))
            return self._read_until(patterns, timeout_idle, timeout_total,
                                    on_chunk=on_chunk, should_continue=lambda: self._calib_active)

        try:
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
            self._emit("calib_disconnect", None)

        except IOError as e:
            emit("error", f"שגיאת תקשורת: {e}")
        except Exception as e:
            emit("error", f"שגיאה: {e}")

    # ──────────────────────────────────────────────
    # Device configuration (Swan_SC_Protocol.md — פקודות ישירות)
    # ──────────────────────────────────────────────
    #
    # כל הפקודות פה הן חילופי בקשה/תשובה קצרים על אותו פורט שהמוניטור הרציף
    # משתמש בו — הקורא (SettingsWindow, טאב "תצורת משקל"/"איפוס יצרן") חייב
    # לעצור את המוניטור (pause_device_access) לפני שליחת פקודות, בדיוק כמו
    # שאשף הכיול עושה.
    # _device_busy מונע שתי פקודות-מכשיר במקביל על אותו פורט.

    def _device_cmd(self, kind, fn):
        """
        מריץ fn() בתהליכון נפרד (עם הגנת _device_busy) ומשדר את התוצאה שלו
        ל-ui_queue תחת kind. fn מחזירה dict, לפחות עם מפתח "ok".
        """
        if self._device_busy or not self.connected:
            return
        self._device_busy = True

        def run():
            try:
                result = fn()
            except IOError as e:
                result = {"ok": False, "error": f"שגיאת תקשורת: {e}"}
            except Exception as e:
                result = {"ok": False, "error": f"שגיאה: {e}"}
            self._device_busy = False
            self._emit(kind, result)

        threading.Thread(target=run, daemon=True).start()

    # ── Full scale (F) ──

    def query_full_scale(self):
        def fn():
            self._flush_input()
            self._write(b"F")
            text = self._read_until(["\n"], timeout_idle=1.0, timeout_total=5.0)
            grams = self._parse_int_after(text, "Full")
            if grams is None:
                return {"ok": False, "error": f"תגובה לא צפויה: {text!r}"}
            return {"ok": True, "grams": grams, "raw": text.strip()}
        self._device_cmd("device_full_scale", fn)

    def set_full_scale(self, grams: int):
        def fn():
            self._flush_input()
            self._write(b"F")
            self._read_until(["\n"], timeout_idle=1.0, timeout_total=5.0)
            self._write(f"{int(grams)}\r".encode("ascii"))
            text = self._read_until(["\n"], timeout_idle=1.0, timeout_total=5.0)
            grams_saved = self._parse_int_after(text, "Full")
            ok = "saved" in text.lower() and grams_saved is not None
            if not ok:
                return {"ok": False, "error": f"תגובה לא צפויה: {text!r}"}
            return {"ok": True, "grams": grams_saved, "raw": text.strip()}
        self._device_cmd("device_full_scale_set", fn)

    # ── Baud rate (B) — משנה הגדרה ומאתחל את המשקל ──

    def query_baud(self):
        def fn():
            self._flush_input()
            self._write(b"B")
            text = self._read_until(["\n"], timeout_idle=1.0, timeout_total=5.0)
            # "Baud=XXXXXXX" מדווח את קצב ה-baud הנוכחי בפועל (למשל 9600), לא אינדקס
            # בתפריט — אומת מול חומרה אמיתית: "Baud=0009600" כשה-baud היה 9600.
            baud = self._parse_int_after(text, "Baud")
            return {"ok": baud is not None, "baud": baud, "raw": text.strip()}
        self._device_cmd("device_baud", fn)

    def set_baud(self, index: int):
        def fn():
            self._flush_input()
            self._write(b"B")
            self._read_until(["\n"], timeout_idle=1.0, timeout_total=5.0)
            self._write(str(int(index)).encode("ascii"))
            text = self._read_until(["\n"], timeout_idle=1.5, timeout_total=6.0)
            ok = "saved" in text.lower()
            if not ok:
                return {"ok": False, "error": f"תגובה לא צפויה: {text!r}"}
            return {"ok": True, "index": index, "raw": text.strip()}
        self._device_cmd("device_baud_set", fn)

    # ── Rounding step (R) ──

    def query_rounding(self):
        def fn():
            self._flush_input()
            self._write(b"R")
            text = self._read_until(["\n"], timeout_idle=1.0, timeout_total=5.0)
            idx = self._parse_int_after(text, "Round")
            return {"ok": idx is not None, "index": idx, "raw": text.strip()}
        self._device_cmd("device_rounding", fn)

    def set_rounding(self, index: int):
        def fn():
            self._flush_input()
            self._write(b"R")
            self._read_until(["\n"], timeout_idle=1.0, timeout_total=5.0)
            self._write(str(int(index)).encode("ascii"))
            text = self._read_until(["\n"], timeout_idle=1.0, timeout_total=5.0)
            ok = "saved" in text.lower()
            if not ok:
                return {"ok": False, "error": f"תגובה לא צפויה: {text!r}"}
            return {"ok": True, "index": index, "raw": text.strip()}
        self._device_cmd("device_rounding_set", fn)

    # ── Decimal format / Disform (Q) ──

    def query_disform(self):
        def fn():
            self._flush_input()
            self._write(b"Q")
            text = self._read_until(["\n"], timeout_idle=1.0, timeout_total=5.0)
            idx = self._parse_int_after(text, "Disform")
            return {"ok": idx is not None, "index": idx, "raw": text.strip()}
        self._device_cmd("device_disform", fn)

    def set_disform(self, index: int):
        def fn():
            self._flush_input()
            self._write(b"Q")
            self._read_until(["\n"], timeout_idle=1.0, timeout_total=5.0)
            self._write(str(int(index)).encode("ascii"))
            text = self._read_until(["\n"], timeout_idle=1.0, timeout_total=5.0)
            ok = "saved" in text.lower()
            if not ok:
                return {"ok": False, "error": f"תגובה לא צפויה: {text!r}"}
            return {"ok": True, "index": index, "raw": text.strip()}
        self._device_cmd("device_disform_set", fn)

    # ── פעולות: אפס / טרה / ביטול טרה ──

    def zero(self):
        def fn():
            self._flush_input()
            self._write(b"Z")
            text = self._read_until([], timeout_idle=0.6, timeout_total=5.0)
            stripped = text.strip()
            if stripped.startswith("Z"):
                return {"ok": True, "raw": stripped}
            if stripped.startswith("F"):
                return {"ok": False, "error": "האיפוס נדחה (עומס, נעילה, או מחוץ לטווח)",
                        "raw": stripped}
            return {"ok": False, "error": f"אין תגובה תקינה: {stripped!r}", "raw": stripped}
        self._device_cmd("device_zero", fn)

    def tare(self):
        def fn():
            self._flush_input()
            self._write(b"T")
            text = self._read_until([], timeout_idle=1.5, timeout_total=6.0)
            stripped = text.strip()
            if stripped.startswith("T"):
                return {"ok": True, "raw": stripped}
            if stripped.startswith("F"):
                return {"ok": False, "error": "הטרה נדחתה — לא התייצב בזמן, נעילה, או עומס",
                        "raw": stripped}
            return {"ok": False, "error": f"אין תגובה תקינה: {stripped!r}", "raw": stripped}
        self._device_cmd("device_tare", fn)

    def clear_tare(self):
        def fn():
            self._flush_input()
            self._write(b"C")
            return {"ok": True, "raw": ""}
        self._device_cmd("device_clear_tare", fn)

    # ── מידע (קריאה בלבד) ──

    def get_info(self):
        def fn():
            self._flush_input()
            self._write(b"I")
            text = self._read_until([], timeout_idle=0.6, timeout_total=3.0)
            return {"ok": bool(text.strip()), "text": text.strip()}
        self._device_cmd("device_info", fn)

    def get_version(self):
        def fn():
            self._flush_input()
            self._write(b"V")
            text = self._read_until(["\n"], timeout_idle=0.6, timeout_total=3.0)
            return {"ok": bool(text.strip()), "text": text.strip()}
        self._device_cmd("device_version", fn)

    def get_raw_ad(self):
        def fn():
            self._flush_input()
            self._write(b"A")
            text = self._read_until(["\n"], timeout_idle=0.6, timeout_total=3.0)
            return {"ok": bool(text.strip()), "text": text.strip()}
        self._device_cmd("device_raw_ad", fn)

    # ── איפוס יצרן (ESC D — הרסני, כל ה-NVRAM חוזר לברירת המחדל) ──

    def factory_reset(self):
        if self._device_busy or not self.connected:
            return
        self._device_busy = True

        def run():
            try:
                self._flush_input()
                # ESC D ESC e — 'e' קטנה בכוונה, כמו בכיול (_calib_thread): זה מה
                # שאומת מול המשקל האמיתי, למרות ש-Swan_SC_Protocol.md מתעד 'E' גדולה.
                self._write(b"\x1B\x44\x1B\x65")
                text = self._read_until([], timeout_idle=1.5, timeout_total=8.0)
                result = {"ok": True, "raw": text.strip()}
            except IOError as e:
                result = {"ok": False, "error": f"שגיאת תקשורת: {e}"}
            except Exception as e:
                result = {"ok": False, "error": f"שגיאה: {e}"}
            self._device_busy = False
            self._emit("device_factory_reset", result)
            self._emit("device_reset_disconnect", None)

        threading.Thread(target=run, daemon=True).start()
