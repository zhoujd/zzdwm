#!/bin/bash

setup_common() {
    echo "Setup ssh keys ..."
    local sshkeys=/mnt/sshkeys
    if [ -d $sshkeys ]; then
        mkdir -p ~/.ssh
        cp -f $sshkeys/* ~/.ssh
        chmod 600 ~/.ssh/*
    fi
}

setup_server() {
    echo "Setup ssh server ..."
    $HOME/zzdwm/script/ssh-server.sh
}

setup_sleep() {
    echo "Setup sleep ..."
    sleep infinity
}

setup_help() {
    echo "Usage: $0 {init|run|help}"
}

CMD=${1:-"run"}
case "$CMD" in
    "init" )
        setup_common
        setup_sleep
        ;;
    "run" )
        setup_common
        setup_server
        setup_sleep
        ;;
    "help" )
        setup_help
        ;;
    * )
        setup_common
        exec "$@"
        ;;
esac
