#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)

install_dep() {
    sudo apt install -y libx11-dev libxft-dev libxinerama-dev
    sudo apt install -y libasound2-dev
    sudo apt install -y libxfixes-dev libxi-dev libxext-dev
    sudo apt install -y libxrandr-dev
    echo "Install dep done"
}

install_dm() {
    sudo cp -fv $SCRIPT_ROOT/bin/startdwm /usr/local/bin
    sudo tee /usr/share/xsessions/dwm.desktop <<EOF
[Desktop Entry]
Name=DWM Session
Comment=DWM window manager
Exec=startdwm
Icon=
Type=Application
DesktopNames=DWM
EOF
    echo "Install dm done"
}

install_script() {
    sudo cp -fv $SCRIPT_ROOT/script/dupterm /usr/local/bin
    echo "Install script done"
}

build() {
    local projects=(
        $SCRIPT_ROOT/src
        $SCRIPT_ROOT/tools/dmenu
        $SCRIPT_ROOT/tools/dwmstatus
        $SCRIPT_ROOT/tools/tabbed
        $SCRIPT_ROOT/tools/xbanish
        $SCRIPT_ROOT/tools/slock
        $SCRIPT_ROOT/tools/shotkey
    )
    for proj in ${projects[@]}; do
        echo "Build $proj"
        make -C $proj clean
        make -C $proj
        sudo make -C $proj install
        echo "Build $proj done"
    done
    echo "Build done"
}

all() {
    install_dep
    install_dm
    install_script
    build
    echo "Install all done"
}

usage() {
    app=$(basename $0)
    cat <<EOF
Usage: $app {dep|dm|script|build|-b|all|-a}
dep          --    Install build dependence
dm           --    Install xsession entry
script       --    Install script
build|-b     --    Build all
all|-a       --    Install all
EOF
}

case $1 in
    dep )
        install_dep
        ;;
    dm )
        install_dm
        ;;
    script )
        install_script
        ;;
    build|-b )
        build
        ;;
    all|-a )
        all
        ;;
    * )
        usage
        ;;
esac
