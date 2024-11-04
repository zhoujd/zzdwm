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

install_tool() {
    local target=/usr/local/bin
    sudo mkdir -p $target
    sudo cp -vf $SCRIPT_ROOT/tools/script/dmenu_run $target
    sudo cp -vf $SCRIPT_ROOT/tools/script/dmenu_ssh $target
    sudo cp -vf $SCRIPT_ROOT/tools/script/dmenu_win $target
    echo "Install tool done"
}

usage() {
    app=$(basename $0)
    cat <<EOF
$app {dep|dm|tool|all}
dep                  --    install build dependence
dm                   --    install xsession entry
tool                 --    install tools
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
    tool )
        install_tool
        ;;
    all )
        install_dep
        install_dm
        install_tool
        ;;
    * )
        usage
        ;;
esac
