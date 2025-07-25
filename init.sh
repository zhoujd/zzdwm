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
    local target=/usr/local/bin
    local xs=/usr/share/xsessions
    echo "Init $xs"
    sudo mkdir -p $xs
    echo "Install dwm"
    sudo cp -fv $CORE_ROOT/bin/dwm-session $target/dwm-session
    local dwm_entry=$xs/dwm.desktop
    echo "Install $dwm_entry"
    sudo tee $dwm_entry <<EOF
[Desktop Entry]
Name=DWM Session
Comment=Dynamic Window Manager
Exec=dwm-session
Icon=dwm
Type=Application
DesktopNames=DWM
EOF
    echo "Install cwm"
    sudo cp -fv $CORE_ROOT/bin/cwm-session $target/cwm-session
    local cwm_entry=$xs/cwm.desktop
    echo "Install $cwm_entry"
    sudo tee $cwm_entry <<EOF
[Desktop Entry]
Name=CWM Session
Comment=OpenBSD's CWM
Exec=cwm-session
Icon=cwm
Type=Application
DesktopNames=CWM
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
        $CORE_ROOT/bin/movwin
        $CORE_ROOT/bin/cwmbar
        $CORE_ROOT/bin/focuswin
        $CORE_ROOT/bin/runec
        $CORE_ROOT/bin/startdwm
        $CORE_ROOT/bin/startcwm        
    )
    for app in ${apps[@]}; do
        sudo cp -fv $app $target
    done
    echo "Install bin done"
}

install_misc() {
    ln -sfTv $CORE_ROOT/misc/.cwmrc ~/.cwmrc
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
