#!/bin/sh

setup_common() {
    # If HOME is unset, empty, or points to root '/' which isn't writeable
    if [ -z "$HOME" ] || [ "$HOME" = "/" ] || [ ! -w "$HOME" ]; then
        export HOME="/tmp"
    fi

    local sshkeys="/mnt/sshkeys"
    local target_ssh="$HOME/.ssh"

    if [ -d "$sshkeys" ] && [ "$(ls -A "$sshkeys" 2>/dev/null)" ]; then
        echo "Setup ssh keys for UID $(id -u) in $target_ssh ..."
        mkdir -p "$target_ssh"
        chmod 700 "$target_ssh"

        # Copy SSH keys
        cp -f "$sshkeys"/* "$target_ssh/" 2>/dev/null || true
        chmod 600 "$target_ssh"/* 2>/dev/null || true

        # Ensure directory permissions remain 700 after copy
        chmod 700 "$target_ssh"
    fi
}

setup_services() {
    local sshcmd="startssh"
    if command -v "$sshcmd" >/dev/null 2>&1; then
        echo "Setup SSH service ..."
        "$sshcmd" &
    fi
}

setup_sleep() {
    echo "Setup sleep ..."
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
        setup_services
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
