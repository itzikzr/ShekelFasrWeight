#!/bin/sh
# Removes the autostart entry created by install-autostart-linux.sh.
FILE="$HOME/.config/autostart/scale-sampler.desktop"

if [ -f "$FILE" ]; then
    rm "$FILE"
    echo "Removed: $FILE"
    echo "The app will no longer launch automatically at login."
else
    echo "No auto-start entry found — nothing to remove."
fi
