#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)

install_dep() {
    sudo apt install -y libx11-dev libxft-dev libxinerama-dev
    echo "Install dep done"
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
$app {dep|dm|all}
dep                  --    install build dependence
dm                   --    install xsession entry
all                  --    install all
EOF
}

case $1 in
    dep )
        install_dep
        ;;
    dm )
        install_dm
        ;;
    all )
        install_dep
        install_dm
        ;;
    * )
        usage
        ;;
esac
