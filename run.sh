#!/bin/sh
cd "$(dirname "$0")"
python3 -c "import serial" 2>/dev/null || pip3 install --break-system-packages pyserial 2>/dev/null || pip3 install pyserial
python3 -c "import openpyxl" 2>/dev/null || pip3 install --break-system-packages openpyxl 2>/dev/null || pip3 install openpyxl
python3 -c "import bidi" 2>/dev/null || pip3 install --break-system-packages python-bidi 2>/dev/null || pip3 install python-bidi
python3 -c "import tkinter" 2>/dev/null || {
    echo "tkinter is missing — install it via your distro's package manager,"
    echo "e.g. 'sudo apt install python3-tk' (Debian/Ubuntu) or 'sudo dnf install python3-tkinter' (Fedora)."
    exit 1
}
python3 scale_sampler.py
