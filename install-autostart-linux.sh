#!/bin/sh
# Registers the app to launch automatically at login (XDG autostart —
# GNOME/most desktop environments honor ~/.config/autostart/*.desktop).
# Separate from install-linux.sh so autostart is opt-in, not forced on every
# install/update. Run uninstall-autostart-linux.sh to remove it again.

set -e
cd "$(dirname "$0")"
APP_DIR="$(pwd)"

mkdir -p "$HOME/.config/autostart"
cat > "$HOME/.config/autostart/scale-sampler.desktop" << EOF
[Desktop Entry]
Type=Application
Name=תחנת שקילה
Comment=Scale Sampler — Swan weighing station
Exec=$APP_DIR/run.sh
Icon=$APP_DIR/scale_app/assets/icon.png
Terminal=false
X-GNOME-Autostart-enabled=true
EOF
chmod +x "$HOME/.config/autostart/scale-sampler.desktop"

echo "Done. The app will now launch automatically at login."
echo "To undo this, run uninstall-autostart-linux.sh."
