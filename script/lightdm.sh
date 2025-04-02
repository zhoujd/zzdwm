#!/bin/bash

echo "Install lighdm"
sudo apt install -y lightdm

echo "Remove default xsession"
sudo rm -fv /usr/share/xsessions/lightdm-xsession.desktop

echo "Update gtk greeter"
sudo tee /etc/lightdm/lightdm-gtk-greeter.conf <<EOF
[greeter]
theme-name = Adwaita-dark
icon-theme-name = Adwaita
clock-format = %a %d %b %I:%M %p
indicators = ~host;~spacer;~session;~clock;~power
EOF

echo "Enable default users"
sudo sed -i 's/#greeter-hide-users=false/greeter-hide-users=false/g' /etc/lightdm/lightdm.conf
grep -n greeter-hide-users=false /etc/lightdm/lightdm.conf

echo "Restart lightdm"
sudo systemctl enable lightdm
sudo systemctl restart lightdm

echo "Setup lightdm done"
