#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
CORE_ROOT=$(cd $SCRIPT_ROOT && pwd)
CORE_TOP=$(cd $CORE_ROOT/.. && pwd)

install_dep() {
    sudo apt update
    sudo apt install -y \
         libx11-dev libxft-dev libxinerama-dev libxrender-dev \
         libasound2-dev \
         libxfixes-dev libxi-dev libxext-dev \
         libxrandr-dev \
         libxcomposite-dev libxdamage-dev
    echo "Install dep done"
}

install_dm() {
    sudo cp -fv $CORE_ROOT/bin/startdwm /usr/local/bin
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
    sudo cp -fv $CORE_ROOT/bin/nextwin $target
    sudo cp -fv $CORE_ROOT/bin/prevwin $target
    sudo cp -fv $CORE_ROOT/bin/dupterm $target
    sudo cp -fv $CORE_ROOT/bin/me $target
    sudo cp -fv $CORE_ROOT/bin/mg $target
    echo "Install bin done"
}

install_misc() {
    target=~/.config
    mkdir -p ~/.config
    rm -rf ~/.config/jasspa
    cp -rv $CORE_ROOT/misc/.config/jasspa $target
    echo "Install misc done"
}

build() {
    local projects=(
        $CORE_ROOT/src
        $CORE_ROOT/tools/dmenu
        $CORE_ROOT/tools/dwmstatus
        $CORE_ROOT/tools/xbanish
        $CORE_ROOT/tools/slock
        $CORE_ROOT/tools/shotkey
        $CORE_ROOT/tools/xcompmgr
        $CORE_ROOT/tools/utils
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
    install_misc
    build
    echo "Install all done"
}

usage() {
    app=$(basename $0)
    cat <<EOF
Usage: $app {dep|dm|bin|misc|build|-b|all|-a}
dep          --    Install build dependence
dm           --    Install xsession entry
bin          --    Install bin
misc         --    Install misc configure
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
    misc )
        install_misc
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
