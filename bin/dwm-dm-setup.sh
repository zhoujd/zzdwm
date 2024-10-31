#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)

## Please run as root
if [ $EUID -ne 0 ]; then
    echo "You must be a root user" 2>&1
    exit 1
fi

echo "Setup stumpwm desktop"
tee /usr/share/xsessions/dwm.desktop <<EOF
[Desktop Entry]
Name=DWM
Comment=DWM window manager
Exec=dwm
Icon=
Type=Application
DesktopNames=DWM
EOF

echo "Setup dwm dm done"
