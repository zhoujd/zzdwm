#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)

install_dm() {
    sudo tee /usr/share/xsessions/dwm.desktop <<EOF
[Desktop Entry]
Name=DWM Session
Comment=DWM window manager
Exec=dwm
Icon=
Type=Application
DesktopNames=DWM
EOF
    echo "Install dm done"
}

usage() {
    app=$(basename $0)
    cat <<EOF
$app {dm|all}
dm                   --    install xsession entry
all                  --    install all
EOF
}

case $1 in
    dm )
        install_dm
        ;;
    all )
        install_dm
        ;;
    * )
        usage
        ;;
esac
