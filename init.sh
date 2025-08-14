#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
CORE_ROOT=$(cd $SCRIPT_ROOT && pwd)
CORE_TOP=$(cd $CORE_ROOT/.. && pwd)

install_dep() {
    echo "Install prepare"
    sudo apt update
    echo "Install dev package"
    sudo apt install -y \
         make gcc g++ yacc \
         libx11-dev libxft-dev libxinerama-dev libxrender-dev \
         libasound2-dev \
         libxfixes-dev libxi-dev libxext-dev \
         libxrandr-dev \
         libxcomposite-dev libxdamage-dev \
         libx11-xcb-dev libxcb-randr0-dev libxcb-xinerama0-dev
    echo "Install tool package"
    sudo apt install -y \
         alsa-utils x11-utils wmctrl psmisc
    echo "Install dep done"
}

install_dm() {
    local target=/usr/share/xsessions
    local dm=$CORE_ROOT/misc/dm
    echo "Init $target"
    sudo mkdir -p $target
    sudo cp -fv $dm/dwm.desktop $target
    sudo cp -fv $dm/cwm.desktop $target
    echo "Install dm done"
}

install_bin() {
    local target=/usr/local/bin
    local apps=(
        $CORE_ROOT/bin/nextwin
        $CORE_ROOT/bin/prevwin
        $CORE_ROOT/bin/dupterm
        $CORE_ROOT/bin/me
        $CORE_ROOT/bin/mg
        $CORE_ROOT/bin/movwin
        $CORE_ROOT/bin/focuswin
        $CORE_ROOT/bin/runec
        $CORE_ROOT/bin/startdwm
        $CORE_ROOT/bin/startcwm
        $CORE_ROOT/bin/dwm-session
        $CORE_ROOT/bin/cwm-session
    )
    for app in ${apps[@]}; do
        sudo cp -fv $app $target
    done
    echo "Install bin done"
}

install_misc() {
    echo "Install cwmrc"
    ln -sfTv $CORE_ROOT/misc/cwm/cwmrc ~/.cwmrc
    echo "Install misc done"
}

build() {
    local projects=(
        $CORE_ROOT/src/dwm
        $CORE_ROOT/src/cwm
        $CORE_ROOT/tools/dmenu
        $CORE_ROOT/tools/dwmstatus
        $CORE_ROOT/tools/xbanish
        $CORE_ROOT/tools/slock
        $CORE_ROOT/tools/shotkey
        $CORE_ROOT/tools/xcompmgr
        $CORE_ROOT/tools/utils
        $CORE_ROOT/tools/lemonbar
        $CORE_ROOT/tools/detach
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

clean() {
    git clean -dfx
    echo "Clean done"
}

all() {
    install_dep
    install_dm
    install_bin
    install_misc
    build
    clean
    echo "Install all done"
}

usage() {
    app=$(basename $0)
    cat <<EOF
Usage: $app {dep|dm|bin|misc|build|-b|clean|-c|all|-a}
dep          --    Install depend package
dm           --    Install xsession entry
bin          --    Install bin
misc         --    Install misc
build|-b     --    Build all
clean|-c     --    Clean all
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
        clean
        ;;
    clean|-c )
        clean
        ;;
    all|-a )
        all
        ;;
    * )
        usage
        ;;
esac
