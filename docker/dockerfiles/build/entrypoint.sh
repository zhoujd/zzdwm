#!/bin/bash

setup_common() {
    local sshkeys=/mnt/sshkeys
    if [ -d $sshkeys ]; then
        echo "Setup ssh keys ..."
        mkdir -p ~/.ssh
        cp -f $sshkeys/* ~/.ssh
        chmod 600 ~/.ssh/*
    fi
}

setup_server() {
    local sshcmd=startssh
    if command -v $sshcmd 2>/dev/null; then
        echo "Setup ssh server ..."
        $sshcmd &
    fi
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
