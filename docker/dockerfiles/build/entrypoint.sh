#!/bin/sh

# Ensure a writeable HOME directory for arbitrary UIDs
setup_home() {
    # If HOME is unset, empty, or points to root '/' which isn't writeable
    if [ -z "$HOME" ] || [ "$HOME" = "/" ] || [ ! -w "$HOME" ]; then
        export HOME="/tmp"
    fi
}

setup_common() {
    setup_home
    local sshkeys="/mnt/sshkeys"
    local target_ssh="$HOME/.ssh"
    if [ -d "$sshkeys" ] && [ "$(ls -A "$sshkeys" 2>/dev/null)" ]; then
        echo "Setup ssh keys for UID $(id -u) in $target_ssh ..."
        mkdir -p "$target_ssh"
        chmod 700 "$target_ssh"
        # Copy keys safely
        cp -f "$sshkeys"/* "$target_ssh/" 2>/dev/null || true
        chmod 600 "$target_ssh"/* 2>/dev/null || true
    fi
}

setup_server() {
    local sshcmd="startssh"
    if command -v "$sshcmd" >/dev/null 2>&1; then
        echo "Setup ssh server (UID: $(id -u))..."
        "$sshcmd" &
    fi
}

setup_sleep() {
    echo "Setup sleep ..."
    # 'exec' replaces PID 1 so 'docker stop' terminates instantly
    exec sleep infinity
}

setup_help() {
    echo "Usage: $0 {init|run|help}"
}

CMD="${1:-run}"
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
