#!/bin/sh
# One-time conversion: turns an offline (no-git, USB-copied) install into a
# real git clone, so Settings -> "עדכונים" (git fetch/pull) can actually
# work — it needs an actual .git folder with the GitHub remote configured,
# which install-linux.sh deliberately never sets up (see its own header
# comment: "Offline-target installer — no git, no code fetching").
#
# Needs git installed and a temporary internet connection for THIS RUN ONLY
# — same "temporary internet, then offline again" deal as install-linux.sh's
# own one-time package installs. The app itself still never needs a network
# connection except when you actually check for/apply an update afterward.
#
# Run this from INSIDE the existing (offline-copied) app folder. It clones a
# fresh copy into a *sibling* folder — never touches or deletes the original
# — carries over the two local-state files that are gitignored on purpose
# (scale_sampler_config.json, scale_data.db) plus any backups/ folder, and
# re-runs the desktop-shortcut/autostart scripts from the NEW folder so they
# point at it instead. From then on, run the app from the new folder, not
# this one — this script only prepares it, it doesn't switch anything for
# you automatically.

set -e
cd "$(dirname "$0")"
OLD_DIR="$(pwd)"
REPO_URL="https://github.com/itzikzr/ShekelFasrWeight.git"
NEW_DIR="$OLD_DIR-git"

if [ -d "$NEW_DIR" ]; then
    echo "כבר קיימת תיקייה בשם $NEW_DIR — כדי לא למחוק בטעות עבודה קיימת," >&2
    echo "הסקריפט הזה לא ידרוס אותה. מחק/העבר אותה בעצמך ואז הרץ שוב." >&2
    exit 1
fi

if ! command -v git >/dev/null 2>&1; then
    echo "git לא מותקן. הרץ קודם: sudo apt install git (או dnf/pacman המקביל בהתאם להפצה), ואז הרץ את הסקריפט הזה שוב." >&2
    exit 1
fi

echo "== משכפל את המאגר מ-GitHub אל $NEW_DIR =="
git clone "$REPO_URL" "$NEW_DIR"

echo "== מעביר קבצי מצב מקומיים (הגדרות / DB / גיבויים) לתיקייה החדשה =="
[ -f "$OLD_DIR/scale_sampler_config.json" ] && cp "$OLD_DIR/scale_sampler_config.json" "$NEW_DIR/"
[ -f "$OLD_DIR/scale_data.db" ] && cp "$OLD_DIR/scale_data.db" "$NEW_DIR/"
[ -d "$OLD_DIR/backups" ] && cp -r "$OLD_DIR/backups" "$NEW_DIR/"

echo "== מריץ מחדש את install-linux.sh מהתיקייה החדשה (מעדכן קיצור שולחן העבודה לנתיב הנכון) =="
( cd "$NEW_DIR" && sh install-linux.sh )

if [ -f "$HOME/.config/autostart/scale-sampler.desktop" ]; then
    echo "== הפעלה אוטומטית הייתה מוגדרת — מעדכן גם אותה לנתיב החדש =="
    ( cd "$NEW_DIR" && sh install-autostart-linux.sh )
fi

echo
echo "הושלם. מהפעם הבאה הרץ את התוכנה מהתיקייה החדשה: $NEW_DIR"
echo "(קיצורי הדרך/ההפעלה האוטומטית כבר מצביעים לשם.)"
echo "אפשר למחוק את התיקייה הישנה ($OLD_DIR) בעצמך, רק לאחר שבדקת שהחדשה פועלת כרגיל."
echo "בדיקת/הורדת עדכון (הגדרות -> עדכונים) תדרוש חיבור אינטרנט זמני בזמן הבדיקה עצמה, לא באופן קבוע."
