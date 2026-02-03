#!/bin/bash

setup_ssh() {
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
        setup_sleep
        ;;
    "run" )
        setup_ssh
        setup_sleep
        ;;
    "help" )
        setup_help
        ;;
    * )
        exec "$@"
        ;;
esac
