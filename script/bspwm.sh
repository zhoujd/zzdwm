#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
CORE_ROOT=$(cd $SCRIPT_ROOT/.. && pwd)
CORE_TOP=$(cd $CORE_ROOT/.. && pwd)

install() {
    echo "Install bspwm config files"
    local config=$CORE_ROOT/misc/config
    ln -sfTv $config/bspwm ~/.config/bspwm
    ln -sfTv $config/sxhkd ~/.config/sxhkd

    echo "Init xsession"
    local target=/usr/share/xsessions
    local dm=$CORE_ROOT/misc/dm
    sudo mkdir -p $target
    sudo cp -fv $dm/bspwm.desktop $target
    echo "Init dm"
    local bin=/usr/local/bin
    local libexec=$CORE_ROOT/libexec
    sudo cp -fv $libexec/dm/bspwm-session $bin

    echo "Install done"
}

uninstall() {
    rm -rfv ~/.config/bspwm
    rm -rfv ~/.config/sxhkd

    local target=/usr/share/xsessions
    sudo rm -fv $target/bspwm.desktop

    local bin=/usr/local/bin
    sudo rm -fv $bin/bspwm-session

    echo "Uninstall done"
}

case $1 in
    install|-i )
        install
        ;;
    uninstall|-u )
        uninstall
        ;;
    * )
        echo "usage: $(basename $0) {install|-i|uninstall|-u}"
        ;;
esac
