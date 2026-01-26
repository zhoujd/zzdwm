#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
CORE_ROOT=$(cd $SCRIPT_ROOT && pwd)
CORE_TOP=$(cd $CORE_ROOT/.. && pwd)

source /etc/os-release

install_dep() {
    case $ID in
        ubuntu|debian )
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
                 alsa-utils x11-utils wmctrl xdotool psmisc
            ;;
        void )
            echo "Install prepare"
            sudo xbps-install -Syu xbps
            echo "Install dev package"
            sudo xbps-install -Sy \
                 make gcc byacc \
                 libX11-devel libuuid libXft-devel libXinerama-devel \
                 gd-devel pkg-config \
                 alsa-lib-devel \
                 libXrandr-devel byacc \
                 libXfixes-devel libXi-devel \
                 libXcomposite-devel libXdamage-devel
            echo "Install tool package"
            sudo xbps-install -Sy \
                 bash-completion xorg wmctrl xdotool psmisc
            ;;
        * )
            echo "Distro $ID not supported"
            exit 1
            ;;
    esac
    echo "Install dep done"
}

install_dm() {
    echo "Init xsession"
    local target=/usr/share/xsessions
    local dm=$CORE_ROOT/misc/dm
    sudo mkdir -p $target
    sudo cp -fv $dm/{dwm,cwm,bspwm}.desktop $target
    echo "Init dm"
    local bin=/usr/local/bin
    local libexec=$CORE_ROOT/libexec
    local apps=(
        $libexec/dm/nextwin
        $libexec/dm/prevwin
        $libexec/dm/nextwincd
        $libexec/dm/prevwincd
        $libexec/dm/focuswin
        $libexec/dm/focuswincd
        $libexec/dm/dupterm
        $libexec/dm/movewin
        $libexec/dm/deckwin
        $libexec/dm/doubledeck
        $libexec/dm/monocle
        $libexec/dm/grid
        $libexec/dm/tile
        $libexec/dm/startdwm
        $libexec/dm/startcwm
        $libexec/dm/dwm-session
        $libexec/dm/cwm-session
        $libexec/dm/bspwm-session
    )
    for app in ${apps[@]}; do
        sudo cp -fv $app $bin
    done
    echo "Install dm done"
}

install_bin() {
    local bin=/usr/local/bin
    local apps=(
        $CORE_ROOT/bin/ec
        $CORE_ROOT/bin/me
        $CORE_ROOT/bin/vi
        $CORE_ROOT/bin/ff
        $CORE_ROOT/bin/etags
        $CORE_ROOT/bin/abduco
        $CORE_ROOT/bin/hexedit
    )
    for app in ${apps[@]}; do
        sudo cp -fv $app $bin
    done
    echo "Install bin done"
}

install_misc() {
    echo "Install cwmrc"
    local cwm=$CORE_ROOT/misc/cwm
    ln -sfTv $cwm/cwmrc ~/.cwmrc

    echo "Install bspwm config files"
    local bspwm=$CORE_ROOT/misc/bspwm
    ln -sfTv $bspwm/config/bspwm ~/.config/bspwm
    ln -sfTv $bspwm/config/sxhkd ~/.config/sxhkd

    echo "Install misc done"
}

install() {
    install_dm
    install_bin
    install_misc
    echo "Install all done"
}

build() {
    local projects=(
        $CORE_ROOT/src/dwm
        $CORE_ROOT/src/cwm
        $CORE_ROOT/tools/dmenu
        $CORE_ROOT/tools/dwmstatus
        $CORE_ROOT/tools/slock
        $CORE_ROOT/tools/utils
        $CORE_ROOT/tools/shotkey
        $CORE_ROOT/tools/xbanish
        $CORE_ROOT/tools/xcompmgr
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
    echo "Init all done"
}

usage() {
    app=$(basename $0)
    cat <<EOF
usage: $app {dep|-d|dm|bin|misc|install|-i|build|-b|clean|-c|all|-a}
dep|-d       --    Install depend package
dm           --    Install xsession
bin          --    Install bin
misc         --    Install misc
install|-i   --    Install all
build|-b     --    Build all
clean|-c     --    Clean all
all|-a       --    Init all
EOF
}

case $1 in
    dep|-d )
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
    install|-i )
        install
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
