#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
CORE_ROOT=$(cd $SCRIPT_ROOT && pwd)
CORE_TOP=$(cd $CORE_ROOT/.. && pwd)

install_dep() {
    echo "Install prepare"
    sudo apt update
    echo "Install dev package"
    sudo apt install -y \
         libx11-dev libxft-dev libxinerama-dev libxrender-dev \
         libasound2-dev \
         libxfixes-dev libxi-dev libxext-dev \
         libxrandr-dev \
         libxcomposite-dev libxdamage-dev
    echo "Install tool package"
    sudo apt install -y \
         alsa-utils x11-utils wmctrl psmisc
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
    local target=/usr/local/bin
    local apps=(
        $CORE_ROOT/bin/nextwin
        $CORE_ROOT/bin/prevwin
        $CORE_ROOT/bin/dupterm
        $CORE_ROOT/bin/hexedit
        $CORE_ROOT/bin/me
        $CORE_ROOT/bin/mg
        $CORE_ROOT/bin/mtm
        $CORE_ROOT/bin/abduco
        $CORE_ROOT/bin/dtach
        $CORE_ROOT/bin/dvtm
        $CORE_ROOT/bin/dvtm-editor
        $CORE_ROOT/bin/dvtm-pager
        $CORE_ROOT/bin/dvtm-status
        $CORE_ROOT/bin/nq
        $CORE_ROOT/bin/nqtail
        $CORE_ROOT/bin/rsync-avzh
    )
    for app in ${apps[@]}; do
        sudo cp -fv $app $target
    done
    echo "Install bin done"
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
    build
    echo "Install all done"
}

usage() {
    app=$(basename $0)
    cat <<EOF
Usage: $app {dep|dm|bin|build|-b|all|-a}
dep          --    Install depend package
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
