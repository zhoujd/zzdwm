#!/bin/bash

# A simple logout dialog

menu=(
    "0: Cancel"
    "1: Logout"
    "2: Shutdown"
    "3: Reboot"
    "4: Lock"
)
prompt="exit:"
choice=`printf '%s\n' "${menu[@]}" | dmenu -i -l 10 -p $prompt | cut -d ':' -f 1`
case "$choice" in
    0) exit ;;
    1) xdotool key super+shift+q & ;;
    2) systemctl poweroff & ;;
    3) systemctl shutdown & ;;
    4) slock & ;;
esac