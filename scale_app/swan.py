"""
Swan scale-head protocols.

פרוטוקול קריאת משקל:
  TX: 'W'                                 (בייט בודד, בלי CR/LF) — במצב שליחה/בקשה
  RX: <+/-><7 תווים — ספרות + נקודה אופציונלית><CR>

מצב שידור רציף (listen): 'S' גדולה נשלחת פעם אחת בעת ההתחברות כדי להתחיל את השידור,
ו-'s' קטנה נשלחת פעם אחת בעת ההתנתקות כדי להפסיק אותו (ראו WeighingEngine.connect/disconnect).

פרוטוקול כיול (Swan PC0035, ESC-based) נמצא ב-engine.py — פרוטוקול נפרד, לא קשור לקריאת משקל.
"""

import re

_SWAN_TEXT_RE = re.compile(r'^([+\-])([\d.]{7})$')


def parse_swan_frame(frame: str):
    """
    Swan text — [+/-] ואחריו 7 תווים (ספרות, ואולי נקודה אחת בפנים) ו-CR.
    לא משנה איפה הנקודה נמצאת (או אם בכלל קיימת) — פשוט קוראים את המספר.
    דוגמאות: '+000.010' וגם '+0000010' -> 0.010 kg; '+00370.0' -> 370.0 kg
    מחזיר (weight|None, status_str, special|None, decimals) — decimals הוא מספר
    הספרות בפועל אחרי הנקודה בפריים הגולמי (0 אם אין נקודה כלל, ראו הערה למטה),
    כדי שהצגת "משקל חי" תוכל להראות בדיוק כמו שההתקן שלח (למשל '370.0' ולא
    '370.000' שנכפה ע"י פורמט קבוע) — ראו formatting.fmt_weight(decimals=...).
    """
    s = frame.strip("\r\n\x00")
    m = _SWAN_TEXT_RE.match(s)
    if not m:
        return None, "", None, None
    sign, body = m.groups()
    try:
        if "." in body:
            weight = float(sign + body)
            decimals = len(body.split(".", 1)[1])
        else:
            weight = int(body) / 1000.0
            if sign == "-":
                weight = -weight
            decimals = 3  # body הוא גרמים שלמים; ההצגה הקיימת ממירה לק"ג ב-3 ספרות
    except ValueError:
        return None, "", None, None
    return weight, "", None, decimals


def parse_swan_frame_bytes(raw: bytes):
    """
    Swan PC0034 — פריים 18 בייט (ללא CR מוביל ו-LF סוגר):
    [S1] XX.XXX [G/N] RS [S2] XX.XXX RS [CS]
    מחזיר (weight|None, status_str, special|None)
    קיים לשימוש עתידי — אף thread נוכחי לא קורא לו; כל הפרוטוקול הנוכחי הוא טקסטואלי.
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
