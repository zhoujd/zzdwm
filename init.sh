#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)

install_dep() {
    sudo apt update
    sudo apt install -y \
         libx11-dev libxft-dev libxinerama-dev \
         libasound2-dev \
         libxfixes-dev libxi-dev libxext-dev \
         libxrandr-dev \
         libwebkit2gtk-4.0-dev libgcr-3-dev
    echo "Install dep done"
}

install_dm() {
    sudo cp -fv $SCRIPT_ROOT/bin/startdwm /usr/local/bin
    sudo tee /usr/share/xsessions/dwm.desktop <<EOF
[Desktop Entry]
Name=DWM Session
Comment=Dynamic window manager
Exec=startdwm
Icon=dwm
Type=Application
DesktopNames=DWM
EOF
    echo "Install dm done"
}

install_bin() {
    target=/usr/local/bin
    sudo cp -fv $SCRIPT_ROOT/bin/dupterm $target
    sudo cp -fv $SCRIPT_ROOT/bin/mg $target
    echo "Install bin done"
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
        $SCRIPT_ROOT/tools/surf
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
    install_bin
    build
    echo "Install all done"
}

usage() {
    app=$(basename $0)
    cat <<EOF
Usage: $app {dep|dm|bin|build|-b|all|-a}
dep          --    Install build dependence
dm           --    Install xsession entry
bin          --    Install bin
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
    bin )
        install_bin
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
