#!/bin/bash

# A simple logout dialog
entries=(
    "0: Cancel"
    "1: Logout"
    "2: Shutdown"
    "3: Reboot"
    "4: Lock"
)
choice=$(printf '%s\n' "${entries[@]}" | \
             dmenu -i -l 10 -p "select an action:" | \
             cut -d ':' -f 1)
# execute the choice in background
case "$choice" in
    0) exit ;;
    1) xdotool key super+shift+q & ;;
    2) systemctl poweroff & ;;
    3) systemctl shutdown & ;;
    4) slock & ;;
esac
