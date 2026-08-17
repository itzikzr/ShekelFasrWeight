#!/bin/sh
# Offline-target installer — no git, no code fetching.
#
# How to use: copy this entire project folder (the one this script lives in)
# to the target machine any way you like (USB drive, scp, network share — NOT
# git). Then run this script from inside that copied folder. It needs internet
# ONLY for this one run (to fetch python3-tk / pyserial / openpyxl); after that
# the app itself never needs a network connection.

set -e
cd "$(dirname "$0")"

echo "== Installing system packages (python3, tkinter) =="
if command -v apt >/dev/null 2>&1; then
    sudo apt update
    sudo apt install -y python3 python3-pip python3-tk
elif command -v dnf >/dev/null 2>&1; then
    sudo dnf install -y python3 python3-pip python3-tkinter
else
    echo "Unrecognized package manager — install python3, python3-pip, and a tkinter package manually, then re-run this script." >&2
    exit 1
fi

echo "== Adding $USER to the dialout group (serial port access) =="
sudo usermod -aG dialout "$USER"

echo "== Excluding the Swan FTDI adapter from ModemManager =="
# ModemManager probes every new USB-serial device to check if it's a cellular
# modem (sends AT commands over the port), which can race with our own
# request/response commands (SettingsWindow's "תצורת משקל" tab) on the same
# port. A real, worth-having defensive fix — though on the NUC8CCHK unit this
# specific rule turned out NOT to be the cause of an empty-response bug there
# (that was traced to the scale's firmware not implementing the SC command
# set at all; see CLAUDE.md). Targets only this exact FTDI chip (0403:6015);
# no other device is affected.
if command -v udevadm >/dev/null 2>&1 && [ -d /etc/udev/rules.d ]; then
    sudo cp udev-rules/99-swan-ftdi-ignore-modemmanager.rules /etc/udev/rules.d/
    sudo udevadm control --reload-rules
    sudo udevadm trigger
fi

echo "== Installing Python dependencies =="
python3 -c "import serial" 2>/dev/null || pip3 install --break-system-packages pyserial 2>/dev/null || pip3 install pyserial
python3 -c "import openpyxl" 2>/dev/null || pip3 install --break-system-packages openpyxl 2>/dev/null || pip3 install openpyxl
python3 -c "import bidi" 2>/dev/null || pip3 install --break-system-packages python-bidi 2>/dev/null || pip3 install python-bidi

APP_DIR="$(pwd)"

echo "== Creating desktop shortcut =="
DESKTOP_FILE_CONTENT="[Desktop Entry]
Type=Application
Name=תחנת שקילה
Comment=Scale Sampler — Swan weighing station
Exec=$APP_DIR/run.sh
Icon=$APP_DIR/scale_app/assets/icon.png
Terminal=false
Categories=Utility;"

mkdir -p "$HOME/.local/share/applications"
echo "$DESKTOP_FILE_CONTENT" > "$HOME/.local/share/applications/scale-sampler.desktop"
chmod +x "$HOME/.local/share/applications/scale-sampler.desktop"

if [ -d "$HOME/Desktop" ]; then
    echo "$DESKTOP_FILE_CONTENT" > "$HOME/Desktop/scale-sampler.desktop"
    chmod +x "$HOME/Desktop/scale-sampler.desktop"
    command -v gio >/dev/null 2>&1 && gio set "$HOME/Desktop/scale-sampler.desktop" metadata::trusted true 2>/dev/null
fi

echo
echo "Done."
echo "You can disconnect this machine from the network now — everything from"
echo "here on runs fully offline."
echo "Log out and back in (or reboot) so the dialout group membership takes effect."
echo "Then run the app with: ./run.sh (or the new desktop/menu shortcut)."
