#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)

install_autostart() {
    ln -sfTv $SCRIPT_ROOT/misc/.dwm ~/.dwm
    echo "Install autostart done"
}

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
$app {autostart|-as|dm}
autostart|-as        --    install autostart script
dm                   --    install xsession entry
EOF
}

case $1 in
    autostart|-as )
        install_autostart
        ;;
    dm )
        install_dm
        ;;
    * )
        usage
        ;;
esac
