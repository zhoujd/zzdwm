#!/bin/bash

dep() {
    echo "Install package"
    sudo apt install -y lightdm
    sudo apt install -y lightdm-gtk-greeter
    echo "Enable service"
    sudo systemctl enable lightdm
    echo "Remove default xsession"
    sudo rm -fv /usr/share/xsessions/lightdm-xsession.desktop
    echo "Install dep done"
}

config() {
    echo "Update gtk greeter"
    sudo tee /etc/lightdm/lightdm-gtk-greeter.conf <<EOF
[greeter]
theme-name = Adwaita-dark
icon-theme-name = Adwaita
font-name = SF Pro Display 10
clock-format = %a %e %b %l:%M %p
indicators = ~host;~spacer;~session;~clock;~power
EOF
    echo "Enable default users"
    sudo sed -i 's/#greeter-hide-users=false/greeter-hide-users=false/g' /etc/lightdm/lightdm.conf
    grep -n greeter-hide-users=false /etc/lightdm/lightdm.conf
    echo "Config lightdm done"
}

restart() {
    echo "Remove $HOME/.Xauthority"
    rm -fv $HOME/.Xauthority
    echo "Restart lightdm service"
    sudo systemctl restart lightdm
    echo "Restart lightdm done"
}

usage() {
    cat <<EOF
Usage: $(basename $0) {dep|-d|config|-c|restart|-r|all|-a}"
EOF
}

case $1 in
    dep|-d )
        dep
        ;;
    config|-c )
        config
        ;;
    restart|-r )
        restart
        ;;
    all|-a )
        dep
        config
        restart
        ;;
    * )
        usage
        ;;
esac
