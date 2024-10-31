#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)

install_autostart() {
    ln -sfTv $SCRIPT_ROOT/misc/.dwm ~/.dwm
    echo "Install autostart done"
}

install_autostart
